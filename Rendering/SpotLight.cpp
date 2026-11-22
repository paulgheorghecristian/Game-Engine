#include "SpotLight.h"
#include "Shader.h"
#include "RenderingMaster.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#define SHADOW_MAP_WIDTH 2048
#define SHADOW_MAP_HEIGHT 2048

RenderingObject SpotLight::g_lightMesh;

SpotLight::SpotLight(const Transform &transform,
                     const glm::vec3 &color,
                     bool casts_shadow,
                     bool volumetric,
                     int shadowMapSize) : Light(transform,
                                                color,
                                                shadowMapSize,
                                                shadowMapSize,
                                                casts_shadow, false, true), m_volumetric(volumetric)
{
    float near = 1.0f;
    float far = m_transform.getScale().z+near;

    /* a cone with an explicit angle gets a frustum fitted around it, so the
     * whole shadow map covers the beam (90 degrees otherwise, as before) */
    float fov = glm::radians(90.0f);
    if (m_coneAngle > 0.0f) {
        fov = glm::min(fov, 2.0f * m_coneAngle * 1.2f);
    }
    m_shadowMapProjectionMatrix = glm::perspective(fov, 1.0f, near, far);
    m_shadowMapViewMatrix = glm::mat4_cast (glm::inverse (m_transform.getRotation()));
    m_shadowMapViewMatrix = glm::translate (m_shadowMapViewMatrix, -(m_transform.getPosition()));

    worldRot = m_transform.getEulerRotation();

    numSamplePoints = 26;
    coef1 = 0.5;
    coef2 = 0.1;

    m_att_diffuse.x = 500.0f;
    m_att_diffuse.y = 10.0f;
    m_att_diffuse.z = 10.0f;

    m_att_specular.x = 500.0f;
    m_att_specular.y = 10.0f;
    m_att_specular.z = 10.0f;
}

SpotLight::~SpotLight()
{

}

void SpotLight::render(Shader &shader)
{
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());

    shader.bind();
    g_lightMesh.getMeshes()[0]->draw();
    shader.unbind();
}

void SpotLight::render()
{
    Shader &shader = getLightAccumulationShader();

    // empirical value
    float alpha = getConeAngle();
    float lightLength = m_transform.getScale().z;

    shader.updateUniform("spotLightProjectionMatrix", (void *) &m_shadowMapProjectionMatrix);
    shader.updateUniform("spotLightViewMatrix", (void *) &m_shadowMapViewMatrix);
    glm::mat4 eyeToSpotLightClip = m_shadowMapProjectionMatrix * m_shadowMapViewMatrix *
                                   RenderingMaster::getInstance()->getInverseViewMatrix();
    shader.updateUniform("eyeToSpotLightClip", (void *) &eyeToSpotLightClip);
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
    shader.updateUniform("lightColor", (void *) &m_lightColor);
    shader.updateUniform("lightPosition", (void *) &m_transform.getPosition());
    shader.updateUniform("alpha", alpha);
    shader.updateUniform("lightLength", lightLength);
    shader.updateUniform("att_diffuse", (void *) &m_att_diffuse);
    shader.updateUniform("att_specular", (void *) &m_att_specular);

    shader.updateUniform("castsShadow", isCastingShadow() ? 1 : 0);
    m_shadowMapTexture.use(0);

    shader.bind();
    g_lightMesh.getMeshes()[0]->draw();
    shader.unbind();
}

Shader &SpotLight::getLightAccumulationShader()
{
    static Shader shader("res/shaders/lightAccCreatorSpotLight.json");

    return shader;
}

void SpotLight::setLightMesh(RenderingObject &&lightMesh) {
    g_lightMesh = std::move(lightMesh);
}

void SpotLight::recomputeShadowMapViewMatrix()
{
    m_shadowMapViewMatrix = glm::mat4_cast (glm::inverse (m_transform.getRotation()));
    m_shadowMapViewMatrix = glm::translate (m_shadowMapViewMatrix, -(m_transform.getPosition()));
}

void SpotLight::recomputeShadowMapProjectionMatrix()
{
    float near = 1.0f;
    float far = m_transform.getScale().z+near;

    /* a cone with an explicit angle gets a frustum fitted around it, so the
     * whole shadow map covers the beam (90 degrees otherwise, as before) */
    float fov = glm::radians(90.0f);
    if (m_coneAngle > 0.0f) {
        fov = glm::min(fov, 2.0f * m_coneAngle * 1.2f);
    }
    m_shadowMapProjectionMatrix = glm::perspective(fov, 1.0f, near, far);
}

void SpotLight::prepareOpenGLForLightPass()
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

void SpotLight::prepareOpenGLForStencilPass()
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

void SpotLight::renderGUI() {
    if (showGUI == true) {
        glm::vec3 scale = m_transform.getScale();

        ImGui::Begin("Lights");
        ImGui::PushID(std::to_string(imguiID).c_str());
        ImGui::Checkbox("Volumetric", &m_volumetric);

        ImGui::Text("Light Rotation");
        ImGui::DragFloat("rx", &worldRot.x, 0.5f);
        ImGui::DragFloat("ry", &worldRot.y, 0.5f);
        ImGui::DragFloat("rz", &worldRot.z, 0.5f);

        ImGui::Text("Light Scale");
        ImGui::DragFloat("Radius", &scale.x, 0.1f);
        ImGui::DragFloat("Length", &scale.z, 0.1f);
        scale.y = scale.x;
        if (m_coneAngle > 0.0f) {
            ImGui::SliderFloat("Cone angle", &m_coneAngle, 0.101f, 0.6f, "%.3f rad");
            /* keep the light volume enclosing the cone */
            scale.x = scale.y = glm::max(scale.x, 2.1f * scale.z * glm::tan(m_coneAngle));
        }

        ImGui::Text("Volumetric attribs");
        ImGui::DragInt("numSamplePoints", &numSamplePoints);
        ImGui::DragFloat("coef1", &coef1, 0.01f);
        ImGui::DragFloat("coef2", &coef2, 0.01f);
        ImGui::Text("Beam (beamDensity > 0: fades along its length)");
        ImGui::SliderFloat("Beam density", &m_beamDensity, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("Beam falloff", &m_beamFalloff, 20.0f, 1000.0f, "%.0f");

        m_transform.setScale(scale);

        ImGui::PopID();
        ImGui::End();
    }
    Light::renderGUI();
}

void SpotLight::update() {
    Light::update();

    if (showGUI == true) {
        m_transform.setRotation(worldRot);
    }
}

std::string SpotLight::jsonifyAttribs() {
    std::string res("");

    res += Light::jsonifyAttribs();
    res += ",\"type\": \"SPOT\"";
    if (m_volumetric == true) {
        res += ",\"volumetric\": true";
    } else if (m_volumetric == false) {
        res += ",\"volumetric\": false";
    }
    res += ",\"numSamplePoints\":" + std::to_string(numSamplePoints);
    res += ",\"coef1\":" + std::to_string(coef1);
    res += ",\"coef2\":" + std::to_string(coef2);
    res += ",\"shadowMapSize\":" + std::to_string(m_shadowMapFrameBuffer.getWidth());
    if (m_beamDensity > 0.0f) {
        res += ",\"beamDensity\":" + std::to_string(m_beamDensity);
        res += ",\"beamFalloff\":" + std::to_string(m_beamFalloff);
    }
    if (m_coneAngle > 0.0f) {
        res += ",\"coneAngle\":" + std::to_string(m_coneAngle);
    }

    return res;
}