#include "DirectionalLight.h"

#include "MathUtils.h"
#include "RenderingMaster.h"
#include "Shader.h"

#define SHADOW_MAP_WIDTH 3000
#define SHADOW_MAP_HEIGHT 3000

DirectionalLight::DirectionalLight(const Transform &transform,
                                   const glm::vec3 &color,
                                   const glm::vec3 &lightDirection,
                                   bool casts_shadow) : Light(transform,
                                                              color,
                                                              SHADOW_MAP_WIDTH,
                                                              SHADOW_MAP_HEIGHT,
                                                              casts_shadow, false),
                                                        m_lightDirection(lightDirection)
{
    Frustum frustum;

    /* TODO remove hardcode */
    MathUtils::calculateFrustum(RenderingMaster::getInstance()->getCamera(), 1.0f, 250.0f, 75.0f, 16.0f/9.0f, frustum);
    MathUtils::calculateFrustumSurroundingCuboid(RenderingMaster::getInstance()->getCamera(),
                                                 frustum,
                                                 m_lightDirection,
                                                 frustum,
                                                 m_shadowMapProjectionMatrix,
                                                 m_shadowMapViewMatrix);
}

DirectionalLight::~DirectionalLight()
{

}

void DirectionalLight::render(Shader &shader)
{
    shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());

    shader.bind();
    getLightMesh().draw();
    shader.unbind();
}

void DirectionalLight::render()
{
    Shader &shader = getLightAccumulationShader();

    shader.updateUniform("dirLightProjMatrix", (void *) &m_shadowMapProjectionMatrix);
    shader.updateUniform("dirLightViewMatrix", (void *) &m_shadowMapViewMatrix);

    m_shadowMapTexture.use(0);

    shader.bind();
    getLightMesh().draw();
    shader.unbind();
}

Shader &DirectionalLight::getLightAccumulationShader()
{
    static Shader shader("res/shaders/lightAccCreatorDirLight.json");

    return shader;
}

Mesh &DirectionalLight::getLightMesh()
{
    static Mesh *mesh = Mesh::getRectangle();

    return *mesh;
}

void DirectionalLight::update()
{

}

void DirectionalLight::recomputeShadowMapViewMatrix()
{
    Frustum frustum;

    /* TODO remove hardcode */
    MathUtils::calculateFrustum(RenderingMaster::getInstance()->getCamera(), 1.0f, 250.0f, 75.0f, 16.0f/9.0f, frustum);
    MathUtils::calculateFrustumSurroundingCuboid(RenderingMaster::getInstance()->getCamera(),
                                                 frustum,
                                                 m_lightDirection,
                                                 frustum,
                                                 m_shadowMapProjectionMatrix,
                                                 m_shadowMapViewMatrix);
}

void DirectionalLight::recomputeShadowMapProjectionMatrix()
{
    /* already calculated in recomputeShadowMapViewMatrix() */
}
