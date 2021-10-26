#include "AnimationLoader.hpp"

#include <iostream>

AnimationLoader *AnimationLoader::m_instance = NULL;

AnimationLoader::AnimationLoader(aiScene *scene): m_scene(scene) {
}

AnimationLoader::~AnimationLoader() {
    m_scene = NULL;
}

AnimationLoader *AnimationLoader::getInstance() {
    return m_instance;
}

void AnimationLoader::init(aiScene *scene) {
    if (m_instance == NULL) {
        m_instance = new AnimationLoader(scene);
    }
}

void AnimationLoader::destroy() {
    if (m_instance != NULL) {
        delete m_instance;
        m_instance = NULL;
    }
}

aiMatrix4x4 AnimationLoader::GLMMat4ToAi(const glm::mat4 &mat) {
     return aiMatrix4x4(mat[0][0],mat[0][1],mat[0][2],mat[0][3],
                        mat[1][0],mat[1][1],mat[1][2],mat[1][3],
                        mat[2][0],mat[2][1],mat[2][2],mat[2][3],
                        mat[3][0],mat[3][1],mat[3][2],mat[3][3]);
}

glm::mat4 AnimationLoader::AiToGLMMat4(const aiMatrix4x4 &inMat) {
    glm::mat4 tmp;
    tmp[0][0] = inMat.a1;
    tmp[1][0] = inMat.b1;
    tmp[2][0] = inMat.c1;
    tmp[3][0] = inMat.d1;

    tmp[0][1] = inMat.a2;
    tmp[1][1] = inMat.b2;
    tmp[2][1] = inMat.c2;
    tmp[3][1] = inMat.d2;

    tmp[0][2] = inMat.a3;
    tmp[1][2] = inMat.b3;
    tmp[2][2] = inMat.c3;
    tmp[3][2] = inMat.d3;

    tmp[0][3] = inMat.a4;
    tmp[1][3] = inMat.b4;
    tmp[2][3] = inMat.c4;
    tmp[3][3] = inMat.d4;
    return tmp;
}

void AnimationLoader::loadNodesAnim() {
    if (m_scene == NULL)
        return;

    if (m_scene->mNumAnimations == 0)
        return;

    aiNodesAnim.clear();
    for (unsigned int i = 0; i < m_scene->mAnimations[0]->mNumChannels; i++) {
        aiNodesAnim.push_back(m_scene->mAnimations[0]->mChannels[i]);
    }

    aiNode *rootNode = m_scene->mRootNode;
    globalInverseTransform = glm::inverse(AiToGLMMat4(rootNode->mTransformation));
}

void AnimationLoader::loadBoneParents() {
    for (unsigned int i = 0; i < bones.size(); i++) {
        const std::string bName = bones[i]->m_name;
        const std::string parentName = FindAiNode(bName)->mParent->mName.data;

        SkeletalBone *pBone = FindBone(parentName);

        bones[i]->m_parentBone = pBone;

        if (pBone == NULL)
            std::cout<<"Parent Bone for "<<bName<<" does not exist (is nullptr)"<<std::endl;
    }
}

void AnimationLoader::addBone(SkeletalBone *bone) {
    bones.push_back(bone);
}

void AnimationLoader::recursiveLoadNodes(aiNode *node) {
    //node->mTransformation = AnimationLoader::GLMMat4ToAi(glm::mat4(1.0f));
    aiNodes.push_back(node);

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        recursiveLoadNodes(node->mChildren[i]);
}

SkeletalBone *AnimationLoader::FindBone(const std::string &name) {
    for (unsigned int i = 0; i < bones.size(); i++) {
        if (bones[i]->m_name == name)
            return bones[i];
    }

    return NULL;
}

aiNode *AnimationLoader::FindAiNode(const std::string &name) {
     for (unsigned int i = 0; i < aiNodes.size(); i++) {
        if (aiNodes[i]->mName.data == name)
            return aiNodes[i];
     }

     return NULL;
}

aiNodeAnim *AnimationLoader::FindAiNodeAnim(const std::string &name) {
     for (unsigned int i = 0; i < aiNodesAnim.size(); i++) {
        if (aiNodesAnim[i]->mNodeName.data == name)
            return aiNodesAnim[i];
     }

     return NULL;
}

int AnimationLoader::FindBoneIDByName(const std::string &name) {
     for (unsigned int i = 0; i < bones.size(); i++) {
        if (bones[i]->m_name.compare(name) == 0)
            return i;
    }

    return -1;
}