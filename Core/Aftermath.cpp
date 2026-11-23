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
static const float IMPACT = BlackHole::DEBRIS_ARRIVAL;                    /* when the moon's debris arrives */
static const float FRONT_SPEED = FRONT_DISTANCE / (IMPACT - FRONT_START);
/* silence while the hole forms and throws the moon out (no sound crosses
 * space): the first thing heard is the debris coming in, just before it hits */
static const float ROAR_START = IMPACT - 1.5f;
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
        "position": [0, 0, 0], "radius": 150, "height": 80, "direction": [0, -0.25, 1], "spread": 0.16,
        "speed": [260, 420], "gravity": -12, "startScale": 26, "growth": 34, "scaleJitter": 0.5,
        "blend": "alpha", "tint": [0.66, 0.52, 0.4], "brightness": 0.45, "fadeIn": 0.2})",
    /* torn-off bits: small dark chunks, heavier */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 320, "lifetimeMs": 1300,
        "position": [0, 0, 0], "radius": 130, "height": 60, "direction": [0, -0.4, 1], "spread": 0.22,
        "speed": [330, 520], "gravity": -140, "startScale": 4.0, "growth": 0, "scaleJitter": 0.6,
        "blend": "alpha", "tint": [0.16, 0.13, 0.11], "brightness": 0.05})",
    /* burning embers from the moon's debris */
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 650, "lifetimeMs": 1500,
        "position": [0, 0, 0], "radius": 170, "height": 110, "direction": [0, -0.35, 1], "spread": 0.25,
        "speed": [280, 460], "gravity": -30, "startScale": 3.4, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.2, 0.85, 0.3]})",
    /* spray torn off the water */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 160, "lifetimeMs": 1500,
        "position": [0, 0, 0], "radius": 160, "height": 6, "direction": [0, 0.12, 1], "spread": 0.18,
        "speed": [200, 330], "gravity": -40, "startScale": 10, "growth": 45, "scaleJitter": 0.4,
        "blend": "alpha", "tint": [0.55, 0.6, 0.66], "brightness": 0.3, "fadeIn": 0.15})",
    /* the shockwave's wall, approaching: a bank of dust ... */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 450, "lifetimeMs": 1100,
        "position": [0, 0, 0], "radius": 420, "height": 180, "direction": [0, -0.15, 1], "spread": 0.1,
        "speed": [480, 560], "gravity": 0, "startScale": 70, "growth": 90, "scaleJitter": 0.4,
        "blend": "alpha", "tint": [0.7, 0.52, 0.38], "brightness": 0.5, "fadeIn": 0.25})",
    /* ... lit by burning debris */
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 500, "lifetimeMs": 1100,
        "position": [0, 0, 0], "radius": 420, "height": 200, "direction": [0, -0.25, 1], "spread": 0.12,
        "speed": [480, 580], "gravity": -20, "startScale": 4.5, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.4, 0.9, 0.3]})",
    /* the moon's debris raining down out of the sky, from all over it (the
     * shell of it has passed the Earth and is all round): four streams of
     * burning fragments streaking down like meteors ... */
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 420, "lifetimeMs": 1000,
        "position": [0, 0, 0], "radius": 650, "height": 120, "direction": [0, 0, 1], "spread": 0.05,
        "speed": [650, 1000], "gravity": -200, "startScale": 9, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.8, 1.05, 0.35]})",
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 420, "lifetimeMs": 1000,
        "position": [0, 0, 0], "radius": 650, "height": 120, "direction": [0, 0, 1], "spread": 0.05,
        "speed": [650, 1000], "gravity": -200, "startScale": 9, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.8, 1.05, 0.35]})",
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 420, "lifetimeMs": 1000,
        "position": [0, 0, 0], "radius": 650, "height": 120, "direction": [0, 0, 1], "spread": 0.05,
        "speed": [650, 1000], "gravity": -200, "startScale": 9, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.8, 1.05, 0.35]})",
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 420, "lifetimeMs": 1000,
        "position": [0, 0, 0], "radius": 650, "height": 120, "direction": [0, 0, 1], "spread": 0.05,
        "speed": [650, 1000], "gravity": -200, "startScale": 9, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.8, 1.05, 0.35]})",
    /* ... and two of the smoke they trail */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 320, "lifetimeMs": 1300,
        "position": [0, 0, 0], "radius": 650, "height": 120, "direction": [0, 0, 1], "spread": 0.06,
        "speed": [450, 750], "gravity": 0, "startScale": 18, "growth": 40, "scaleJitter": 0.5,
        "blend": "alpha", "tint": [0.32, 0.26, 0.2], "brightness": 0.3})",
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 320, "lifetimeMs": 1300,
        "position": [0, 0, 0], "radius": 650, "height": 120, "direction": [0, 0, 1], "spread": 0.06,
        "speed": [450, 750], "gravity": 0, "startScale": 18, "growth": 40, "scaleJitter": 0.5,
        "blend": "alpha", "tint": [0.32, 0.26, 0.2], "brightness": 0.3})",
    /* the dust storm (dustOut only): thick billows rushing right past the camera */
    R"({"type": "spray", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 420, "lifetimeMs": 900,
        "position": [0, 0, 0], "radius": 110, "height": 70, "direction": [0, -0.2, 1], "spread": 0.2,
        "speed": [320, 480], "gravity": 0, "startScale": 45, "growth": 70, "scaleJitter": 0.5,
        "blend": "alpha", "tint": [0.6, 0.46, 0.33], "brightness": 0.4, "fadeIn": 0.15})",
    /* and some of it coming straight at the player, head-on out of the sky:
     * a narrow stream that opens up as it comes (two: from above where they
     * look, and from the hole) */
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 140, "lifetimeMs": 1300,
        "position": [0, 0, 0], "radius": 30, "height": 10, "direction": [0, 0, 1], "spread": 0.05,
        "speed": [500, 750], "gravity": -30, "startScale": 6, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.8, 1.05, 0.35]})",
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 140, "lifetimeMs": 1300,
        "position": [0, 0, 0], "radius": 30, "height": 10, "direction": [0, 0, 1], "spread": 0.05,
        "speed": [500, 750], "gravity": -30, "startScale": 6, "scaleJitter": 0.6,
        "blend": "additive", "tint": [2.8, 1.05, 0.35]})",
};
static const int DEBRIS_COUNT = sizeof(DEBRIS) / sizeof(DEBRIS[0]);
static const int WATER_SPRAY = 3;
static const int WALL_DUST = 4, WALL_EMBERS = 5, FALLING = 6, FALLING_COUNT = 6, STORM_DUST = 12, AT_PLAYER = 13;

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
    rm->setDust(0.0f, 0.0f);
    rm->setStormWater(0.0f, 0.0f);
    rm->setLightning(0.0f, 0.5f, 0.0f, 0.0f);
    audio->stop(m_windVoice);
    audio->stop(m_rumbleVoice);
    m_windVoice = m_rumbleVoice = 0;
    m_nextBoom = 0.0f;
    m_booms = 0;
    m_delayed.clear();
    m_lastBolt = -1;
    m_nextThud = 0.0f;
    m_thuds = 0;
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

void Aftermath::placeDebris(Camera *camera, const glm::vec3 &downwind, float strength, float wall, float frontAhead,
                            float dust, float falling, float t)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    /* the emitters' +z turned to blow downwind, from upwind of the camera */
    glm::vec3 z = glm::normalize(downwind);
    glm::vec3 x = glm::normalize(glm::cross(glm::vec3(0, 1, 0), z));
    glm::vec3 y = glm::cross(z, x);
    glm::mat3 rotation(x, y, z);
    glm::vec3 eye = camera->getPosition();
    /* above the camera, slanting down with the wind: it comes out of the
     * sky (from below, it looked like the ground was throwing it up) */
    glm::vec3 origin = eye - z * 170.0f + glm::vec3(0.0f, 80.0f, 0.0f);
    float waterHeight = rm->water != nullptr ? rm->water->getHeight() : 0.0f;

    bool show = strength > 0.005f || wall > 0.005f || falling > 0.005f;
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
            at = eye - z * (std::max(frontAhead, 0.0f) + WALL_DEPTH) + glm::vec3(0.0f, 60.0f, 0.0f);
            s = wall;
        } else if (i == STORM_DUST) {
            at = eye - z * 120.0f + glm::vec3(0.0f, 50.0f, 0.0f);
            s = dust;
        } else if (i >= AT_PLAYER) {
            /* head-on: from far up in the sky straight at the eye */
            glm::vec3 from = i == AT_PLAYER
                ? glm::normalize(camera->getForward() * glm::vec3(1, 0, 1) + glm::vec3(0.0f, 1.1f, 0.0f))
                : glm::normalize(m_holeDirection);
            from = glm::normalize(from + 0.08f * glm::vec3(std::sin(t * 0.7f + i), 0.0f, std::cos(t * 0.5f + i)));
            glm::vec3 fz = -from;
            glm::vec3 fx = glm::normalize(glm::cross(glm::vec3(0, 1, 0), fz));
            glm::vec3 fy = glm::cross(fz, fx);
            m_debris[i]->setOrigin(eye + from * 450.0f, glm::mat3(fx, fy, fz));
            m_debris[i]->setStrength(falling);
            continue;
        } else if (i >= FALLING && i < FALLING + FALLING_COUNT) {
            /* out of the sky, each stream from its own direction (the first
             * from the hole's), high and low, drifting round slowly */
            int k = i - FALLING;
            glm::vec3 hole = glm::normalize(m_holeDirection);
            float holeAzimuth = std::atan2(hole.z, hole.x);
            float azimuth = k == 0 ? holeAzimuth : holeAzimuth + k * 2.1f + 0.15f * std::sin(t * 0.3f + k);
            float elevation = k == 0 ? std::asin(glm::clamp(hole.y, 0.2f, 0.95f)) : 0.55f + 0.3f * hash(k * 3.1f);
            glm::vec3 from(std::cos(elevation) * std::cos(azimuth), std::sin(elevation), std::cos(elevation) * std::sin(azimuth));
            glm::vec3 fz = -from;
            glm::vec3 fx = glm::normalize(glm::cross(glm::vec3(0, 1, 0), fz));
            glm::vec3 fy = glm::cross(fz, fx);
            /* spawned on a wide disc high over the player, shifted back along
             * its slant so it lands all round them, wherever they are */
            const float HIGH = 360.0f;
            glm::vec3 across(fz.x, 0.0f, fz.z);
            glm::vec3 spawn = eye + glm::vec3(0.0f, HIGH, 0.0f) - across * (HIGH / std::max(-fz.y, 0.2f));
            m_debris[i]->setOrigin(spawn, glm::mat3(fx, fy, fz));
            m_debris[i]->setStrength(falling);
            continue;
        } else if (i == WATER_SPRAY) {
            /* spray only over water, near its surface */
            at.y = waterHeight + 1.0f;
            s *= rm->water != nullptr ? 1.0f - smoothstep(60.0f, 220.0f, eye.y - waterHeight) : 0.0f;
        }
        m_debris[i]->setOrigin(at, rotation);
        m_debris[i]->setStrength(m_rainOnly ? 0.0f : s);       /* the dust-out storm: only the matter raining */
    }
}

int Aftermath::play(const std::string &name, float volume, float pitch, bool loop)
{
    return AudioMaster::getInstance()->play(name, volume * m_gain, pitch, loop);
}

int Aftermath::play3D(const std::string &name, const glm::vec3 &position, float volume, float radius, float pitch,
                      bool loop)
{
    return AudioMaster::getInstance()->play3D(name, position, volume * m_gain, radius, pitch, loop);
}

void Aftermath::setVolume(int voice, float volume)
{
    AudioMaster::getInstance()->setVolume(voice, volume * m_gain);
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
        m_holeDirection = toHole;
        m_rainOnly = hole->dustOut > 0.0f;
        m_gain = hole->dustOut > 0.0f ? 1.8f : 1.0f;
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
    /* with dustOut the gale never settles: it thickens into a dust storm
     * that hides everything by dustOut seconds */
    bool dustOut = hole->dustOut > 0.0f;
    float settle = dustOut ? 1.0f : glm::mix(1.0f, 0.2f, smoothstep(14.0f, 32.0f, t));
    float storm = smoothstep(IMPACT, IMPACT + 0.5f, t) * settle * gusting;
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
    float debris = smoothstep(IMPACT - 0.3f, IMPACT + 0.3f, t) *
                   (dustOut ? 1.0f : glm::mix(1.0f, 0.12f, smoothstep(10.0f, 28.0f, t)));
    float dust = 0.0f;
    if (dustOut) {
        float end = std::max(hole->dustOut, IMPACT + 3.0f);
        dust = smoothstep(IMPACT + 0.5f, end, t);
        float veil = smoothstep(IMPACT + 2.0f, end, t);
        float cover = veil * veil;                       /* how much of the picture the dust hides */
        rm->setDust(dust, cover);
        /* lightning in the dust: none until it covers a good part of the
         * picture, then more and more often as it closes in (a chance every
         * 0.7 s); a flash flickers twice. Thunder follows, later the further away */
        float strikes = smoothstep(0.25f, 0.85f, cover);
        float flash = 0.0f, boltX = 0.5f, boltSeed = 0.0f, boltReach = 0.0f;
        const float SLOT = 0.7f;
        int slot = (int) std::floor(t / SLOT);
        for (int k = slot - 2; k <= slot; k++) {
            float start = k * SLOT + hash(k * 3.7f + 1.0f) * SLOT * 0.6f;
            if (strikes <= 0.0f || hash(k * 9.1f + 2.0f) > 0.75f * strikes) continue;
            float dt = t - start;
            if (dt < 0.0f) continue;
            if (k > m_lastBolt) {
                m_lastBolt = k;
                float delay = 0.25f + 2.2f * hash(k * 5.3f + 3.0f);
                const char *thunder[] = { "boom_1", "boom_2", "boom_3" };
                m_delayed.push_back({ t + delay, thunder[k % 3], glm::mix(1.0f, 0.45f, delay / 2.5f),
                                      0.8f + 0.35f * hash(k * 2.9f + 4.0f) });
            }
            float f = std::exp(-dt * 9.0f) + (dt > 0.13f ? 0.7f * std::exp(-(dt - 0.13f) * 7.0f) : 0.0f);
            if (f > flash) {
                flash = f;
                boltX = 0.15f + 0.7f * hash(k * 7.7f + 5.0f);
                boltSeed = hash(k * 1.3f + 6.0f) * 50.0f;
                boltReach = 0.45f + 0.55f * hash(k * 4.1f + 7.0f);
            }
        }
        rm->setLightning(std::min(flash, 1.0f), boltX, boltSeed, boltReach);
    }
    if (hole->stormWater > 0.0f) {
        float wet = smoothstep(IMPACT - 0.2f, IMPACT + 1.5f, t) * hole->stormWater;
        rm->setStormWater(wet * gusting, wet);
    }
    float wall = smoothstep(FRONT_START, FRONT_START + 0.6f, t) * (1.0f - smoothstep(IMPACT, IMPACT + 0.8f, t));
    glm::vec3 downwind(m_stormDirection.x, 0.0f, m_stormDirection.y);
    /* the debris arriving out of the sky, from the moment it reaches us */
    float falling = smoothstep(IMPACT - 0.4f, IMPACT + 0.4f, t) *
                    (hole->dustOut > 0.0f ? 1.0f : glm::mix(1.0f, 0.15f, smoothstep(8.0f, 28.0f, t)));
    placeDebris(camera, downwind, debris, wall, frontAhead, dust, falling, t);

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
    if (m_roarVoice == 0 && t >= ROAR_START) {
        m_roarVoice = play3D("storm_loop", eye - downwind * std::max(frontAhead, 0.0f), 0.0f, FRONT_DISTANCE * 1.25f,
                                    1.0f, true);
    }
    if (!m_impactPlayed && t >= IMPACT) {
        m_impactPlayed = true;
        play("blast", 1.0f, 0.75f);
        play("boom_1", 0.9f, 0.85f);
        play3D("gust_1", eye - downwind * 60.0f, 1.0f, 800.0f, 0.8f);
        m_sprayVoice = play("spray_loop", 0.0f, 1.0f, true);
    }
    if (!m_secondBoomPlayed && t >= IMPACT + 0.45f) {
        m_secondBoomPlayed = true;
        play("boom_2", 0.7f, 0.8f);
    }
    if (m_roarVoice != 0) {
        /* rides the front in, then is all around (the gale's own loudness) */
        audio->setPosition(m_roarVoice, eye - downwind * std::max(frontAhead, 0.0f));
        float roar = t < IMPACT ? 1.1f * smoothstep(ROAR_START, ROAR_START + 0.7f, t)
                                : 1.1f * std::max(storm, 1.0f - smoothstep(IMPACT, IMPACT + 0.5f, t));
        setVolume(m_roarVoice, roar);
    }
    if (m_sprayVoice != 0) setVolume(m_sprayVoice, 0.45f * storm + 0.35f * dust);
    /* with dustOut the storm only gets worse: everything louder, faster,
     * deeper, from the impact to the dust taking everything */
    float rage = hole->dustOut > 0.0f ? smoothstep(IMPACT, std::max(hole->dustOut, IMPACT + 3.0f), t) : 0.0f;
    if (hole->dustOut > 0.0f) {
        /* the dust storm: the wind's howl over the roar, thunder after the
         * lightning, debris thudding down all round */
        if (m_windVoice == 0 && dust > 0.01f) {
            m_windVoice = play("wind_loop", 0.0f, 0.8f, true);
        }
        if (m_windVoice != 0) setVolume(m_windVoice, 1.1f * dust);
        if (m_roarVoice != 0 && t >= IMPACT) setVolume(m_roarVoice, 1.1f * storm * (0.8f + 0.6f * rage) + 0.6f * dust);
        /* a deep rumble under the roar, rising */
        if (m_rumbleVoice == 0 && t >= IMPACT) m_rumbleVoice = play("storm_loop", 0.0f, 0.55f, true);
        if (m_rumbleVoice != 0) setVolume(m_rumbleVoice, 1.2f * rage);
        /* debris coming down further off: booms, more and more often, louder */
        if (t >= IMPACT + 1.0f && t >= m_nextBoom) {
            const char *booms[] = { "boom_1", "boom_2", "boom_3", "blast" };
            int b = m_booms % 4;
            play(booms[b], (b == 3 ? 0.35f : 0.5f) + 0.5f * rage, 0.6f + 0.3f * hash(m_booms * 3.7f + 9.0f));
            m_booms++;
            m_nextBoom = t + (2.8f - 2.2f * rage) * (0.6f + 0.8f * hash(m_booms * 5.9f));
        }
        for (size_t i = 0; i < m_delayed.size();) {
            if (t >= m_delayed[i].at) {
                play(m_delayed[i].sound, m_delayed[i].volume * (0.8f + 0.5f * rage), m_delayed[i].pitch);
                m_delayed.erase(m_delayed.begin() + i);
            } else {
                i++;
            }
        }
        if (dust > 0.05f && t >= m_nextThud) {
            float a = hash(m_thuds * 4.3f) * 6.2831853f, d = 40.0f + 90.0f * hash(m_thuds * 6.1f + 1.0f);
            play3D("land", eye + glm::vec3(std::cos(a) * d, -10.0f, std::sin(a) * d),
                          0.5f + 0.5f * hash(m_thuds * 8.9f + 2.0f), 300.0f, 0.5f + 0.3f * hash(m_thuds * 2.2f + 3.0f));
            m_thuds++;
            m_nextThud = t + (0.25f + 0.9f * hash(m_thuds * 3.3f)) / (0.3f + dust);
        }
    }
    while (t >= m_nextGust) {
        /* a gust rushing past, from upwind and to one side */
        float side = hash(m_gusts * 3.1f) * 2.0f - 1.0f;
        float volume = (0.55f + 0.45f * hash(m_gusts * 5.7f + 1.0f)) * glm::clamp(storm * 1.4f, 0.0f, 1.0f) *
                       (1.0f + 0.4f * rage);
        float pitch = 0.85f + 0.25f * hash(m_gusts * 7.3f + 2.0f);
        if (volume > 0.05f) {
            play3D("gust_" + std::to_string(1 + m_gusts % 3), eye - downwind * 90.0f + across * (side * 140.0f),
                          volume, 700.0f, pitch);
        }
        m_gusts++;
        /* gusts come often in the gale, rarely once it settles */
        m_nextGust += (1.2f + 2.6f * hash(m_gusts * 11.1f)) / glm::max(storm, 0.25f) * 0.5f / (1.0f + 2.0f * rage);
    }
    return quit;
}
