#include "Waking.h"
#include "Camera.h"
#include "FrameBuffer.h"
#include "Mesh.h"
#include "Shader.h"
#include "Timeline.h"

#include <cmath>
#include <GL/glew.h>

bool Waking::s_holding = false;

const std::vector<Aftermath::Parameter> &Waking::lanes()
{
    static const std::vector<Aftermath::Parameter> LANES = {
        { "eyesOpen", "Waking", 0.0f, 1.0f, 1.0f, "How open the eyes are (0 = shut, 1 = wide open): the lids close in from top and bottom." },
        { "blur",     "Waking", 0.0f, 1.0f, 0.0f, "How blurred the waking eyes see (1 = blurred and doubled, 0 = clear)." },
        { "getUp",    "Waking", 0.0f, 1.0f, 1.0f, "Getting up (0 = lying in bed, 0.5 = sitting up, 1 = standing where the player starts). Below 1 the player is held: no walking, no looking round." },
    };
    return LANES;
}

static void readPose(const rapidjson::Value &json, const char *key, glm::vec3 &position, float &yaw, float &pitch)
{
    if (!json.HasMember(key) || !json[key].IsObject()) return;
    const auto &pose = json[key];
    if (pose.HasMember("position") && pose["position"].IsArray() && pose["position"].Size() == 3) {
        for (int i = 0; i < 3; i++) position[i] = pose["position"][i].GetFloat();
    }
    if (pose.HasMember("yaw") && pose["yaw"].IsNumber()) yaw = pose["yaw"].GetFloat();
    if (pose.HasMember("pitch") && pose["pitch"].IsNumber()) pitch = pose["pitch"].GetFloat();
}

void Waking::load(const rapidjson::Value &map, const glm::vec3 &standing, float yaw, float pitch)
{
    m_has = map.HasMember("Waking") && map["Waking"].IsObject();
    if (!m_has) return;
    m_standing.position = standing;
    m_standing.yaw = yaw;
    m_standing.pitch = pitch;
    m_lying = m_sitting = m_standing;
    readPose(map["Waking"], "lying", m_lying.position, m_lying.yaw, m_lying.pitch);
    readPose(map["Waking"], "sitting", m_sitting.position, m_sitting.yaw, m_sitting.pitch);
}

void Waking::readTimeline(const Timeline &timeline)
{
    m_eyesOpen = timeline.get("eyesOpen", 1.0f);
    m_blur = timeline.get("blur", 0.0f);
    m_getUp = timeline.get("getUp", 1.0f);
    s_holding = m_has && m_getUp < 1.0f;
}

/* (the shorter way round) */
static float mixAngle(float a, float b, float t)
{
    float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
    return a + d * t;
}

void Waking::applyCamera(Camera *camera, float time)
{
    if (!m_has || m_getUp >= 1.0f) {
        if (m_wasHolding) {
            /* up: the player's own view from here, as the scene left it */
            camera->setRotation(glm::radians(m_standing.pitch), glm::radians(m_standing.yaw), 0.0f);
            m_wasHolding = false;
        }
        return;
    }
    m_wasHolding = true;
    float g = glm::clamp(m_getUp, 0.0f, 1.0f);
    const Pose &from = g < 0.5f ? m_lying : m_sitting;
    const Pose &to = g < 0.5f ? m_sitting : m_standing;
    float t = glm::smoothstep(0.0f, 1.0f, g < 0.5f ? g * 2.0f : g * 2.0f - 1.0f);
    glm::vec3 position = glm::mix(from.position, to.position, t);
    float yaw = mixAngle(from.yaw, to.yaw, t);
    float pitch = glm::mix(from.pitch, to.pitch, t);
    /* breathing, slow and heavy while still lying down */
    float lying = 1.0f - glm::clamp(g * 2.0f, 0.0f, 1.0f);
    position.y += std::sin(time * 1.4f) * 0.35f * lying;
    pitch += std::sin(time * 1.4f + 0.8f) * 0.6f * lying;
    camera->setPosition(position);
    camera->setRotation(glm::radians(pitch), glm::radians(yaw), 0.0f);
}

void Waking::render(FrameBuffer &picture, int width, int height, float time)
{
    if (!m_has || (m_eyesOpen >= 0.999f && m_blur <= 0.001f)) return;
    if (m_shader == nullptr) {
        m_copy = new FrameBuffer(width, height, 1);
        m_shader = new Shader("res/shaders/waking.json");
        m_shader->updateUniform("pictureSampler", 0);
        m_quad = Mesh::getRectangle();
    }
    /* the picture as it is, to read from while drawing over it */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, picture.getFrameBufferObject());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_copy->getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    /* its mipmaps: the picture at half, a quarter, an eighth... size. The blur reads a smaller one
     * the more blurred, blended smoothly between them: a few reads a pixel, whatever the strength */
    glBindTexture(GL_TEXTURE_2D, m_copy->getRenderTargets()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    picture.bindSingleRenderTarget(0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_shader->bind();
    m_shader->updateUniform("eyesOpen", m_eyesOpen);
    m_shader->updateUniform("blur", m_blur);
    m_shader->updateUniform("time", time);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_copy->getRenderTargets()[0]);
    m_quad->draw();
    /* the state the frame expects (as the anti-aliasing before this leaves it): left off, the next
     * frame's scene was drawn without its depth test, a wrong picture */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}
