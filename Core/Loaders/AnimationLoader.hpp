#ifndef __ANIMATION_LOADER_HPP__
#define __ANIMATION_LOADER_HPP__

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SkeletalBone.hpp"

class AnimationLoader {
    public:

        static void init(aiScene *scene);
        static void destroy();
        static AnimationLoader *getInstance();

        static aiMatrix4x4 GLMMat4ToAi(const glm::mat4 &mat);
        static glm::mat4 AiToGLMMat4(const aiMatrix4x4 &inMat);

        void loadNodesAnim();
        void addBone(SkeletalBone *bone);
        void loadBoneParents();
        void recursiveLoadNodes(aiNode *node);

        SkeletalBone *FindBone(const std::string &name);
        aiNode *FindAiNode(const std::string &name);
        aiNodeAnim *FindAiNodeAnim(const std::string &name);
        int FindBoneIDByName(const std::string &name);

        inline std::vector<aiNodeAnim *> &getAiNodesAnim() { return aiNodesAnim; }
        inline std::vector<SkeletalBone *> &getBones() { return bones; }
        inline glm::mat4 &getGlobalInverserTransform() { return globalInverseTransform; }

    private:
        AnimationLoader(aiScene *scene);
        ~AnimationLoader();

        static AnimationLoader *m_instance;

        std::vector<aiNode *> aiNodes;
        std::vector<aiNodeAnim *> aiNodesAnim;
        std::vector<SkeletalBone*> bones;
        glm::mat4 globalInverseTransform;

        aiScene *m_scene;
};

#endif /* __ANIMATION_LOADER_HPP__ */