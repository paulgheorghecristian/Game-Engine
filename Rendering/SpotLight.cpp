#include "SpotLight.h"
#include "Shader.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#define SHADOW_MAP_WIDTH 2048
#define SHADOW_MAP_HEIGHT 2048

SpotLight::SpotLight(const Transform &transform,
                     const glm::vec3 &color,
                     bool casts_shadow,
                     bool volumetric) : Light(transform,
                                                color,
                                                SHADOW_MAP_WIDTH,
                                                SHADOW_MAP_HEIGHT,
                                                casts_shadow, false, true), m_volumetric(volumetric)
{
    float near = 1.0f;
    float far = m_transform.getScale().z+near;

    m_shadowMapProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, near, far);
    m_shadowMapViewMatrix = glm::mat4_cast (glm::inverse (m_transform.getRotation()));
    m_shadowMapViewMatrix = glm::translate (m_shadowMapViewMatrix, -(m_transform.getPosition()));

    worldRot = m_transform.getEulerRotation();

    numSamplePoints = 100;
    coef1 = 0.5;
    coef2 = 0.1;

    m_att_diffuse.x = 5.0f;
    m_att_diffuse.y = 0.01f;
    m_att_diffuse.z = 0.04f;

    m_att_specular.x = 5.0f;
    m_att_specular.y = 0.01f;
    m_att_specular.z = 0.01f;
}

SpotLight::~SpotLight()
{

}

void SpotLight::render(Shader &shader)
{
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());

    shader.bind();
    getLightMesh().draw();
    shader.unbind();
}

void SpotLight::render()
{
    Shader &shader = getLightAccumulationShader();

    // empirical value
    float alpha = m_transform.getScale().x / 1000.0f;
    float lightLength = m_transform.getScale().z;

    shader.updateUniform("spotLightProjectionMatrix", (void *) &m_shadowMapProjectionMatrix);
    shader.updateUniform("spotLightViewMatrix", (void *) &m_shadowMapViewMatrix);
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
    shader.updateUniform("lightColor", (void *) &m_lightColor);
    shader.updateUniform("lightPosition", (void *) &m_transform.getPosition());
    shader.updateUniform("alpha", alpha);
    shader.updateUniform("lightLength", lightLength);
    shader.updateUniform("att_diffuse", (void *) &m_att_diffuse);
    shader.updateUniform("att_specular", (void *) &m_att_specular);

    m_shadowMapTexture.use(0);

    shader.bind();
    getLightMesh().draw();
    shader.unbind();
}

Shader &SpotLight::getLightAccumulationShader()
{
    static Shader shader("res/shaders/lightAccCreatorSpotLight.json");

    return shader;
}

Mesh &SpotLight::getLightMesh()
{
    static Mesh *mesh = Mesh::loadObject("res/models/SpotLightMesh5.obj");

    return *mesh;
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

    m_shadowMapProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, near, far);
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

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

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
    glDepthMask(GL_FALSE);

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

        ImGui::Text("Volumetric attribs");
        ImGui::DragInt("numSamplePoints", &numSamplePoints);
        ImGui::DragFloat("coef1", &coef1, 0.01f);
        ImGui::DragFloat("coef2", &coef2, 0.01f);

        m_transform.setScale(scale);

        ImGui::PopID();
        ImGui::End();
    }
    Light::renderGUI();
}

void SpotLight::update() {
    Light::update();

    if (showGUI == true) {
        m_transform.setRotation(glm::radians(worldRot));
    }
}
