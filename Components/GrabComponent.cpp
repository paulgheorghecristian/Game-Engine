#include "GrabComponent.h"

#include "RenderingMaster.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "Common.h"

GrabComponent::GrabComponent(float radius) : m_radius(radius), performRayTest(false), showGUI(false) {
}

void GrabComponent::input(Input &inputManager) {
    if (inputManager.getMouseDown(1)) {
        performRayTest = true;
    }

    if (inputManager.getMouseUp(1)) {
        performRayTest = false;
    }

    if (showGUI && inputManager.getMouseDown(3)) {
        PhysicsComponent *physicsComponent = (PhysicsComponent *) _entity->getComponent (Entity::Flags::DYNAMIC);

        if (physicsComponent != NULL) {
            physicsComponent->enable();
        }

        showGUI = false;
    }
}

void GrabComponent::update() {
    //todo maybe execute only once in rendering master the intersection
    const glm::vec3 &currWorldPos = RenderingMaster::getInstance()->getCamera()->getPosition();
    const glm::vec3 &dir = RenderingMaster::getInstance()->getCurrWorldPosRay();
    const glm::vec3 &entityPos = _entity->getTransform().getPosition();

    glm::vec3 from_glm = currWorldPos+dir*10.0f;
    glm::vec3 to_glm = currWorldPos+dir*1000.0f;

    btVector3 from = btVector3(from_glm.x, from_glm.y, from_glm.z);
    btVector3 to = btVector3(to_glm.x, to_glm.y, to_glm.z);

    if (performRayTest) {

        btCollisionWorld::AllHitsRayResultCallback rayCallback(from, to);
        PhysicsMaster::getInstance()->getWorld()->rayTest(from, to, rayCallback);

        for (int i = 0; i < rayCallback.m_hitFractions.size(); i++) {
            UserData *data = (UserData*) rayCallback.m_collisionObjects[i]->getUserPointer();
            if (data && data->type == PointerType::ENTITY && data->pointer.entity == _entity) {
                PhysicsComponent *physicsComponent = (PhysicsComponent *) _entity->getComponent (Entity::Flags::DYNAMIC);

                showGUI = true;
                worldPos = _entity->getTransform().getPosition();
                worldRot = _entity->getTransform().getEulerRotation();

                if (physicsComponent != NULL) {
                    physicsComponent->disable();
                }
            }
        }
    }

    if (showGUI) {
        _entity->getTransform().setPosition(worldPos);
        _entity->getTransform().setRotation(worldRot);
        performRayTest = false;
    }

    btTransform transform = m_ghostObj->getWorldTransform();
    btVector3 origin = transform.getOrigin();

    transform.setOrigin(btVector3(entityPos.x, entityPos.y, entityPos.z));
    m_ghostObj->setWorldTransform(transform);
}

void GrabComponent::render() {
    if (showGUI) {
        ImGui::Begin("GUI");
        ImGui::Text("Position");
        ImGui::DragFloat("x", &worldPos.x, 0.05f);
        ImGui::DragFloat("y", &worldPos.y, 0.05f);
        ImGui::DragFloat("z", &worldPos.z, 0.05f);
        ImGui::Separator();
        ImGui::Text("Rotation");
        ImGui::DragFloat("Rotx", &worldRot.x, 0.0005f);
        ImGui::DragFloat("Roty", &worldRot.y, 0.0005f);
        ImGui::DragFloat("Rotz", &worldRot.z, 0.0005f);
        ImGui::End();
    }
}

void GrabComponent::init() {
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

    worldPos = _entity->getTransform().getPosition();
    worldRot = _entity->getTransform().getEulerRotation();
}

const unsigned int GrabComponent::getFlag() const {
    return Entity::Flags::GRAB;
}

GrabComponent::~GrabComponent()
{

}
