#ifndef __SKELETAL_BONE_HPP__
#define __SKELETAL_BONE_HPP__

#include "SkeletalAnimation.hpp"
#include "Mesh.h"

#include "assimp/matrix4x4.h"
#include "assimp/scene.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Skeleton;
class RenderingObject;

class SkeletalBone
{
    public:        
        SkeletalBone(const std::string &name = "", int id = -1);
        SkeletalBone(Mesh *mesh, int id, const std::string &name, const aiMatrix4x4 &offsetMat);
        SkeletalBone(Mesh *mesh, int id, const std::string &name, const glm::mat4 &offsetMat);

        void updateKeyframeTransform(float time);
        glm::mat4 getParentTransforms();

        unsigned int findPosition(float time);
        glm::vec3 calcInterpolatedPosition(float time);
        unsigned int findRotation(float time);
        glm::quat calcInterpolatedRotation(float time);

    private:
        friend class RenderingObject;
        friend class SkeletalAnimation;
        friend class AnimationLoader;
        friend class Skeleton;

        Mesh *m_mesh;
        aiNode *m_node;
        aiNodeAnim *m_animNode;

        SkeletalBone *m_parentBone;
        Skeleton *m_parentSkeleton;

        glm::mat4 m_parentTransforms;
        glm::mat4 m_offsetMatrix;

        int m_id;
        std::string m_name;

        glm::vec3 pos;
        glm::quat rot;
        glm::vec3 scale;
        glm::vec3 p1;
        glm::vec3 p2;
};


#endif /* __SKELETAL_BONE_HPP__ */