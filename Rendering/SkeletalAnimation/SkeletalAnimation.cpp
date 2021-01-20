#include "SkeletalAnimation.hpp"

SkeletalAnimation::SkeletalAnimation() {
    m_startTime = m_endTime = m_priority = 0;
}

SkeletalAnimation::SkeletalAnimation(const std::string &name,
                                        const glm::vec2 &times, int priority) {
    m_name = name;
    m_startTime = times.x;
    m_endTime = times.y;
    m_priority = priority;
}

float SkeletalAnimation::animTimeToFrame(float time) {
     float frame = 0.033333333f; // 30 FPS
     return time / frame;
}

glm::vec2 SkeletalAnimation::framesToTime(const glm::vec2 &frames) {
     float frame = 0.033333333f; // 30 FPS
     return frames * frame;
}