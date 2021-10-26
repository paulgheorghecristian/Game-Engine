#include "Skeleton.hpp"
#include "AnimationLoader.hpp"

Skeleton::Skeleton() {
    m_time = m_startTime = m_endTime = 0;
    m_activeAnimation = NULL;

    m_animLoop = false;
    m_animPlay = false;

    m_scene = NULL;
}

Skeleton::Skeleton(aiScene *scene,
                    const std::vector<SkeletalBone *> &bones,
                    const glm::mat4 &globalInverseTransform): Skeleton() {
    m_scene = scene;
    init(bones, globalInverseTransform);
}

void Skeleton::init(const std::vector<SkeletalBone *> &bones, const glm::mat4 &globalInverseTransform) {
    m_bones = bones;
    m_globalInverseTransform = globalInverseTransform;

    m_time = m_startTime = m_endTime = 0;
    m_activeAnimation = NULL;
    m_idleAnimation = NULL;

    m_animLoop = false;

    for (unsigned int i = 0; i < m_bones.size(); i++)
        m_bones[i]->m_parentSkeleton = this;
}

SkeletalBone *Skeleton::findBone(const std::string &name) {
    // create hashmap?
    for (unsigned int i = 0; i < m_bones.size(); i++) {
        if (m_bones[i]->m_name.compare(name) == 0)
            return m_bones[i];
    }
    return NULL;
}

void Skeleton::updateBoneMatsVector() {
    if (m_bones.size() == 0)
        return;

    m_boneMats.clear();

    //TODO put constant
    for (unsigned int i = 0; i < 100; i++) {
        if (i >= m_bones.size()) {
            m_boneMats.push_back(glm::mat4(1.0));
        } else {
            glm::mat4 concatenated_transformation = (m_bones[i]->getParentTransforms() * 
                                            AnimationLoader::AiToGLMMat4(m_bones[i]->m_node->mTransformation));
            m_boneMats.push_back(m_globalInverseTransform * concatenated_transformation * m_bones[i]->m_offsetMatrix);
        }
    }
}

void Skeleton::update() {
    updateBoneMatsVector();

    if (!m_animPlay)
        //If we're not playing an animation, then just give up, do nothing.
        return;
             
    //Update the time variable by adding the delta time of the last frame
    //It's * 0.001f because the delta time is in milliseconds, and we 
    //need it in seconds.
    m_time += 0.016666667;

    //Make sure the time can't be less than our animation's start time.
    if (m_time < m_startTime)
        m_time = m_startTime;

    //Make sure the time can't be greater than our animation's end time.
    if (m_time > m_endTime) {
        if (m_animLoop)
            //If looping is set, then loop!
            m_time = m_startTime;
        else
            //Else, give up.
            stopAnimating();
    }

    for (unsigned int i = 0; i < m_bones.size(); i++)
        m_bones[i]->updateKeyframeTransform(m_time);
}

void Skeleton::playAnimation(SkeletalAnimation &anim, bool loop, bool resetToStart) {
    //If there's an animation currently playing
    if (m_activeAnimation != NULL) {
        //And this animation is more important than the current one
        if (anim.m_priority < m_activeAnimation->m_priority)
            //Set the current animation to the one passed in.
            m_activeAnimation = &anim;
        else
            //Do nothing.
            return;
    }
    else
        //Set the current animation to the one passed in.
        m_activeAnimation = &anim;

    m_startTime = m_activeAnimation->m_startTime;
    m_endTime = m_activeAnimation->m_endTime;

    m_animPlay = true;
    m_animLoop = loop;

    //The reset_to_start variable determines whether or not the animation
    //should restart upon playing.
    if (resetToStart)
        m_time = m_activeAnimation->m_startTime;
}

//This function stops animating
void Skeleton::stopAnimating() {
    m_time = m_endTime;
    m_activeAnimation = NULL;
    m_animPlay = false;
}

//This function sets the idle animation
//(An idle animation, for those who don't know, is an animation
//that plays when no other animations are playing)
void Skeleton::setIdleAnimation(SkeletalAnimation *in_anim) {
    m_idleAnimation = in_anim;
}

Skeleton::~Skeleton() {
    delete m_scene;
    m_scene = NULL;
    for (SkeletalBone *bone : m_bones) {
        delete bone;
        bone = NULL;
    }
}