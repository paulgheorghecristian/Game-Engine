#include "TempAnimationComponent.h"

TempAnimationComponent::TempAnimationComponent(glm::vec3 addPosition,
                                               glm::vec3 addRotation,
                                               glm::vec3 addScale) : m_addPosition(addPosition),
                                                                     m_addRotation(addRotation),
                                                                     m_addScale(addScale)
{
}

void TempAnimationComponent::input(Input &inputManager) {

}

void TempAnimationComponent::update() {
    _entity->getTransform().addPosition(m_addPosition.x, m_addPosition.y, m_addPosition.z);
    _entity->getTransform().addRotation(m_addRotation.x, m_addRotation.y, m_addRotation.z);
    _entity->getTransform().addScale(m_addScale.x, m_addScale.y, m_addScale.z);
}

void TempAnimationComponent::render() {

}

const unsigned int TempAnimationComponent::getFlag() const {
    return Entity::Flags::ANIMATED;
}

TempAnimationComponent::~TempAnimationComponent()
{
    //dtor
}
