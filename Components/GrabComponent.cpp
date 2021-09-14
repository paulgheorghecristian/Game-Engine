#include "GrabComponent.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "Common.h"

unsigned int GrabComponent::g_num_of_instances = 0;

GrabComponent::GrabComponent(float radius) : m_radius(radius), showGUI(false), localRotate(false), m_ghostObj(NULL) {
    imguiID = GrabComponent::g_num_of_instances;
    GrabComponent::g_num_of_instances++;
}

void GrabComponent::input(Input &inputManager) {
    PhysicsComponent *physicsComponent = (PhysicsComponent *) _entity->getComponent (Entity::Flags::DYNAMIC);

    if (!inputManager.getWarpMouse() && inputManager.getMouseDown(1) &&
            PhysicsMaster::getInstance()->getMouseRayInteresectEntities().find(_entity) !=
            PhysicsMaster::getInstance()->getMouseRayInteresectEntities().end()) {
        if (physicsComponent != NULL) {
            physicsComponent->disable();
        }
        showGUI = true;

        worldPos = _entity->getTransform().getPosition();
        worldRot = _entity->getTransform().getEulerRotation();
        tmpWorldRot = worldRot;
        worldScale = _entity->getTransform().getScale();
    }

    if (showGUI && inputManager.getMouseDown(3)) {
        if (physicsComponent != NULL) {
            physicsComponent->enable();
        }

        showGUI = false;
    }
}

void GrabComponent::update() {
    if (showGUI) {
        glm::vec3 res = tmpWorldRot - worldRot;
        _entity->getTransform().setPosition(worldPos);
        _entity->getTransform().addRotation(res.x, res.y, res.z, localRotate);
        _entity->getTransform().setScale(worldScale);
    }

    btTransform transform = m_ghostObj->getWorldTransform();
    const glm::vec3 &entityPos = _entity->getTransform().getPosition();

    transform.setOrigin(btVector3(entityPos.x, entityPos.y, entityPos.z));
    m_ghostObj->setWorldTransform(transform);
}

void GrabComponent::render() {
    if (showGUI) {
        tmpWorldRot = worldRot;

        ImGui::Begin("GUI");
        ImGui::PushID(std::to_string(imguiID).c_str());
        ImGui::Text("Position");
        ImGui::DragFloat("x", &worldPos.x, 0.05f);
        ImGui::DragFloat("y", &worldPos.y, 0.05f);
        ImGui::DragFloat("z", &worldPos.z, 0.05f);
        ImGui::Text("Rotation");
        ImGui::DragFloat("Rotx", &worldRot.x, 0.05f);
        ImGui::DragFloat("Roty", &worldRot.y, 0.05f);
        ImGui::DragFloat("Rotz", &worldRot.z, 0.05f);
        ImGui::Checkbox("Local Rotation", &localRotate);
        ImGui::Text("Scale");
        ImGui::DragFloat("Scalex", &worldScale.x, 0.005f);
        ImGui::DragFloat("Scaley", &worldScale.y, 0.005f);
        ImGui::DragFloat("Scalez", &worldScale.z, 0.005f);
        if (ImGui::Button("Delete")) {
            _entity->setToBeRemoved(true);
        }
        ImGui::Separator();
        ImGui::PopID();
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
    worldScale = _entity->getTransform().getScale();
}

const unsigned int GrabComponent::getFlag() const {
    return Entity::Flags::GRAB;
}

std::string GrabComponent::jsonify() {
    std::string res("");

    res += "\"GrabComponent\":{";
    res += "\"radius\":" + std::to_string(m_radius) + "}";

    return res;
}

GrabComponent::~GrabComponent()
{
    PhysicsMaster::getInstance()->getWorld()->removeCollisionObject(m_ghostObj);

    UserData *uData = (UserData *) m_ghostObj->getUserPointer();
    btCollisionShape *collisionShape = m_ghostObj->getCollisionShape();

    delete uData;
    uData = NULL;

    delete collisionShape;
    collisionShape = NULL;

    delete m_ghostObj;
    m_ghostObj = NULL;
}
