#include "Aftermath.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "AudioMaster.h"
#include "BlackHole.h"
#include "Camera.h"
#include "ParticleEmitter.h"
#include "RenderingMaster.h"
#include "WaterRenderer.h"
#include "rapidjson/document.h"

/* the shockwave: launched from the horizon on the hole's side soon after
 * the hole forms, it sweeps over the land as a wall of dust and embers;
 * its roar grows with its nearness, and the blast, the gale and the shake
 * come the moment it reaches the player (seconds, world units) */
static const float FRONT_START = 0.6f;
static const float FRONT_DISTANCE = 2400.0f;
static const float FRONT_SPEED = 850.0f;
static const float IMPACT = FRONT_START + FRONT_DISTANCE / FRONT_SPEED;   /* ~3.4 s */
/* how far behind its leading edge the wall's emitters sit (radius + flight) */
static const float WALL_DEPTH = 420.0f + 520.0f * 1.1f;

static float smoothstep(float a, float b, float x)
{
    float t = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* a repeatable random number in [0, 1) */
static float hash(float n)
{
    float x = std::sin(n * 12.9898f) * 43758.5453f;
    return x - std::floor(x);
}

/* what the storm flings at the player: emitted upwind of the camera,
 * launched downwind along +z of the emitter (turned each frame) */
static const char *DEBRIS[] = {
    /* dust and grit: big soft brown puffs */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 320, "lifetimeMs": 1700,
        "position": [0, 0, 0], "radius": 150, "height": 80, "direction": [0, -0.04, 1], "spread": 0.16,
        "speed": [260, 420], "gravity": -12, "startScale": 26, "growth": 34, "scaleJitter": 0.5,
        "blend": "alpha", "tint": [0.66, 0.52, 0.4], "brightness": 0.45, "fadeIn": 0.2})",
    /* torn-off bits: small dark chunks, heavier */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 320, "lifetimeMs": 1300,
        "position": [0, 0, 0], "radius": 130, "height": 60, "direction": [0, 0.08, 1], "spread": 0.22,
        "speed": [330, 520], "gravity": -140, "startScale": 4.0, "growth": 0, "scaleJitter": 0.6,
        "blend": "alpha", "tint": [0.16, 0.13, 0.11], "brightness": 0.05})",
    /* burning embers from the moon's debris */
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 650, "lifetimeMs": 1500,
        "position": [0, 0, 0], "radius": 170, "height": 110, "direction": [0, -0.02, 1], "spread": 0.25,
        "speed": [280, 460], "gravity": -30, "startScale": 3.4, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.2, 0.85, 0.3]})",
    /* spray torn off the water */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 160, "lifetimeMs": 1500,
        "position": [0, 0, 0], "radius": 160, "height": 6, "direction": [0, 0.12, 1], "spread": 0.18,
        "speed": [200, 330], "gravity": -40, "startScale": 10, "growth": 45, "scaleJitter": 0.4,
        "blend": "alpha", "tint": [0.55, 0.6, 0.66], "brightness": 0.3, "fadeIn": 0.15})",
    /* the shockwave's wall, approaching: a bank of dust ... */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 450, "lifetimeMs": 1100,
        "position": [0, 0, 0], "radius": 420, "height": 180, "direction": [0, 0.06, 1], "spread": 0.1,
        "speed": [480, 560], "gravity": 0, "startScale": 70, "growth": 90, "scaleJitter": 0.4,
        "blend": "alpha", "tint": [0.7, 0.52, 0.38], "brightness": 0.5, "fadeIn": 0.25})",
    /* ... lit by burning debris */
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 500, "lifetimeMs": 1100,
        "position": [0, 0, 0], "radius": 420, "height": 200, "direction": [0, 0.02, 1], "spread": 0.12,
        "speed": [480, 580], "gravity": -20, "startScale": 4.5, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.4, 0.9, 0.3]})",
};
static const int DEBRIS_COUNT = sizeof(DEBRIS) / sizeof(DEBRIS[0]);
static const int WATER_SPRAY = 3;
static const int WALL_DUST = 4, WALL_EMBERS = 5;

Aftermath::~Aftermath()
{
    /* while shown, the emitters are in RenderingMaster's list, which deletes
     * them; otherwise they are ours */
    if (!m_debrisShown) {
        for (ParticleEmitter *emitter : m_debris) delete emitter;
    }
}

void Aftermath::reset()
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    AudioMaster *audio = AudioMaster::getInstance();
    m_active = false;
    m_impactPlayed = m_secondBoomPlayed = false;
    m_gusts = 0;
    audio->stop(m_roarVoice);
    audio->stop(m_sprayVoice);
    m_roarVoice = m_sprayVoice = 0;
    rm->setStorm(0.0f, 0.0f);
    rm->setStormDirection(m_stormDirection, 0.0f);
    rm->getCamera()->setShake(glm::vec3(0.0f));
    rm->setFadeOut(0.0f);
    if (m_masterVolume >= 0.0f) {
        audio->setMasterVolume(m_masterVolume);
        m_masterVolume = -1.0f;
    }
    if (m_debrisShown) {
        for (ParticleEmitter *emitter : m_debris) rm->removeParticleEmitter(emitter);
        m_debrisShown = false;
    }
}

void Aftermath::startDebris()
{
    if (!m_debris.empty()) {
        return;
    }
    for (int i = 0; i < DEBRIS_COUNT; i++) {
        rapidjson::Document json;
        json.Parse(DEBRIS[i]);
        m_debris.push_back(new ParticleEmitter(json));
    }
}

void Aftermath::placeDebris(Camera *camera, const glm::vec3 &downwind, float strength, float wall, float frontAhead)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    /* the emitters' +z turned to blow downwind, from upwind of the camera */
    glm::vec3 z = glm::normalize(downwind);
    glm::vec3 x = glm::normalize(glm::cross(glm::vec3(0, 1, 0), z));
    glm::vec3 y = glm::cross(z, x);
    glm::mat3 rotation(x, y, z);
    glm::vec3 eye = camera->getPosition();
    glm::vec3 origin = eye - z * 170.0f + glm::vec3(0.0f, -30.0f, 0.0f);
    float waterHeight = rm->water != nullptr ? rm->water->getHeight() : 0.0f;

    bool show = strength > 0.005f || wall > 0.005f;
    if (show != m_debrisShown) {
        if (show) {
            startDebris();
            for (ParticleEmitter *emitter : m_debris) {
                emitter->setOrigin(eye - z * (std::max(frontAhead, 0.0f) + WALL_DEPTH), rotation);
                emitter->restart();   /* emitted from here, not from where they last were */
                rm->addParticleEmitter(emitter);
            }
        } else {
            for (ParticleEmitter *emitter : m_debris) rm->removeParticleEmitter(emitter);
        }
        m_debrisShown = show;
    }
    if (!show) {
        return;
    }
    for (int i = 0; i < DEBRIS_COUNT; i++) {
        glm::vec3 at = origin;
        float s = strength;
        if (i == WALL_DUST || i == WALL_EMBERS) {
            /* the wall's leading edge is the front */
            at = eye - z * (std::max(frontAhead, 0.0f) + WALL_DEPTH) + glm::vec3(0.0f, -40.0f, 0.0f);
            s = wall;
        } else if (i == WATER_SPRAY) {
            /* spray only over water, near its surface */
            at.y = waterHeight + 1.0f;
            s *= rm->water != nullptr ? 1.0f - smoothstep(60.0f, 220.0f, eye.y - waterHeight) : 0.0f;
        }
        m_debris[i]->setOrigin(at, rotation);
        m_debris[i]->setStrength(s);
    }
}

bool Aftermath::update(Camera *camera)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    if (camera == nullptr) {
        return false;
    }
    BlackHole *hole = nullptr;
    for (BlackHole *h : rm->blackHoles) {
        if (h->sky && h->kugelblitz) { hole = h; break; }
    }
    if (hole == nullptr) {
        if (m_active) reset();
        return false;
    }

    /* Restart / Reset / dragging the collapse back: undone */
    if (m_active && (hole->collapse < 1.0f || hole->playTime < m_lastPlayTime)) {
        reset();
    }
    /* the hole forms while we watch (a map loaded finished stays quiet) */
    if (!m_active && hole->collapse >= 1.0f && m_lastCollapse >= 0.0f && m_lastCollapse < 1.0f) {
        m_active = true;
        m_completedAt = hole->playTime;
        m_impactPlayed = m_secondBoomPlayed = false;
        m_gusts = 0;
        m_nextGust = IMPACT + 0.4f;
        /* the storm blows from the hole, along the ground */
        glm::vec3 toHole = hole->direction;
        glm::vec2 flat(toHole.x, toHole.z);
        m_stormDirection = glm::length(flat) > 1e-3f ? -glm::normalize(flat) : glm::vec2(1.0f, 0.0f);
    }
    m_lastCollapse = hole->collapse;
    m_lastPlayTime = hole->playTime;
    if (!m_active) {
        return false;
    }

    AudioMaster *audio = AudioMaster::getInstance();
    float t = hole->playTime - m_completedAt;

    /* the calm before, then the storm: a gale that gusts, easing into a stiff wind */
    float calm = smoothstep(0.3f, 1.2f, t) * (1.0f - smoothstep(IMPACT - 0.3f, IMPACT, t));
    float frontAhead = FRONT_DISTANCE - FRONT_SPEED * (t - FRONT_START);   /* upwind of the player */
    float gusting = 0.82f + 0.18f * std::sin(t * 1.9f) * std::sin(t * 0.7f + 1.0f);
    float storm = smoothstep(IMPACT, IMPACT + 0.5f, t) * glm::mix(1.0f, 0.2f, smoothstep(14.0f, 32.0f, t)) * gusting;
    float turn = smoothstep(0.3f, IMPACT - 0.5f, t);
    rm->setStorm(storm, calm);
    rm->setStormDirection(m_stormDirection, turn);

    /* the shockwave shakes the camera hard, then the gale keeps it trembling */
    float hit = t >= IMPACT ? std::exp(-(t - IMPACT) * 1.1f) : 0.0f;
    float amplitude = 0.035f * hit + 0.006f * storm;
    glm::vec3 shake(std::sin(t * 37.0f) * 0.6f + std::sin(t * 23.0f + 1.0f) * 0.4f,
                    std::sin(t * 29.0f + 2.0f) * 0.6f + std::sin(t * 17.0f + 4.0f) * 0.4f,
                    std::sin(t * 31.0f + 3.0f) * 0.5f);
    camera->setShake(shake * amplitude);

    /* debris, from the impact; thinning out as it settles */
    float debris = smoothstep(IMPACT - 0.3f, IMPACT + 0.3f, t) * glm::mix(1.0f, 0.12f, smoothstep(10.0f, 28.0f, t));
    float wall = smoothstep(FRONT_START, FRONT_START + 0.6f, t) * (1.0f - smoothstep(IMPACT, IMPACT + 0.8f, t));
    glm::vec3 downwind(m_stormDirection.x, 0.0f, m_stormDirection.y);
    placeDebris(camera, downwind, debris, wall, frontAhead);

    /* the ending: fade to black and silence, then close */
    bool quit = false;
    if (hole->quitAfter > 0.0f) {
        float fade = smoothstep(hole->quitAfter - 2.0f, hole->quitAfter, t);
        rm->setFadeOut(fade);
        if (fade > 0.0f) {
            if (m_masterVolume < 0.0f) m_masterVolume = audio->getMasterVolume();
            audio->setMasterVolume(m_masterVolume * (1.0f - fade));
        }
        quit = t >= hole->quitAfter;
    }

    /* sound: the shockwave, the roar of the gale, the spray and the gusts */
    if (!audio->isAvailable()) {
        return quit;
    }
    glm::vec3 eye = camera->getPosition();
    glm::vec3 across = glm::normalize(glm::cross(glm::vec3(0, 1, 0), downwind));
    /* the front's roar, from where the front is: louder as it nears */
    if (m_roarVoice == 0 && t >= FRONT_START) {
        m_roarVoice = audio->play3D("storm_loop", eye - downwind * std::max(frontAhead, 0.0f), 1.1f, FRONT_DISTANCE * 1.25f,
                                    1.0f, true);
    }
    if (!m_impactPlayed && t >= IMPACT) {
        m_impactPlayed = true;
        audio->play("blast", 1.0f, 0.75f);
        audio->play("boom_1", 0.9f, 0.85f);
        audio->play3D("gust_1", eye - downwind * 60.0f, 1.0f, 800.0f, 0.8f);
        m_sprayVoice = audio->play("spray_loop", 0.0f, 1.0f, true);
    }
    if (!m_secondBoomPlayed && t >= IMPACT + 0.45f) {
        m_secondBoomPlayed = true;
        audio->play("boom_2", 0.7f, 0.8f);
    }
    if (m_roarVoice != 0) {
        /* rides the front in, then is all around (the gale's own loudness) */
        audio->setPosition(m_roarVoice, eye - downwind * std::max(frontAhead, 0.0f));
        float roar = t < IMPACT ? 1.1f : 1.1f * std::max(storm, 1.0f - smoothstep(IMPACT, IMPACT + 0.5f, t));
        audio->setVolume(m_roarVoice, roar);
    }
    if (m_sprayVoice != 0) audio->setVolume(m_sprayVoice, 0.45f * storm);
    while (t >= m_nextGust) {
        /* a gust rushing past, from upwind and to one side */
        float side = hash(m_gusts * 3.1f) * 2.0f - 1.0f;
        float volume = (0.55f + 0.45f * hash(m_gusts * 5.7f + 1.0f)) * glm::clamp(storm * 1.4f, 0.0f, 1.0f);
        float pitch = 0.85f + 0.25f * hash(m_gusts * 7.3f + 2.0f);
        if (volume > 0.05f) {
            audio->play3D("gust_" + std::to_string(1 + m_gusts % 3), eye - downwind * 90.0f + across * (side * 140.0f),
                          volume, 700.0f, pitch);
        }
        m_gusts++;
        /* gusts come often in the gale, rarely once it settles */
        m_nextGust += (1.2f + 2.6f * hash(m_gusts * 11.1f)) / glm::max(storm, 0.25f) * 0.5f;
    }
    return quit;
}
