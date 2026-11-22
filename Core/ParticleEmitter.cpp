#include "ParticleEmitter.h"

#include <algorithm>
#include <cstring>

#include "Camera.h"
#include "Shader.h"
#include "Texture.h"

/* per instance: position.xyz + scale, normalized age (0..1000) */
static const int FLOATS_PER_INSTANCE = 5;
/* the shader animates/fades over liveForInMs; ages are normalized to this */
static const int NORMALIZED_LIFETIME = 1000;

static float getFloat(const rapidjson::Value &json, const char *key, float def) {
    return json.HasMember(key) && json[key].IsNumber() ? json[key].GetFloat() : def;
}

static glm::vec3 getVec3(const rapidjson::Value &json, const char *key, const glm::vec3 &def) {
    if (!json.HasMember(key) || !json[key].IsArray() || json[key].Size() < 3) {
        return def;
    }
    return glm::vec3(json[key][0].GetFloat(), json[key][1].GetFloat(), json[key][2].GetFloat());
}

ParticleEmitter::ParticleEmitter(const rapidjson::Value &json)
{
    std::string type = json.HasMember("type") ? json["type"].GetString() : "smoke";
    m_type = type == "spray" ? SPRAY : (type == "drift" ? DRIFT : (type == "glow" ? GLOW : SMOKE));

    m_position = getVec3(json, "position", glm::vec3(0));
    m_direction = getVec3(json, "direction", glm::vec3(0, 1, 0));
    if (glm::length(m_direction) < 1e-4f) {
        m_direction = glm::vec3(0, 1, 0);
    }
    m_direction = glm::normalize(m_direction);
    m_localDirection = m_direction;
    m_tint = getVec3(json, "tint", glm::vec3(1));

    m_radius = getFloat(json, "radius", 5.0f);
    m_height = getFloat(json, "height", 0.0f);
    m_spread = getFloat(json, "spread", 0.2f);
    m_speedMin = m_speedMax = 50.0f;
    if (json.HasMember("speed") && json["speed"].IsArray() && json["speed"].Size() >= 2) {
        m_speedMin = json["speed"][0].GetFloat();
        m_speedMax = json["speed"][1].GetFloat();
    } else {
        m_speedMin = m_speedMax = getFloat(json, "speed", 50.0f);
    }
    m_gravity = getFloat(json, "gravity", m_type == SMOKE ? -10.0f : (m_type == SPRAY ? -60.0f : 0.0f));
    m_buoyancy = getFloat(json, "buoyancy", m_type == SMOKE ? 10.0f : 0.0f);
    m_startScale = getFloat(json, "startScale", 10.0f);
    m_growth = getFloat(json, "growth", m_type == SMOKE ? 50.0f : 0.0f);
    m_scaleJitter = getFloat(json, "scaleJitter", 0.3f);
    m_lifetimeMs = std::max(50.0f, getFloat(json, "lifetimeMs", 3000.0f));
    m_brightness = getFloat(json, "brightness", m_type == SMOKE ? 0.25f : 0.0f);
    m_fadeIn = getFloat(json, "fadeIn", m_type == DRIFT ? 0.3f : 0.0f);
    m_wander = getFloat(json, "wander", 30.0f);
    m_additive = json.HasMember("blend") && std::string(json["blend"].GetString()) == "additive";
    m_atlasTopDown = json.HasMember("atlasTopDown") && json["atlasTopDown"].GetBool();

    int cols = 1, rows = 1;
    if (json.HasMember("atlas") && json["atlas"].IsArray() && json["atlas"].Size() >= 2) {
        cols = std::max(1, json["atlas"][0].GetInt());
        rows = std::max(1, json["atlas"][1].GetInt());
    }
    std::string texturePath = json.HasMember("texture") ? json["texture"].GetString() : "res/textures/smoke.png";
    m_texture = new Texture(texturePath, 0, cols, rows);

    int count = json.HasMember("count") ? std::max(1, json["count"].GetInt()) : 100;
    std::vector<glm::vec3> glowPositions;
    if (m_type == GLOW) {
        if (json.HasMember("positions") && json["positions"].IsArray()) {
            for (auto const &p : json["positions"].GetArray()) {
                glowPositions.push_back(glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat()));
            }
        } else {
            glowPositions.push_back(m_position);
        }
        count = glowPositions.size();
    }

    /* deterministic per emitter (stable benchmarks / screenshots) */
    m_rngState = 2166136261u;
    for (int i = 0; i < 3; i++) {
        unsigned int bits;
        memcpy(&bits, &m_position[i], sizeof(bits));
        m_rngState = (m_rngState ^ bits) * 16777619u;
    }
    if (m_rngState == 0) m_rngState = 1;

    /* start staggered and pre-warm so the effect is in steady state on the
     * first frame instead of every particle being born at once */
    m_particles.resize(count);
    for (Particle &p : m_particles) {
        respawn(p);
        p.ageMs = -rand01() * p.lifeMs;
    }
    if (m_type == GLOW) {
        for (int i = 0; i < count; i++) {
            m_particles[i].position = glowPositions[i];
            m_particles[i].previousPosition = glowPositions[i];
            m_glowOffsets.push_back(glowPositions[i] - m_position);
            m_particles[i].velocity = glm::vec3(0);
            m_particles[i].scale = m_startScale;
            m_particles[i].ageMs = 0;
            m_particles[i].lifeMs = 1.0f;
        }
    }
    const float warmupStep = 33.0f;
    for (float t = 0; m_type != GLOW && t < m_lifetimeMs; t += warmupStep) {
        for (Particle &p : m_particles) {
            simulate(p, warmupStep);
        }
    }

    m_instanceData.resize(count * FLOATS_PER_INSTANCE);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * m_instanceData.size(), NULL, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_INSTANCE * sizeof(float), (void *) 0);
    glVertexAttribDivisor(0, 1);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, FLOATS_PER_INSTANCE * sizeof(float), (void *) (4 * sizeof(float)));
    glVertexAttribDivisor(1, 1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

ParticleEmitter::~ParticleEmitter()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
    delete m_texture;
}

Shader &ParticleEmitter::getShader()
{
    static Shader shader("res/shaders/particleRenderShader.json");
    return shader;
}

float ParticleEmitter::rand01()
{
    /* xorshift32 */
    m_rngState ^= m_rngState << 13;
    m_rngState ^= m_rngState >> 17;
    m_rngState ^= m_rngState << 5;
    return (m_rngState & 0xFFFFFF) / float(0x1000000);
}

glm::vec3 ParticleEmitter::randomInUnitSphere()
{
    for (;;) {
        glm::vec3 v(rand01() * 2.0f - 1.0f, rand01() * 2.0f - 1.0f, rand01() * 2.0f - 1.0f);
        if (glm::dot(v, v) <= 1.0f) {
            return v;
        }
    }
}

void ParticleEmitter::respawn(Particle &p)
{
    glm::vec3 offset;
    if (m_type == DRIFT) {
        /* anywhere in a flattened ellipsoid */
        glm::vec3 r = randomInUnitSphere();
        offset = glm::vec3(r.x * m_radius, r.y * std::max(m_height, 1.0f), r.z * m_radius);
    } else {
        /* on a disc of radius m_radius */
        float a = rand01() * 6.2831853f;
        float d = glm::sqrt(rand01()) * m_radius;
        offset = glm::vec3(glm::cos(a) * d, rand01() * m_height, glm::sin(a) * d);
    }
    p.position = m_position + offset;
    p.previousPosition = p.position;

    glm::vec3 dir = glm::normalize(m_direction + randomInUnitSphere() * m_spread);
    float speed = m_speedMin + (m_speedMax - m_speedMin) * rand01();
    p.velocity = (m_type == DRIFT) ? randomInUnitSphere() * speed : dir * speed;

    p.scale = m_startScale * (1.0f - m_scaleJitter * rand01());
    p.lifeMs = m_lifetimeMs * (0.7f + 0.3f * rand01());
    p.ageMs = 0;
    p.distanceToCamera = 0;
}

void ParticleEmitter::simulate(Particle &p, float dtMs)
{
    if (m_type == GLOW) {
        return; /* static, always at full strength */
    }
    if (p.ageMs < 0) {
        /* not born yet */
        p.ageMs += dtMs;
        if (p.ageMs < 0) {
            return;
        }
        float leftover = p.ageMs;
        respawn(p);
        p.ageMs = 0;
        dtMs = leftover;
    }

    p.ageMs += dtMs;
    if (p.ageMs >= p.lifeMs) {
        respawn(p);
        return;
    }

    float dt = dtMs / 1000.0f;
    switch (m_type) {
    case SMOKE:
        /* same integration as SmokeParticle */
        p.scale += m_growth * dt;
        p.position += p.velocity * dt +
                      glm::vec3(0, m_gravity + p.scale * m_buoyancy, 0) * (dt * dt / 2.0f);
        p.velocity.y += m_gravity * dt;
        break;
    case SPRAY:
        /* ballistic, as in World2's particle system */
        p.position += p.velocity * dt + glm::vec3(0, m_gravity, 0) * (dt * dt / 2.0f);
        p.velocity.y += m_gravity * dt;
        p.scale = std::max(0.0f, p.scale + m_growth * dt);
        break;
    case GLOW:
        break;
    case DRIFT: {
        /* random wandering, gently pulled back into the volume */
        p.velocity += randomInUnitSphere() * (m_wander * dt * 4.0f);
        glm::vec3 toCenter = m_position - p.position;
        toCenter.y *= m_radius / std::max(m_height, 1.0f);
        if (glm::length(toCenter) > m_radius) {
            p.velocity += glm::normalize(toCenter) * (m_wander * dt);
        }
        p.velocity *= std::max(0.0f, 1.0f - 0.8f * dt);
        p.position += p.velocity * dt;
        break;
    }
    }
}

void ParticleEmitter::setOrigin(const glm::vec3 &position, const glm::mat3 &rotation)
{
    m_position = position;
    m_direction = glm::normalize(rotation * m_localDirection);
    if (m_type == GLOW) {
        for (size_t i = 0; i < m_particles.size() && i < m_glowOffsets.size(); i++) {
            m_particles[i].position = position + rotation * m_glowOffsets[i];
        }
    }
}

void ParticleEmitter::restart()
{
    if (m_type == GLOW) {
        return;
    }
    for (Particle &p : m_particles) {
        respawn(p);
        p.ageMs = -rand01() * p.lifeMs;
    }
}

void ParticleEmitter::update(Camera &camera, float dtMs)
{
    const glm::vec3 &cameraPosition = camera.getPosition();

    for (Particle &p : m_particles) {
        p.previousPosition = p.position;
        simulate(p, dtMs);
        glm::vec3 d = p.position - cameraPosition;
        p.distanceToCamera = glm::dot(d, d);
    }

    /* alpha-blended particles must be drawn back to front */
    if (!m_additive) {
        std::sort(m_particles.begin(), m_particles.end(),
                  [](const Particle &a, const Particle &b) {
                      return a.distanceToCamera > b.distanceToCamera;
                  });
    }
}

void ParticleEmitter::draw(const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix,
                           int renderWidth, int renderHeight, float alpha)
{
    int alive = 0;
    float *out = m_instanceData.data();
    for (const Particle &p : m_particles) {
        if (p.ageMs < 0) {
            continue;
        }
        /* positions are blended between the last two ticks */
        glm::vec3 position = glm::mix(p.previousPosition, p.position, alpha);
        *out++ = position.x;
        *out++ = position.y;
        *out++ = position.z;
        *out++ = p.scale;
        *out++ = (p.ageMs / p.lifeMs) * NORMALIZED_LIFETIME;
        alive++;
    }
    if (alive == 0) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, alive * FLOATS_PER_INSTANCE * sizeof(float), m_instanceData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    Shader &shader = getShader();
    int lifetime = NORMALIZED_LIFETIME;
    int additive = m_additive ? 1 : 0;
    int topDown = m_atlasTopDown ? 1 : 0;
    shader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    shader.updateUniform("viewMatrix", (void *) &viewMatrix);
    shader.updateUniform("particleSampler", 0);
    shader.updateUniform("depthSampler", 3);
    shader.updateUniform("screenWidth", renderWidth);
    shader.updateUniform("screenHeight", renderHeight);
    shader.updateUniform("liveForInMs", lifetime);
    shader.updateUniform("totalNumOfSubTxts", m_texture->getTotalNumOfSubTxts());
    shader.updateUniform("numOfSubTxtsW", m_texture->getNumOfSubTxtsW());
    shader.updateUniform("numOfSubTxtsH", m_texture->getNumOfSubTxtsH());
    shader.updateUniform("subWidth", m_texture->getSubWidth());
    shader.updateUniform("subHeight", m_texture->getSubHeight());
    shader.updateUniform("tint", (void *) &m_tint);
    shader.updateUniform("brightnessAdd", m_brightness);
    shader.updateUniform("additive", additive);
    shader.updateUniform("fadeIn", m_fadeIn);
    shader.updateUniform("atlasTopDown", topDown);

    glEnable(GL_BLEND);
    if (m_additive) {
        glBlendFunc(GL_ONE, GL_ONE);
    } else {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDepthMask(GL_FALSE);

    glBindVertexArray(m_vao);
    shader.bind();
    m_texture->use(0);
    glDrawArraysInstanced(GL_POINTS, 0, 1, alive);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}
