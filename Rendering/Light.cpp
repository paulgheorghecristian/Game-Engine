#include "Light.h"

Light::Light(const Transform &transform,
             const glm::vec3 &color,
             int depthMapWidth, int depthMapHeight,
             bool casts_shadow,
             bool needs_stencil) : m_transform(transform),
                                   m_lightColor(color),
                                   m_casts_shadow(casts_shadow),
                                   m_needs_stencil_test(needs_stencil),
                                   m_shadowMapFrameBuffer(depthMapWidth,
                                                          depthMapHeight,
                                                          0),
                                   m_shadowMapTexture(m_shadowMapFrameBuffer.getDepthTextureId(),
                                                      0)

{

}

Light::Light(const Transform &transform,
             const glm::vec3 &color,
             bool casts_shadow,
             bool needs_stencil) : m_transform(transform),
                                   m_lightColor(color),
                                   m_casts_shadow(casts_shadow),
                                   m_needs_stencil_test(needs_stencil)
{

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

}

Light::~Light()
{

}
