#include "Light.h"
#include "Common.h"
#include "Input.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

unsigned int Light::g_num_of_instances = 0;

Light::Light(const Transform &transform,
             const glm::vec3 &color,
             int depthMapWidth, int depthMapHeight,
             bool casts_shadow,
             bool needs_stencil,
             bool grab) : m_transform(transform),
                                   m_lightColor(color),
                                   m_casts_shadow(casts_shadow),
                                   m_needs_stencil_test(needs_stencil),
                                   m_shadowMapFrameBuffer(depthMapWidth,
                                                          depthMapHeight,
                                                          0),
                                   m_shadowMapTexture(m_shadowMapFrameBuffer.getDepthTextureId(),
                                                      0),
                                    m_can_be_grabbed(grab),
                                    showGUI(false),
                                    m_to_be_removed(false)
{
    initPhysics(transform, grab);
    imguiID = Light::g_num_of_instances;
    Light::g_num_of_instances++;
}

Light::Light(const Transform &transform,
             const glm::vec3 &color,
             bool casts_shadow,
             bool needs_stencil,
             bool grab) : m_transform(transform),
                                   m_lightColor(color),
                                   m_casts_shadow(casts_shadow),
                                   m_needs_stencil_test(needs_stencil),
                                   m_can_be_grabbed(grab),
                                    showGUI(false),
                                    m_to_be_removed(false)
{
    initPhysics(transform, grab);
    imguiID = Light::g_num_of_instances;
    Light::g_num_of_instances++;
}

void Light::recomputeShadowMapViewMatrix()
{
}

void Light::recomputeShadowMapProjectionMatrix()
{
}

Transform &Light::getTransform()
{
    return m_transform;
}

const glm::mat4 &Light::getShadowMapViewMatrix()
{
    return m_shadowMapViewMatrix;
}

const glm::mat4 &Light::getShadowMapProjectionMatrix()
{
    return m_shadowMapProjectionMatrix;
}

FrameBuffer &Light::getShadowMapFrameBuffer()
{
    return m_shadowMapFrameBuffer;
}

Texture &Light::getShadowMapTexture()
{
    return m_shadowMapTexture;
}

void Light::update()
{
    if (m_can_be_grabbed) {
        btTransform transform = m_ghostObj->getWorldTransform();
        btVector3 origin = transform.getOrigin();
        glm::vec3 pos = m_transform.getPosition();

        transform.setOrigin(btVector3(pos.x, pos.y, pos.z));
        m_ghostObj->setWorldTransform(transform);
    }
}

void Light::initPhysics(const Transform& transform, bool grab) {
    btTransform t;
    const glm::vec3 &position = transform.getPosition();
    btCollisionShape *collisionShape;

    t.setIdentity();
    t.setOrigin(btVector3(position.x, position.y, position.z));

    if (grab) {
        collisionShape = new btSphereShape(50.0f);

        UserData *userData = new UserData();

        m_ghostObj = new btGhostObject();
        m_ghostObj->setCollisionShape(collisionShape);
        m_ghostObj->setWorldTransform(t);
        userData->type = PointerType::LIGHT;
        userData->pointer.light = this;
        m_ghostObj->setUserPointer((void *) userData);
        m_ghostObj->setCollisionFlags(m_ghostObj->getCollisionFlags() |
                                btCollisionObject::CF_NO_CONTACT_RESPONSE);

        PhysicsMaster::getInstance()->getWorld()->addCollisionObject(m_ghostObj);
    }
}

void Light::renderGUI() {
    if (showGUI == true) {
        ImGui::Begin("Lights");
        ImGui::PushID(std::to_string(imguiID).c_str());
        ImGui::Text("Light Color");
        ImGui::ColorEdit3("light color", (float *) &m_lightColor);

        glm::vec3 pos = m_transform.getPosition();

        ImGui::Text("Light Position");
        ImGui::DragFloat("x", &pos.x, 0.05f);
        ImGui::DragFloat("y", &pos.y, 0.05f);
        ImGui::DragFloat("z", &pos.z, 0.05f);

        if (ImGui::Button("Delete")) {
            m_to_be_removed = true;
        }

        m_transform.setPosition(pos);
        ImGui::Separator();
        ImGui::PopID();
        ImGui::End();
    }
}

void Light::input(Input &inputManager) {
    if (!inputManager.getWarpMouse() && inputManager.getMouseDown(1) &&
            PhysicsMaster::getInstance()->getMouseRayInteresectLights().find(this) !=
            PhysicsMaster::getInstance()->getMouseRayInteresectLights().end()) {
        showGUI = true;
    }

    if (showGUI && inputManager.getMouseDown(3)) {
        showGUI = false;
    }
}

Light::~Light()
{

}
