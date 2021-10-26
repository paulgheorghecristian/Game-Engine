#include "SkeletalBone.hpp"
#include "AnimationLoader.hpp"

#include <iostream>

SkeletalBone::SkeletalBone(const std::string &name, int id): m_name(name), m_id(id) {
     m_parentBone = NULL;
     m_mesh = NULL;
     m_node = NULL;
}

SkeletalBone::SkeletalBone(Mesh *mesh, int id, const std::string &name, const aiMatrix4x4 &offsetMat) {
     m_id = id;
     m_name = name;
     m_offsetMatrix = AnimationLoader::AiToGLMMat4(offsetMat);

     m_mesh = mesh;

     m_parentBone = NULL;
     m_node = NULL;
}

SkeletalBone::SkeletalBone(Mesh *mesh, int id, const std::string &name, const glm::mat4 &offsetMat) {
     m_id = id;
     m_name = name;
     m_offsetMatrix = offsetMat;
 
     m_mesh = mesh;

     m_parentBone = NULL;
     m_node = NULL;
}

glm::mat4 SkeletalBone::getParentTransforms() {
     SkeletalBone *b = m_parentBone;
     std::vector<glm::mat4> mats;

     while (b != NULL) {
          glm::mat4 tmp_mat = AnimationLoader::AiToGLMMat4(b->m_node->mTransformation);
          mats.push_back(tmp_mat);

          b = b->m_parentBone;
     }

     glm::mat4 concatenated_transforms;

     for (int i = mats.size()-1; i >= 0; i--)
         concatenated_transforms *= mats[i];

     return concatenated_transforms;
}

unsigned int SkeletalBone::findPosition(float time) {
     if (m_animNode->mNumPositionKeys == 0) {
          return 0;
     }

     for (unsigned int i = 0; i < m_animNode->mNumPositionKeys - 1; i++) {
          if (time < (float) m_animNode->mPositionKeys[i + 1].mTime)
               return i;
     }
     return 0;
}

unsigned int SkeletalBone::findRotation(float time) {
     if (m_animNode->mNumRotationKeys == 0) {
          return 0;
     }

     for (unsigned int i = 0; i < m_animNode->mNumRotationKeys - 1; i++) {
          if (time < (float) m_animNode->mRotationKeys[i + 1].mTime)
               return i;
     }
     return 0;
}

glm::vec3 SkeletalBone::calcInterpolatedPosition(float time) {
     //If there's only one keyframe for the position, we return it.
     //This avoids any possible out of range errors later on in the function
     //as we're dealing with an array.
     if (m_animNode->mNumPositionKeys == 1) {
         aiVector3D assimp_val = m_animNode->mPositionKeys[0].mValue;
         glm::vec3 val(assimp_val.x,assimp_val.y,assimp_val.z);
         return val;
     }

     //The index of our current position, as well as the index that follows.
     //This will allow us to interpolate between the two values.
     unsigned int PositionIndex = findPosition(time);
     unsigned int NextPositionIndex = (PositionIndex + 1);

     //DeltaTime is the amount of time between the two keyframes.
     float DeltaTime = m_animNode->mPositionKeys[NextPositionIndex].mTime - m_animNode->mPositionKeys[PositionIndex].mTime;
     //The factor is simply how much time has passed since the last keyframe,
     //divided by the amount of time between the two keyframes (the DeltaTime)
     //to get the percentage, or how far along between the two keyframes we are.
     float Factor = (time - (float) m_animNode->mPositionKeys[PositionIndex].mTime) / DeltaTime;
 
     //The start and end positions (the position values of each of the keyframes)
     const aiVector3D StartPosition = m_animNode->mPositionKeys[PositionIndex].mValue;
     const aiVector3D EndPosition = m_animNode->mPositionKeys[NextPositionIndex].mValue;
 
     //Here we convert them to glm matrices...
     glm::vec3 p1(StartPosition.x,StartPosition.y,StartPosition.z);
     glm::vec3 p2(EndPosition.x,EndPosition.y,EndPosition.z);

     //and here we linearly interpolate between the two keyframes.
     glm::vec3 val = glm::mix(p1,p2,Factor);

     return val;
}

glm::quat SkeletalBone::calcInterpolatedRotation(float time) {
     if (m_animNode->mNumRotationKeys == 1) {
         aiQuaternion assimp_val = m_animNode->mRotationKeys[0].mValue;
         glm::quat val(assimp_val.w,assimp_val.x,assimp_val.y,assimp_val.z);
         return val;
     }

     unsigned int RotationIndex = findRotation(time);
     unsigned int NextRotationIndex = (RotationIndex + 1);

     float DeltaTime = m_animNode->mRotationKeys[NextRotationIndex].mTime - m_animNode->mRotationKeys[RotationIndex].mTime;
     float Factor = (time - (float) m_animNode->mRotationKeys[RotationIndex].mTime) / DeltaTime;

     const aiQuaternion& StartRotationQ = m_animNode->mRotationKeys[RotationIndex].mValue;
     const aiQuaternion& EndRotationQ = m_animNode->mRotationKeys[NextRotationIndex].mValue;
 
     glm::quat r1(StartRotationQ.w,StartRotationQ.x,StartRotationQ.y,StartRotationQ.z);
     glm::quat r2(EndRotationQ.w,EndRotationQ.x,EndRotationQ.y,EndRotationQ.z);

     glm::quat val = glm::slerp(r1,r2,Factor);
     return val;
}

void SkeletalBone::updateKeyframeTransform(float time) {
     if (m_animNode == NULL)
         return;

     pos = calcInterpolatedPosition(time);
     rot = calcInterpolatedRotation(time);
     scale = glm::vec3(1.0);

     glm::mat4 mat(1.0f);

     mat = glm::translate(mat, pos);
     glm::mat4 R = glm::mat4_cast(rot);
     mat = glm::scale(mat, scale) * R;

     m_node->mTransformation = AnimationLoader::GLMMat4ToAi(mat);
}