#include "PointLight.h"

#include "Shader.h"
#include "Entity.h"
#include "RenderComponent.h"
#include "InstanceRenderComponent.h"
#include "RenderingMaster.h"

#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#define CUTOFF_OFFSET 50

RenderingObject PointLight::g_lightMesh;

Shader &PointLight::getLightAccumulationShader()
{
    static Shader shader("res/shaders/lightAccCreatorPointLight.json");

    return shader;
}

void PointLight::setLightMesh(RenderingObject &&lightMesh) {
    g_lightMesh = std::move(lightMesh);
}

PointLight::PointLight(const Transform &transform,
                       const glm::vec3 &color,
                       bool castShadow,
                       int shadowMapSize) : Light(transform, color, castShadow, false, true),
                                            m_shadowMapSize(shadowMapSize)
{
    m_att_diffuse.x = 500.0f;
    m_att_diffuse.y = 10.0f;
    m_att_diffuse.z = 10.0f;

    m_att_specular.x = 500.0f;
    m_att_specular.y = 10.0f;
    m_att_specular.z = 10.0f;
}

PointLight::~PointLight()
{
    if (m_shadowFbo != 0) {
        glDeleteFramebuffers(1, &m_shadowFbo);
    }
    if (m_shadowCube != 0) {
        glDeleteTextures(1, &m_shadowCube);
    }

}

void PointLight::render(Shader &shader)
{
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());

    shader.bind();
    g_lightMesh.getMeshes()[0]->draw();
    shader.unbind();
}

void PointLight::render()
{
    float cutOff = m_transform.getScale().x * 0.75f;
    Shader &shader = getLightAccumulationShader();

    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
    shader.updateUniform("lightColor", (void *) &m_lightColor);
    shader.updateUniform("lightPosition", (void *) &m_transform.getPosition());
    shader.updateUniform("cutOff", cutOff);
    shader.updateUniform("att_diffuse", (void *) &m_att_diffuse);
    shader.updateUniform("att_specular", (void *) &m_att_specular);

    int castsShadow = (isCastingShadow() && m_shadowCube != 0) ? 1 : 0;
    float range = getRange();
    shader.updateUniform("castsShadow", castsShadow);
    shader.updateUniform("shadowFar", range);
    shader.updateUniform("shadowCube", 5);
    shader.updateUniform("emitAxis", (void *) &m_emitAxis);
    shader.updateUniform("emitCos", (void *) &m_emitCos);
    shader.updateUniform("tubeAxis", (void *) &m_tubeAxis);
    shader.updateUniform("tubeHalf", m_tubeHalf);
    if (castsShadow) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_shadowCube);
    }

    shader.bind();
    g_lightMesh.getMeshes()[0]->draw();
    shader.unbind();
}

void PointLight::prepareOpenGLForLightPass()
{
    if (needsStencilTest()) {
        glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    /* Draw the volume's back faces with depth test GEQUAL: only pixels whose
     * scene geometry lies in front of the back of the (convex) volume can be
     * lit, so sky / far geometry behind the light gets rejected by early-Z
     * instead of running the lighting shader. Depth clamp keeps back faces
     * that cross the near/far planes from being clipped away. */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_CLAMP);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
}

void PointLight::prepareOpenGLForStencilPass()
{
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_CLAMP);

    glDisable(GL_CULL_FACE);
}

void PointLight::renderGUI() {
    if (showGUI == true) {
        glm::vec3 scale = m_transform.getScale();

        ImGui::Begin("Lights");
        ImGui::PushID(std::to_string(imguiID).c_str());

        ImGui::Text("Light Scale");
        ImGui::DragFloat("Radius", &scale.x, 0.1f);
        scale.y = scale.x;
        scale.z = scale.x;

        m_transform.setScale(scale);

        ImGui::PopID();
        ImGui::End();
    }
    Light::renderGUI();
}

void PointLight::setEmission(const glm::vec3 &axis, float full, float none)
{
    m_emitAxis = glm::length(axis) > 0.0f ? glm::normalize(axis) : glm::vec3(0.0f, -1.0f, 0.0f);
    m_emitCos = glm::vec2(std::cos(glm::radians(full)), std::cos(glm::radians(std::max(none, full + 0.1f))));
}

void PointLight::setTube(const glm::vec3 &axis, float length)
{
    m_tubeAxis = glm::length(axis) > 0.0f ? glm::normalize(axis) : glm::vec3(1.0f, 0.0f, 0.0f);
    m_tubeHalf = std::max(length, 0.0f) * 0.5f;
}

std::string PointLight::jsonifyAttribs() {
    std::string res("");

    res += Light::jsonifyAttribs();
    res += ",\"type\": \"POINT\"";
    res += ",\"shadowMapSize\":" + std::to_string(m_shadowMapSize);
    if (m_emitCos.x >= -1.0f) {
        res += ",\"emission\":{\"axis\":[" + std::to_string(m_emitAxis.x) + "," + std::to_string(m_emitAxis.y) + "," +
               std::to_string(m_emitAxis.z) + "],\"full\":" + std::to_string(glm::degrees(std::acos(m_emitCos.x))) +
               ",\"none\":" + std::to_string(glm::degrees(std::acos(m_emitCos.y))) + "}";
    }

    if (m_tubeHalf > 0.0f) {
        res += ",\"tube\":{\"axis\":[" + std::to_string(m_tubeAxis.x) + "," + std::to_string(m_tubeAxis.y) + "," +
               std::to_string(m_tubeAxis.z) + "],\"length\":" + std::to_string(m_tubeHalf * 2.0f) + "}";
    }
    return res;
}
Shader &PointLight::getShadowShader()
{
    static Shader shader("res/shaders/pointShadow.json");
    return shader;
}

bool PointLight::inFace(const glm::vec3 &p, float radius, int face)
{
    /* the face looks along one axis (sign s); its view is bounded by the 45 degree planes to the other
     * two: in it when in front, and within both pairs of side planes (r * sqrt 2: the sphere's reach
     * across a 45 degree plane) */
    const int axis = face / 2;
    const float forward = (face % 2 == 0 ? 1.0f : -1.0f) * p[axis];
    const float slack = radius * 1.41421356f;
    return forward > -radius && std::abs(p[(axis + 1) % 3]) <= forward + slack &&
           std::abs(p[(axis + 2) % 3]) <= forward + slack;
}

void PointLight::renderShadowCube(const std::vector<Entity *> &entities, unsigned faces)
{
    if (m_shadowCube == 0) {
        glGenTextures(1, &m_shadowCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_shadowCube);
        for (int face = 0; face < 6; face++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24,
                         m_shadowMapSize, m_shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        /* compared in the lookup (a filtered yes/no per tap: smooth shadow edges, not stair steps) */
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glGenFramebuffers(1, &m_shadowFbo);
    }

    static const glm::vec3 directions[6] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
    static const glm::vec3 ups[6] = { {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0} };

    const glm::vec3 position = m_transform.getPosition();
    const float range = getRange();
    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, range);

    Shader &shader = getShadowShader();
    shader.updateUniform("lightPosition", (void *) &position);
    shader.updateUniform("farPlane", range);
    shader.updateUniform("textureSampler", 0);
    RenderingMaster::getInstance()->uploadWind(shader);

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glViewport(0, 0, m_shadowMapSize, m_shadowMapSize);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    /* the faces turned away from the light: what is stored is the far side of each caster, so the
     * side facing the light never shadows itself (no acne) without a large bias, and shadows reach
     * the objects that cast them. (Double-sided materials and foliage have one side only: both sides
     * of those. Not terrain: it would shadow itself speckled with the bias this allows, and a light
     * near the ground hardly needs a hill's shadow on itself) */
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    for (int face = 0; face < 6; face++) {
        if (!(faces & (1u << face))) continue;      /* (its casters unchanged: kept) */
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_shadowCube, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        glm::mat4 viewProjection = projection * glm::lookAt(position, position + directions[face], ups[face]);
        shader.updateUniform("viewProjection", (void *) &viewProjection);

        for (Entity *entity : entities) {
            if (entity->getComponent(Entity::Flags::BILL) != NULL) {
                continue;
            }
            RenderComponent *rc = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
            if (rc != NULL && rc->castsShadow()) {
                /* only casters in range and not entirely behind this face */
                glm::vec3 localCenter;
                float localRadius;
                rc->getRenderingObject().getBoundingSphere(localCenter, localRadius);
                const glm::mat4 &model = entity->getTransform().getModelMatrix();
                glm::vec3 center = glm::vec3(model * glm::vec4(localCenter, 1.0f));
                glm::vec3 s = entity->getTransform().getScale();
                float radius = localRadius * glm::max(glm::abs(s.x), glm::max(glm::abs(s.y), glm::abs(s.z)));
                glm::vec3 toCenter = center - position;
                if (glm::length(toCenter) - radius > range || !inFace(toCenter, radius, face)) {
                    continue;
                }
                shader.updateUniform("instanced", false);
                rc->render(&shader);
            }
            InstanceRenderComponent *irc = (InstanceRenderComponent *) entity->getComponent(Entity::Flags::INSTANCE_RENDERABLE);
            if (irc != NULL) {
                shader.updateUniform("instanced", true);
                glDisable(GL_CULL_FACE);
                irc->render(&shader);
                glEnable(GL_CULL_FACE);
            }
        }
    }

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
