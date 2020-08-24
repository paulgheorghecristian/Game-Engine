#include "ActionComponent.h"

#include "RenderingMaster.h"
#include "PhysicsMaster.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "Common.h"

ActionComponent::ActionComponent(float radius, std::function<void(bool start, Entity *entity)> action) : m_radius(radius), m_action(action){
}

void ActionComponent::input(Input &inputManager) {
    if (inputManager.getKeyUp(SDLK_e) &&
            PhysicsMaster::getInstance()->getForwardIntersectRes().find(_entity) !=
            PhysicsMaster::getInstance()->getForwardIntersectRes().end()) {
        (void) m_action(true, _entity);
    }
}

void ActionComponent::update() {
    const glm::vec3 &currWorldPos = RenderingMaster::getInstance()->getCamera()->getPosition();
    const glm::vec3 &entityPos = _entity->getTransform().getPosition();

    btTransform transform = m_ghostObj->getWorldTransform();
    btVector3 origin = transform.getOrigin();

    (void) m_action(false, _entity);

    transform.setOrigin(btVector3(entityPos.x, entityPos.y, entityPos.z));
    m_ghostObj->setWorldTransform(transform);
}

void ActionComponent::init() {
    assert (_entity != NULL);

    btTransform t;
    const glm::vec3 &position = _entity->getTransform().getPosition();
    btCollisionShape *collisionShape;

    t.setIdentity();
    t.setOrigin(btVector3(position.x, position.y, position.z));

    collisionShape = new btSphereShape(m_radius);

    UserData *userData = new UserData();

    m_ghostObj = new btGhostObject();
    m_ghostObj->setCollisionShape(collisionShape);
    m_ghostObj->setWorldTransform(t);
    userData->type = PointerType::ENTITY;
    userData->pointer.entity = _entity;
    m_ghostObj->setUserPointer((void *) userData);
    m_ghostObj->setCollisionFlags(m_ghostObj->getCollisionFlags() |
                              btCollisionObject::CF_NO_CONTACT_RESPONSE);

    PhysicsMaster::getInstance()->getWorld()->addCollisionObject(m_ghostObj);
}

const unsigned int ActionComponent::getFlag() const {
    return Entity::Flags::ACTION;
}

ActionComponent::~ActionComponent()
{

}
