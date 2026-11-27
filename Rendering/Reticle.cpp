#include "Reticle.h"

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Mesh.h"
#include "Shader.h"

void Reticle::draw(int width, int height, float aim, float pulse, float shown)
{
    if (shown <= 0.001f) return;
    if (m_shader == nullptr) {
        m_shader.reset(new Shader("res/shaders/reticle.json"));
        m_quad.reset(Mesh::getRectangle());
    }
    /* sizes for a 1080 line screen, scaled with it */
    float scale = height / 1080.0f;
    aim = glm::clamp(aim, 0.0f, 1.0f);
    float radius = glm::mix(2.2f, 11.0f, aim) * scale * (1.0f - 0.18f * pulse);
    float thickness = glm::mix(1.6f, 2.2f, aim) * scale;
    float fill = 1.0f - glm::smoothstep(0.0f, 0.5f, aim);     /* a dot at rest, a ring when aimed */
    glm::vec3 rest(0.92f, 0.94f, 0.92f), active(1.0f, 0.74f, 0.34f);
    glm::vec4 color(glm::mix(rest, active, glm::smoothstep(0.3f, 1.0f, aim)), glm::mix(0.38f, 0.9f, aim) * shown);
    glm::vec2 size((float) width, (float) height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_shader->bind();
    m_shader->updateUniform("screenSize", (void *) &size);
    m_shader->updateUniform("radius", radius);
    m_shader->updateUniform("thickness", thickness);
    m_shader->updateUniform("fill", fill);
    m_shader->updateUniform("color", (void *) &color);
    m_quad->draw();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
