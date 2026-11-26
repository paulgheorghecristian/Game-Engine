#include "RoomAlive.h"
#include "FrameBuffer.h"
#include "Mesh.h"
#include "PointLight.h"
#include "RenderingMaster.h"
#include "Shader.h"
#include "Timeline.h"

#include <algorithm>
#include <GL/glew.h>

const std::vector<Aftermath::Parameter> &RoomAlive::lanes()
{
    static const std::vector<Aftermath::Parameter> LANES = {
        { "aliveWarp", "Room alive", 0.0f, 1.0f, 0.0f, "How hard reality bends round the middle of the room: pulled in, twisted, breathing, rippling (0 = not at all)." },
        { "aliveDark", "Room alive", 0.0f, 1.0f, 0.0f, "How dark it gets where it pulls, deepest at the middle (0 = no darkness)." },
        { "aliveGlow", "Room alive", 0.0f, 1.0f, 0.0f, "How bright the warm light glowing up through the floor is (0 = none)." },
    };
    return LANES;
}

static glm::vec3 readVec3(const rapidjson::Value &json, const char *key, glm::vec3 fallback)
{
    if (json.HasMember(key) && json[key].IsArray() && json[key].Size() == 3) {
        return glm::vec3(json[key][0].GetFloat(), json[key][1].GetFloat(), json[key][2].GetFloat());
    }
    return fallback;
}

void RoomAlive::load(const rapidjson::Value &map)
{
    m_has = map.HasMember("RoomAlive") && map["RoomAlive"].IsObject();
    if (!m_has) return;
    const auto &json = map["RoomAlive"];
    m_center = readVec3(json, "center", m_center);
    m_glowPosition = readVec3(json, "glowPosition", m_center - glm::vec3(0.0f, 100.0f, 0.0f));
    m_glowColor = readVec3(json, "glowColor", m_glowColor);
    if (json.HasMember("radius") && json["radius"].IsNumber()) m_radius = json["radius"].GetFloat();
    if (json.HasMember("darkRadius") && json["darkRadius"].IsNumber()) m_darkRadius = json["darkRadius"].GetFloat();
    if (json.HasMember("glowRange") && json["glowRange"].IsNumber()) m_glowRange = json["glowRange"].GetFloat();
}

void RoomAlive::readTimeline(const Timeline &timeline)
{
    if (!m_has) return;
    m_warp = glm::clamp(timeline.get("aliveWarp", 0.0f), 0.0f, 1.0f);
    m_dark = glm::clamp(timeline.get("aliveDark", 0.0f), 0.0f, 1.0f);
    m_glow = glm::clamp(timeline.get("aliveGlow", 0.0f), 0.0f, 1.0f);
    updateLight();
}

void RoomAlive::updateLight()
{
    std::vector<Light *> &lights = RenderingMaster::getInstance()->getLights();
    if (m_glow <= 0.0f) {
        if (m_light != nullptr) {
            lights.erase(std::remove(lights.begin(), lights.end(), (Light *) m_light), lights.end());
            delete m_light;
            m_light = nullptr;
        }
        return;
    }
    if (m_light == nullptr) {
        /* unshadowed: it glows up through the floor */
        m_light = new PointLight(Transform(m_glowPosition, glm::vec3(0), glm::vec3(m_glowRange)), m_glowColor, false);
        m_light->setOwnedByFeature(true);
        RenderingMaster::getInstance()->addLightToScene(m_light);
    }
    m_light->setLightColor(m_glowColor * m_glow);
}

void RoomAlive::render(FrameBuffer &picture, unsigned int depthTexture, const glm::mat4 &viewProjection,
                       int width, int height, float time)
{
    if (!m_has || (m_warp <= 0.001f && m_dark <= 0.001f)) return;
    if (m_shader == nullptr) {
        m_copy = new FrameBuffer(width, height, 1);
        m_shader = new Shader("res/shaders/roomAlive.json");
        m_shader->updateUniform("pictureSampler", 0);
        m_shader->updateUniform("depthSampler", 1);
        m_quad = Mesh::getRectangle();
    }
    /* the picture as it is, to read from while drawing over it */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, picture.getFrameBufferObject());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_copy->getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    picture.bindSingleRenderTarget(0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_shader->bind();
    glm::mat4 inverse = glm::inverse(viewProjection);
    m_shader->updateUniform("viewProjection", (void *) &viewProjection);
    m_shader->updateUniform("inverseViewProjection", (void *) &inverse);
    m_shader->updateUniform("center", (void *) &m_center);
    m_shader->updateUniform("radius", m_radius);
    m_shader->updateUniform("darkRadius", m_darkRadius);
    m_shader->updateUniform("warp", m_warp);
    m_shader->updateUniform("dark", m_dark);
    m_shader->updateUniform("time", time);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_copy->getRenderTargets()[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    m_quad->draw();
    glActiveTexture(GL_TEXTURE0);
    /* the state the frame expects (as the anti-aliasing before this leaves it) */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}
