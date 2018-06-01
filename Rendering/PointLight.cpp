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
    static Mesh *mesh = Mesh::loadObject("res/models/lightSphere.obj");

    return *mesh;
}

PointLight::PointLight(const Transform &transform,
                       const glm::vec3 &color) : Light(transform, color, false, true)
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
