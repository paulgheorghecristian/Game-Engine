#include "BounceLight.h"

#include <algorithm>

#include "FrameBuffer.h"
#include "Mesh.h"
#include "Shader.h"

static void bind(int unit, GLuint texture)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}

void BounceLight::render(int width, int height, GLuint depth, GLuint normals, GLuint light, GLuint albedo,
                         const glm::mat4 &projection, float radius, bool pbr, float emissiveStrength, Mesh *quad)
{
    int w = std::max(16, width / 2), h = std::max(16, height / 2);
    if (m_shader == nullptr) {
        m_shader.reset(new Shader("res/shaders/bounceLight.json"));
        m_blur.reset(new Shader("res/shaders/bounceLightBlur.json"));
        m_shader->updateUniform("depthSampler", 0);
        m_shader->updateUniform("normalSampler", 1);
        m_shader->updateUniform("lightSampler", 2);
        m_shader->updateUniform("albedoSampler", 3);
        m_blur->updateUniform("bounceSampler", 0);
        m_blur->updateUniform("depthSampler", 1);
        m_blur->updateUniform("normalSampler", 2);
    }
    if (m_gather == nullptr || w != m_width || h != m_height) {
        m_gather.reset(new FrameBuffer(w, h, 1, false, GL_RGBA16F));
        m_across.reset(new FrameBuffer(w, h, 1, false, GL_RGBA16F));
        m_result.reset(new FrameBuffer(w, h, 1, false, GL_RGBA16F));
        m_width = w;
        m_height = h;
    }
    glm::mat4 inverse = glm::inverse(projection);
    m_frame = m_frame + 1.0f > 64.0f ? 0.0f : m_frame + 1.0f;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_gather->bindSingleRenderTarget(0);
    m_shader->bind();
    m_shader->updateUniform("projectionMatrix", (void *) &projection);
    m_shader->updateUniform("inverseProjectionMatrix", (void *) &inverse);
    m_shader->updateUniform("radius", radius);
    m_shader->updateUniform("pbrEnabled", pbr);
    m_shader->updateUniform("emissiveStrength", emissiveStrength);
    m_shader->updateUniform("frame", m_frame);
    bind(0, depth);
    bind(1, normals);
    bind(2, light);
    bind(3, albedo);
    quad->draw();
    m_gather->unbind();

    m_blur->bind();
    m_blur->updateUniform("inverseProjectionMatrix", (void *) &inverse);
    bind(1, depth);
    bind(2, normals);
    glm::vec2 across(1.0f / w, 0.0f), down(0.0f, 1.0f / h);
    m_across->bindSingleRenderTarget(0);
    m_blur->updateUniform("direction", (void *) &across);
    bind(0, m_gather->getRenderTargets()[0]);
    quad->draw();
    m_across->unbind();
    m_result->bindSingleRenderTarget(0);
    m_blur->updateUniform("direction", (void *) &down);
    bind(0, m_across->getRenderTargets()[0]);
    quad->draw();
    m_result->unbind();
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}

GLuint BounceLight::texture() const
{
    return m_result != nullptr ? m_result->getRenderTargets()[0] : 0;
}
