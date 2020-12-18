#include "PointLight.h"

#include "Shader.h"

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
                       const glm::vec3 &color) : Light(transform, color, false, false, true)
                       /* for now it doesn't cast shadow */
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

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

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
    glDepthMask(GL_FALSE);

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

std::string PointLight::jsonifyAttribs() {
    std::string res("");

    res += Light::jsonifyAttribs();
    res += ",\"type\": \"POINT\"";

    return res;
}