#include "Light.h"

Mesh *Light::pointMesh;
Mesh *Light::spotMesh;
Mesh *Light::directionalMesh;
glm::mat4 Light::dirLightProjMatrix;
glm::mat4 Light::dirLightViewMatrix;
FrameBuffer *Light::depthTextureFBDirLight;
Texture *Light::depthTextureDirLight;

Light::Light(LightType type,
             const glm::vec3 color,
             const Transform &transform) : m_type (type),
                                           m_transform (transform),
                                           m_lightColor (color),
                                           m_depthTextureFrameBufferForSpotLight (512,
                                                                                  512,
                                                                                  0),
                                           m_depthTextureForSpotLight (m_depthTextureFrameBufferForSpotLight.getDepthTextureId(),
                                                                       5) {
    if (type == LightType::SPOT) {
        float near = 1.0f;
        float far = 600.0f;

        glm::vec3 spotLightForward = glm::vec3(0,0,-1) * m_transform.getRotation();
        spotLightForward = glm::normalize (spotLightForward);

        m_spotLightPerspectiveMatrix = glm::perspective(glm::radians(100.0f), 1.0f, near, far);
        m_spotLightViewMatrix = glm::mat4_cast (glm::inverse (m_transform.getRotation()));
        m_spotLightViewMatrix = glm::translate (m_spotLightViewMatrix, -(m_transform.getPosition()));// + spotLightForward * 20.0f));
    } else {
        m_spotLightPerspectiveMatrix = glm::mat4 (1.0f);
        m_spotLightViewMatrix = glm::mat4 (1.0f);
    }
}

void Light::render(Shader &shader) {
    float cutOff = m_transform.getScale().x - LIGHT_CUTOFF_OFFSET;

    if (m_type != DIRECTIONAL) {
        shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
        shader.updateUniform("lightColor", (void *) &m_lightColor);
        shader.updateUniform("lightPosition", (void *) &m_transform.getPosition());
        if (m_type == SPOT) {
            cutOff = -1;
        }
        shader.updateUniform("cutOff", (void *) &cutOff);
    }

    shader.bind();
    switch (m_type) {
        case DIRECTIONAL:
            directionalMesh->draw();
            break;
        case POINT:
            pointMesh->draw();
            break;
        case SPOT:
            spotMesh->draw();
            break;
    }
    shader.unbind();
}

void Light::recomputeSpotLightViewMatrix() {
    m_spotLightViewMatrix = glm::mat4_cast (glm::inverse (m_transform.getRotation()));
    m_spotLightViewMatrix = glm::translate (m_spotLightViewMatrix, -m_transform.getPosition());
}

void Light::setPointMesh (Mesh *pointMesh) {
    Light::pointMesh = pointMesh;
}

void Light::setSpotMesh (Mesh *spotMesh) {
    Light::spotMesh = spotMesh;
}

void Light::setDirectionalMesh (Mesh *directionalMesh) {
    Light::directionalMesh = directionalMesh;
}

Transform &Light::getTransform() {
    return m_transform;
}

Light::LightType Light::getLightType() {
    return m_type;
}

const glm::mat4 &Light::getSpotLightPerspectiveMatrix() {
    assert (m_type == LightType::SPOT);
    return m_spotLightPerspectiveMatrix;
}

const glm::mat4 &Light::getSpotLightViewMatrix() {
    assert (m_type == LightType::SPOT);
    return m_spotLightViewMatrix;
}

FrameBuffer &Light::getDepthTextureFrameBufferForSpotLight() {
    return m_depthTextureFrameBufferForSpotLight;
}

Texture &Light::getDepthTextureForSpotLight() {
    return m_depthTextureForSpotLight;
}

Light::~Light() {

}
