#include "Aftermath.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "AudioMaster.h"
#include "BlackHole.h"
#include "Camera.h"
#include "ParticleEmitter.h"
#include "PointLight.h"
#include "RenderingMaster.h"
#include "WaterRenderer.h"
#include "Timeline.h"
#include "rapidjson/document.h"

/* how far behind its leading edge the shockwave wall's emitters sit (radius + flight) */
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

/* a fire where debris came down: flames, sparks, a column of smoke (as the
 * lagoon's campfire, bigger); placed with setOrigin (y: offsets from the ground) */
static const char *FIRE[] = {
    R"({"type": "smoke", "texture": "res/textures/fire-texture-atlas.jpg", "heat": 1.0, "atlas": [4, 4],
        "atlasTopDown": true, "count": 60, "lifetimeMs": 900, "position": [0, 0, 0], "radius": 9,
        "direction": [0, 1, 0], "spread": 0.2, "speed": [18, 32], "gravity": 0, "buoyancy": 0.4,
        "startScale": 22, "growth": -8, "blend": "additive", "tint": [1.0, 0.8, 0.6], "brightness": 0.0})",
    R"({"type": "spray", "texture": "res/textures/glowWhite.png", "count": 60, "lifetimeMs": 1800,
        "position": [0, 0, 0], "radius": 8, "direction": [0, 1, 0], "spread": 0.5, "speed": [30, 70],
        "gravity": -8, "startScale": 1.8, "blend": "additive", "tint": [1.0, 0.5, 0.18]})",
    R"({"type": "smoke", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 40, "lifetimeMs": 5000,
        "position": [0, 0, 0], "radius": 6, "direction": [0.2, 1, 0], "spread": 0.2, "speed": [14, 24],
        "gravity": -2, "buoyancy": 0.5, "startScale": 18, "growth": 22, "blend": "alpha",
        "tint": [0.22, 0.17, 0.13], "brightness": 0.35, "fadeIn": 0.35})",
};
static const float FIRE_HEIGHT[] = { 1.0f, 3.0f, 30.0f };
static const int WATER_SPRAY = 3;
static const int WALL_DUST = 4, WALL_EMBERS = 5, FALLING = 6, FALLING_COUNT = 6, STORM_DUST = 12, AT_PLAYER = 13;

Aftermath::~Aftermath()
{
    /* while shown, the emitters are in RenderingMaster's list, which deletes
     * them (and the fires' emitters and lights); otherwise they are ours */
    if (!m_debrisShown) {
        for (ParticleEmitter *emitter : m_debris) delete emitter;
    }
}

int Aftermath::play(const std::string &name, float volume, float pitch, bool loop)
{
    return AudioMaster::getInstance()->play(name, volume * m_gain, pitch, loop);
}

int Aftermath::play3D(const std::string &name, const glm::vec3 &position, float volume, float radius, float pitch)
{
    return AudioMaster::getInstance()->play3D(name, position, volume * m_gain, radius, pitch, false);
}

void Aftermath::reset()
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    AudioMaster *audio = AudioMaster::getInstance();
    m_active = false;
    rm->setStorm(0.0f, 0.0f);
    rm->setStormDirection(m_stormDirection, 0.0f);
    rm->getCamera()->setShake(glm::vec3(0.0f));
    rm->setFadeOut(0.0f);
    rm->setDust(0.0f, 0.0f);
    rm->setStormWater(0.0f, 0.0f);
    rm->setLightning(0.0f, 0.5f, 0.0f, 0.0f);
    rm->setDebrisArrival(BlackHole::DEBRIS_ARRIVAL);
    putOutFires();
    m_gusts = m_booms = m_thuds = 0;
    m_lastBolt = -1;
    m_nextGust = m_nextBoom = m_nextThud = -1.0f;
    m_delayed.clear();
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

void Aftermath::placeDebris(Camera *camera, const glm::vec3 &downwind, float ground, float wall, float frontDistance,
                            float stormDust, float rain, float t)
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

    bool show = ground > 0.005f || wall > 0.005f || stormDust > 0.005f || rain > 0.005f;
    if (show != m_debrisShown) {
        if (show) {
            startDebris();
            for (ParticleEmitter *emitter : m_debris) {
                emitter->setOrigin(eye - z * (std::max(frontDistance, 0.0f) + WALL_DEPTH), rotation);
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
        float s = ground;
        if (i == WALL_DUST || i == WALL_EMBERS) {
            /* the wall's leading edge is the front */
            at = eye - z * (std::max(frontDistance, 0.0f) + WALL_DEPTH) + glm::vec3(0.0f, 60.0f, 0.0f);
            s = wall;
        } else if (i == STORM_DUST) {
            at = eye - z * 120.0f + glm::vec3(0.0f, 50.0f, 0.0f);
            s = stormDust;
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
            m_debris[i]->setStrength(rain);
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
            m_debris[i]->setStrength(rain);
            continue;
        } else if (i == WATER_SPRAY) {
            /* spray only over water, near its surface */
            at.y = waterHeight + 1.0f;
            s *= rm->water != nullptr ? 1.0f - smoothstep(60.0f, 220.0f, eye.y - waterHeight) : 0.0f;
        }
        m_debris[i]->setOrigin(at, rotation);
        m_debris[i]->setStrength(s);
    }
}

/* the fires the debris lights, as many as the timeline says by now, at random
 * spots round the player: the first right next to them, later ones further
 * out. On the player's ground (they stand on flat ground: not measured); each
 * grows from a flicker to full over a couple of seconds */
void Aftermath::updateFires(int count, float t, const glm::vec3 &eye)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    const float EYE_HEIGHT = 40.0f;
    for (int k = (int) m_fires.size(); k < count; k++) {
        float angle = hash(k * 3.3f + 1.0f) * 6.2831853f;
        float reach = 80.0f + 18.0f * k;
        float d = glm::mix(25.0f, reach, hash(k * 5.1f + 2.0f));
        glm::vec3 ground(eye.x + std::cos(angle) * d, eye.y - EYE_HEIGHT, eye.z + std::sin(angle) * d);
        Fire fire;
        /* big fires, each its own size: wide, tall flames, a heavy column of smoke */
        float size = 2.0f + 1.5f * hash(k * 6.3f + 21.0f);
        for (int i = 0; i < 3; i++) {
            rapidjson::Document json;
            json.Parse(FIRE[i]);
            json["radius"].SetFloat(json["radius"].GetFloat() * size);
            json["startScale"].SetFloat(json["startScale"].GetFloat() * size);
            json["count"].SetInt((int) (json["count"].GetInt() * (0.6f + 0.5f * size)));
            if (json.HasMember("growth")) json["growth"].SetFloat(json["growth"].GetFloat() * size);
            json["speed"][0].SetFloat(json["speed"][0].GetFloat() * (0.6f + 0.4f * size));
            json["speed"][1].SetFloat(json["speed"][1].GetFloat() * (0.6f + 0.4f * size));
            if (i < 2) {
                /* big but not blinding: dimmer flames (they overlap more) */
                for (int c = 0; c < 3; c++) json["tint"][c].SetFloat(json["tint"][c].GetFloat() * 0.55f);
            }
            ParticleEmitter *emitter = new ParticleEmitter(json);
            emitter->setOrigin(ground + glm::vec3(0.0f, FIRE_HEIGHT[i] * size, 0.0f), glm::mat3(1.0f));
            emitter->setStrength(0.0f);
            rm->addParticleEmitter(emitter);
            fire.emitters.push_back(emitter);
        }
        fire.light = new PointLight(Transform(ground + glm::vec3(0.0f, 18.0f * size, 0.0f), glm::vec3(0.0f),
                                              glm::vec3(240.0f * (0.6f + 0.4f * size))),
                                    glm::vec3(0.0f), false);
        Light::Animation flicker;
        flicker.flicker = 0.35f;
        flicker.flickerSpeed = 10.0f + 3.0f * hash(k * 2.3f);
        fire.light->setAnimation(flicker);
        fire.light->setOwnedByFeature(true);
        rm->addLightToScene(fire.light);
        fire.litAt = t;
        fire.size = size;
        m_fires.push_back(fire);
    }
    for (Fire &fire : m_fires) {
        float grow = smoothstep(0.0f, 2.5f, t - fire.litAt);
        for (ParticleEmitter *emitter : fire.emitters) emitter->setStrength(grow);
        fire.light->setBaseColor(glm::vec3(1.0f, 0.42f, 0.12f) * (0.6f * grow));
    }
}

void Aftermath::putOutFires()
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    std::vector<Light *> &lights = rm->getLights();
    for (Fire &fire : m_fires) {
        for (ParticleEmitter *emitter : fire.emitters) {
            rm->removeParticleEmitter(emitter);
            delete emitter;
        }
        lights.erase(std::remove(lights.begin(), lights.end(), (Light *) fire.light), lights.end());
        delete fire.light;
    }
    m_fires.clear();
}

bool Aftermath::update(Camera *camera, Timeline &timeline)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    if (camera == nullptr) {
        return false;
    }
    BlackHole *hole = nullptr;
    for (BlackHole *h : rm->blackHoles) {
        if (h->sky && h->kugelblitz) { hole = h; break; }
    }
    /* the storm runs while its sequence does (the Moon destruction finished,
     * watched); Restart / Reset rewind the sequence, and this with it */
    float t = timeline.time();
    bool running = hole != nullptr && timeline.running() && t >= 0.0f;
    if (!running || (m_active && t < m_lastT - 0.05f)) {
        if (m_active) reset();
        m_lastT = t;
        if (!running) return false;
    }
    m_lastT = t;
    if (!m_active) {
        m_active = true;
        /* the storm blows from the hole, along the ground */
        m_holeDirection = hole->direction;
        glm::vec2 flat(hole->direction.x, hole->direction.z);
        m_stormDirection = glm::length(flat) > 1e-3f ? -glm::normalize(flat) : glm::vec2(1.0f, 0.0f);
        m_nextGust = m_nextBoom = m_nextThud = -1.0f;
    }
    AudioMaster *audio = AudioMaster::getInstance();
    m_gain = timeline.get("gain", 1.0f);
    rm->setDebrisArrival(timeline.get("debrisArrival", BlackHole::DEBRIS_ARRIVAL));

    /* the gale: gusting round its level; the calm before it */
    float gusting = 1.0f - timeline.get("gustiness", 1.0f) * (0.18f - 0.18f * std::sin(t * 1.9f) * std::sin(t * 0.7f + 1.0f));
    float storm = timeline.get("storm") * gusting;
    rm->setStorm(storm, timeline.get("calm"));
    rm->setStormDirection(m_stormDirection, timeline.get("windTurn"));

    glm::vec3 shake(std::sin(t * 37.0f) * 0.6f + std::sin(t * 23.0f + 1.0f) * 0.4f,
                    std::sin(t * 29.0f + 2.0f) * 0.6f + std::sin(t * 17.0f + 4.0f) * 0.4f,
                    std::sin(t * 31.0f + 3.0f) * 0.5f);
    camera->setShake(shake * timeline.get("shake"));

    /* the dust, the lightning in it (a chance every 0.7 s; a flash flickers
     * twice, thunder follows, later the further away) */
    float dust = timeline.get("dust");
    rm->setDust(dust, timeline.get("dustVeil"));
    float chance = timeline.get("lightning");
    float flash = 0.0f, boltX = 0.5f, boltSeed = 0.0f, boltReach = 0.0f;
    const float SLOT = 0.7f;
    int slot = (int) std::floor(t / SLOT);
    for (int k = slot - 2; k <= slot && chance > 0.0f; k++) {
        float start = k * SLOT + hash(k * 3.7f + 1.0f) * SLOT * 0.6f;
        if (hash(k * 9.1f + 2.0f) > chance) continue;
        float dt = t - start;
        if (dt < 0.0f) continue;
        if (k > m_lastBolt) {
            m_lastBolt = k;
            float delay = 0.25f + 2.2f * hash(k * 5.3f + 3.0f);
            const char *thunder[] = { "boom_1", "boom_2", "boom_3" };
            m_delayed.push_back({ t + delay, thunder[k % 3],
                                  glm::mix(1.0f, 0.45f, delay / 2.5f) * timeline.get("thunderVolume", 1.0f),
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
    rm->setStormWater(timeline.get("rain") * gusting, timeline.get("lensWater"));

    /* what the storm throws, and the fires the debris lights */
    glm::vec3 downwind(m_stormDirection.x, 0.0f, m_stormDirection.y);
    float frontDistance = timeline.get("frontDistance");
    placeDebris(camera, downwind, timeline.get("groundDebris"), timeline.get("wall"), frontDistance,
                timeline.get("stormDust"), timeline.get("debrisRain"), t);
    updateFires((int) timeline.get("fires"), t, camera->getPosition());

    /* the timeline's sounds ride the front in, and the upwind side */
    glm::vec3 eye = camera->getPosition();
    timeline.setAnchor("front", eye - downwind * std::max(frontDistance, 0.0f));
    timeline.setAnchor("upwind", eye - downwind * 60.0f);

    /* the picture and the sound fading out; the hole's quitAfter: then close */
    float fade = timeline.get("fadeOut");
    bool quit = false;
    if (hole->quitAfter > 0.0f) {
        fade = std::max(fade, smoothstep(hole->quitAfter - 2.0f, hole->quitAfter, t));
        quit = t >= hole->quitAfter;
    }
    rm->setFadeOut(fade);
    if (fade > 0.0f) {
        if (m_masterVolume < 0.0f) m_masterVolume = audio->getMasterVolume();
        audio->setMasterVolume(m_masterVolume * (1.0f - fade));
    }

    /* its own sounds: thunder, gusts, booms, thuds (scheduled, not scripted) */
    if (!audio->isAvailable()) {
        return quit;
    }
    for (size_t i = 0; i < m_delayed.size();) {
        if (t >= m_delayed[i].at) {
            play(m_delayed[i].sound, m_delayed[i].volume, m_delayed[i].pitch);
            m_delayed.erase(m_delayed.begin() + i);
        } else {
            i++;
        }
    }
    glm::vec3 across = glm::normalize(glm::cross(glm::vec3(0, 1, 0), downwind));
    float gusts = timeline.get("gusts");
    if (gusts > 0.0f) {
        if (m_nextGust < 0.0f) m_nextGust = t + 0.4f;
        while (t >= m_nextGust) {
            /* a gust rushing past, from upwind and to one side */
            float side = hash(m_gusts * 3.1f) * 2.0f - 1.0f;
            float volume = (0.55f + 0.45f * hash(m_gusts * 5.7f + 1.0f)) * glm::clamp(storm * 1.4f, 0.0f, 1.0f) *
                           timeline.get("gustVolume", 1.0f);
            float pitch = 0.85f + 0.25f * hash(m_gusts * 7.3f + 2.0f);
            if (volume > 0.05f) {
                play3D("gust_" + std::to_string(1 + m_gusts % 3), eye - downwind * 90.0f + across * (side * 140.0f),
                       volume, 700.0f, pitch);
            }
            m_gusts++;
            /* often in the gale, rarely once it settles */
            m_nextGust += (1.2f + 2.6f * hash(m_gusts * 11.1f)) / glm::max(storm, 0.25f) * 0.5f / gusts;
        }
    }
    float booms = timeline.get("booms");
    if (booms > 0.0f) {
        if (m_nextBoom < 0.0f) m_nextBoom = t;
        if (t >= m_nextBoom) {
            /* debris coming down further off */
            const char *names[] = { "boom_1", "boom_2", "boom_3", "blast" };
            int b = m_booms % 4;
            float volume = timeline.get("boomVolume", 1.0f) * (b == 3 ? 0.7f : 1.0f);
            play(names[b], volume, 0.6f + 0.3f * hash(m_booms * 3.7f + 9.0f));
            m_booms++;
            m_nextBoom = t + (0.6f + 0.8f * hash(m_booms * 5.9f)) / booms;
        }
    }
    float thuds = timeline.get("thuds");
    if (thuds > 0.0f) {
        if (m_nextThud < 0.0f) m_nextThud = t;
        if (t >= m_nextThud) {
            /* debris thudding down close by */
            float a = hash(m_thuds * 4.3f) * 6.2831853f, d = 40.0f + 90.0f * hash(m_thuds * 6.1f + 1.0f);
            play3D("land", eye + glm::vec3(std::cos(a) * d, -10.0f, std::sin(a) * d),
                   0.5f + 0.5f * hash(m_thuds * 8.9f + 2.0f), 300.0f, 0.5f + 0.3f * hash(m_thuds * 2.2f + 3.0f));
            m_thuds++;
            m_nextThud = t + (0.36f + 1.3f * hash(m_thuds * 3.3f)) / thuds;
        }
    }
    return quit;
}
