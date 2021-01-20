#ifndef __SKELETAL_ANIMATION_HPP__
#define __SKELETAL_ANIMATION_HPP__

#include <glm/glm.hpp>
#include <string>

class SkeletalAnimation
{
     public:
         SkeletalAnimation();
         SkeletalAnimation(const std::string &name,
                            const glm::vec2 &times, int priority);

        static float animTimeToFrame(float time);
        static glm::vec2 framesToTime(const glm::vec2 &frames);
    private:
        std::string m_name;
        float m_startTime;
        float m_endTime;
        int m_priority;

        friend class Skeleton;
        friend class SkeletalAnimationComponent;
};


#endif /* __SKELETAL_ANIMATION_HPP__ */