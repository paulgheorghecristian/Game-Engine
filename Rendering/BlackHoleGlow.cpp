#include "BlackHoleGlow.h"
#include "BlackHole.h"
#include "FrameBuffer.h"
#include "Mesh.h"
#include "Shader.h"

BlackHoleGlow::~BlackHoleGlow()
{
    if (m_texture != 0) glDeleteTextures(1, &m_texture);
    if (m_fbo != 0) glDeleteFramebuffers(1, &m_fbo);
}

bool BlackHoleGlow::wanted(const std::vector<BlackHole *> &holes)
{
    for (BlackHole *hole : holes) {
        if (hole->bloom > 0.0f && hole->drawsAnything()) return true;
    }
    return false;
}

void BlackHoleGlow::begin(int width, int height)
{
    if (m_texture == 0 || width != m_width || height != m_height) {
        if (m_texture == 0) {
            glGenTextures(1, &m_texture);
            glGenFramebuffers(1, &m_fbo);
        }
        m_width = width;
        m_height = height;
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void BlackHoleGlow::bind(FrameBuffer &target)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.getRenderTargets()[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_texture, 0);
    const GLenum both[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, both);
}

void BlackHoleGlow::finish(FrameBuffer &target)
{
    if (m_shader == nullptr) {
        m_shader.reset(new Shader("res/shaders/blackHoleGlow.json"));
        m_shader->updateUniform("glowSampler", 0);
        m_quad.reset(Mesh::getRectangle());
    }
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    m_shader->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_quad->draw();
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}
