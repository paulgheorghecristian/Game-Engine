#ifndef __SKELETON_HPP__
#define __SKELETON_HPP_

#include "SkeletalBone.hpp"

class Skeleton
{
    public:
        Skeleton();
        Skeleton(aiScene *scene,
                    const std::vector<SkeletalBone *> &bones,
                    const glm::mat4 &globalInverseTransform);
        ~Skeleton();
        void init(const std::vector<SkeletalBone *> &bones,
                    const glm::mat4 &globalInverseTransform);
        SkeletalBone *findBone(const std::string &name);
        void updateBoneMatsVector();
        void update();

        void playAnimation(SkeletalAnimation &anim, bool loop, bool resetToStart);
        void stopAnimating();
        void setIdleAnimation(SkeletalAnimation *in_anim);

    private:
        std::vector<SkeletalBone *> m_bones;
        glm::mat4 m_globalInverseTransform;
        aiScene *m_scene;

        std::vector<glm::mat4> m_boneMats;

        float m_time;

        float m_startTime;
        float m_endTime;

        SkeletalAnimation *m_activeAnimation;
        SkeletalAnimation *m_idleAnimation;

        bool m_animPlay;
        bool m_animLoop;

        friend class SkeletalAnimationComponent;
};

#endif /* __SKELETON_HPP_ */