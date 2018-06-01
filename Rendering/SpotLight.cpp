#include "SpotLight.h"

#include "Shader.h"

#define SHADOW_MAP_WIDTH 1024
#define SHADOW_MAP_HEIGHT 1024

SpotLight::SpotLight(const Transform &transform,
                     const glm::vec3 &color,
                     bool casts_shadow) : Light(transform,
                                                color,
                                                SHADOW_MAP_WIDTH,
                                                SHADOW_MAP_HEIGHT,
                                                casts_shadow, true)
{
    float near = 1.0f;
    float far = m_transform.getScale().z;

    m_shadowMapProjectionMatrix = glm::perspective(glm::radians(100.0f), 1.0f, near, far);
    m_shadowMapViewMatrix = glm::mat4_cast (glm::inverse (m_transform.getRotation()));
    m_shadowMapViewMatrix = glm::translate (m_shadowMapViewMatrix, -(m_transform.getPosition()));
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

    shader.updateUniform("spotLightProjectionMatrix", (void *) &m_shadowMapProjectionMatrix);
    shader.updateUniform("spotLightViewMatrix", (void *) &m_shadowMapViewMatrix);
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
    shader.updateUniform("lightColor", (void *) &m_lightColor);
    shader.updateUniform("lightPosition", (void *) &m_transform.getPosition());

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
    float far = m_transform.getScale().z;

    m_shadowMapProjectionMatrix = glm::perspective(glm::radians(100.0f), 1.0f, near, far);
}
