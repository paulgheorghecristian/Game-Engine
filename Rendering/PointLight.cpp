#include "PointLight.h"

#include "Shader.h"

#define CUTOFF_OFFSET 50

Shader &PointLight::getLightAccumulationShader()
{
    static Shader shader("res/shaders/lightAccCreatorPointLight.json");

    return shader;
}

Mesh &PointLight::getLightMesh()
{
    static Mesh *mesh = Mesh::loadObject("res/models/lightsphere.obj");

    return *mesh;
}

PointLight::PointLight(const Transform &transform,
                       const glm::vec3 &color) : Light(transform, color, false, false, true)
                       /* for now it doesn't cast shadow */
{

}

PointLight::~PointLight()
{

}

void PointLight::render(Shader &shader)
{
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());

    shader.bind();
    getLightMesh().draw();
    shader.unbind();
}

void PointLight::render()
{
    float cutOff = m_transform.getScale().x - CUTOFF_OFFSET;
    Shader &shader = getLightAccumulationShader();

    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
    shader.updateUniform("lightColor", (void *) &m_lightColor);
    shader.updateUniform("lightPosition", (void *) &m_transform.getPosition());
    shader.updateUniform("cutOff", (void *) &cutOff);

    shader.bind();
    getLightMesh().draw();
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
