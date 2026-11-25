#include "Aftermath.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

#include "AudioMaster.h"
#include "BlackHole.h"
#include "Camera.h"
#include "ParticleEmitter.h"
#include "PointLight.h"
#include "RenderingMaster.h"
#include "MoonDestruction.h"
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

/* Everything the storm reads from the timeline: the one list of it. The storm
 * code below reads them by name through value(); the timeline editor gets
 * this list from the engine and builds its sliders from it. */
static const std::vector<Aftermath::Parameter> STORM = {
    /* name            group                 min  max   standard  what it does */
    { "collapse",      "Moon",               0.0f, 1.0f, 0.0f, "How far the Moon has fallen into the black hole (0 = whole, 1 = gone: the black hole has formed). Not set: the map's own playback of it." },
    { "wind",          "Wind",               0.0f, 1.5f, 0.0f, "How hard the gale blows: grass, trees, dust and smoke bend and stream with it." },
    { "calm",          "Wind",               0.0f, 1.0f, 0.0f, "The map's own wind dropping (1 = almost still). Does not touch the gale (wind)." },
    { "windTurn",      "Wind",               0.0f, 1.0f, 0.0f, "The wind turning to blow from the black hole (0 = the map's own wind, 1 = straight from the hole)." },
    { "gustiness",     "Wind",               0.0f, 2.0f, 1.0f, "How much the gale and the rain swell and drop (0 = steady). Changes wind and rain only while they are on." },
    { "waves",         "Wind",               0.0f, 1.5f, 0.0f, "How high the waves swell on the water." },
    { "fog",           "Sky",                0.0f, 1.5f, 0.0f, "Storm fog: thicker, and reaching higher up." },
    { "clouds",        "Sky",                0.0f, 1.5f, 0.0f, "Storm clouds: covering more of the sky and racing across it." },
    { "debrisShell",   "Sky",                0.0f, 2.0f, 0.0f, "The Moon's debris spreading across the sky: 0 = at the black hole, 1 = it has reached you (the horizon), 2 = gone. Not set: it follows the Moon (it reaches you 8 s after the black hole formed)." },
    { "ejecta",        "Sky",                0.0f, 1.0f, 0.0f, "Matter flying out of the black hole straight at you. Not set: it follows the Moon (as the last of it falls in, until the debris reaches you)." },
    { "debrisRain",    "Debris",             0.0f, 1.5f, 0.0f, "The Moon's debris raining out of the sky, some of it straight at you." },
    { "groundDebris",  "Debris",             0.0f, 1.5f, 0.0f, "Dust, chunks and embers flung along the ground, and spray off the water. They come from the black hole's side." },
    { "wall",          "Debris",             0.0f, 1.5f, 0.0f, "A wall of dust and embers on the ground at the storm front. Only seen when frontDistance is small: far away it is hidden by the land." },
    { "frontDistance", "Debris",             0.0f, 3000.0f, 0.0f, "How far away the storm front is (0 = it has reached you). The storm's roar comes from there, and the wall stands there." },
    { "stormDust",     "Debris",             0.0f, 1.5f, 0.0f, "Thick billows of dust rushing past the camera. They come from the black hole's side." },
    { "fires",         "Debris",             0.0f, 60.0f, 0.0f, "How many fires burn around you (a whole number; each takes a couple of seconds to grow)." },
    { "dust",          "Dust and lightning", 0.0f, 1.0f, 0.0f, "How thick the dust fog over the land is." },
    { "dustVeil",      "Dust and lightning", 0.0f, 1.0f, 0.0f, "How much of the picture the dust hides (1 = nothing but dust)." },
    { "lightning",     "Dust and lightning", 0.0f, 1.0f, 0.0f, "How often lightning flashes inside the dust: the chance of a strike every 0.7 seconds (1 = every time). Thunder follows each strike: the timeline's thunder noises." },
    { "thunderVolume", "Dust and lightning", 0.0f, 3.0f, 1.0f, "How loud the thunder is. Only heard when lightning strikes." },
    { "rain",          "Rain",               0.0f, 1.5f, 0.0f, "Wind-driven rain streaking past." },
    { "lensWater",     "Rain",               0.0f, 1.5f, 0.0f, "Water drops on the camera lens." },
    { "shake",         "Camera",             0.0f, 0.1f, 0.0f, "How hard the camera shakes (0.01 = a tremble, 0.1 = violent)." },
    { "fadeOut",       "Camera",             0.0f, 1.0f, 0.0f, "Fades the picture and all sound out (1 = black and silent)." },
    { "gain",          "Noise",              0.0f, 4.0f, 1.0f, "Loudness of all the storm's sounds: the ones in Sounds, and its gusts, booms, thuds and thunder." },
    { "gusts",         "Noise",              0.0f, 6.0f, 0.0f, "How often gusts of wind rush past (1 = about one a second, 2 = twice as often). Their sounds: the timeline's gusts noises." },
    { "gustVolume",    "Noise",              0.0f, 3.0f, 1.0f, "How loud the gusts are. Only heard when gusts is on." },
    { "booms",         "Noise",              0.0f, 4.0f, 0.0f, "How often distant impacts boom (about that many a second). Their sounds: the timeline's booms noises." },
    { "boomVolume",    "Noise",              0.0f, 3.0f, 1.0f, "How loud the booms are. Only heard when booms is on." },
    { "thuds",         "Noise",              0.0f, 4.0f, 0.0f, "How often debris thuds down close by (about that many a second). Their sounds: the timeline's thuds noises." },
    { "thudVolume",    "Noise",              0.0f, 3.0f, 1.0f, "How loud the thuds are. Only heard when thuds is on." },
};
/* ...and the lanes of the elements that read the timeline themselves (their own files) */
static std::vector<Aftermath::Parameter> joined(std::vector<Aftermath::Parameter> list,
                                                const std::vector<Aftermath::Parameter> &more)
{
    list.insert(list.end(), more.begin(), more.end());
    return list;
}
static const std::vector<Aftermath::Parameter> PARAMETERS = joined(STORM, MoonDestruction::lanes());

const std::vector<Aftermath::Parameter> &Aftermath::parameters()
{
    return PARAMETERS;
}

std::map<std::string, float> Aftermath::isolatedOff()
{
    static const char *MODIFIERS[] = { "gain", "gustiness", "gustVolume", "boomVolume", "thudVolume", "thunderVolume", "frontDistance" };
    std::map<std::string, float> off;
    for (const Parameter &p : PARAMETERS) {
        if (std::none_of(std::begin(MODIFIERS), std::end(MODIFIERS), [&](const char *m) { return std::strcmp(m, p.name) == 0; })) {
            off[p.name] = p.standard;
        }
    }
    return off;
}

/* text as a JSON string's contents (quotes, backslashes and control characters escaped) */
static std::string jsonText(const char *text)
{
    std::string out;
    for (const char *c = text; *c; c++) {
        if (*c == '"' || *c == '\\') { out += '\\'; out += *c; }
        else if ((unsigned char) *c < 0x20) { char code[8]; std::snprintf(code, sizeof(code), "\\u%04x", *c); out += code; }
        else out += *c;
    }
    return out;
}

std::string Aftermath::parametersJson()
{
    std::string json = "[";
    for (const Parameter &p : PARAMETERS) {
        char numbers[96];
        std::snprintf(numbers, sizeof(numbers), "\"min\": %g, \"max\": %g, \"standard\": %g", p.min, p.max, p.standard);
        if (json.size() > 1) json += ", ";
        json += "{\"name\": \"" + jsonText(p.name) + "\", \"group\": \"" + jsonText(p.group) + "\", " + numbers +
                (p.after ? ", \"after\": \"" + jsonText(p.after) + "\"" : std::string()) +
                ", \"about\": \"" + jsonText(p.about) + "\"}";
    }
    return json + "]";
}

std::string Aftermath::anchorsJson()
{
    return "[{\"name\": \"\", \"label\": \"everywhere\", \"about\": \"Not from a place: the same all round you.\"}, "
           "{\"name\": \"front\", \"label\": \"storm front\", \"about\": \"From the storm's front, frontDistance away: it gets louder as the front comes in.\"}, "
           "{\"name\": \"upwind\", \"label\": \"upwind\", \"about\": \"From just upwind of you, the side the storm comes from.\"}]";
}

/* where the Moon's destruction is on the scene clock: when it starts to break up, and when the
 * black hole has formed. From the timeline when it sets "collapse", else the map's playback */
static float breakupOn(const Timeline &timeline, const BlackHole *hole)
{
    float t = timeline.firstReach("collapse", 0.0005f);
    return t >= 0.0f ? t : hole->collapseDelay;
}
static float formedOn(const Timeline &timeline, const BlackHole *hole)
{
    float t = timeline.firstReach("collapse", 1.0f);
    return t >= 0.0f ? t : hole->formationTime();
}

/* when the Moon's debris reaches us: the timeline's debrisShell reaching 1, or (not set)
 * BlackHole::DEBRIS_ARRIVAL seconds after the black hole formed */
static float impactOn(const Timeline &timeline, const BlackHole *hole)
{
    float t = timeline.firstReach("debrisShell", 1.0f);
    return t >= 0.0f ? t : formedOn(timeline, hole) + BlackHole::DEBRIS_ARRIVAL;
}

void Aftermath::driveCollapse(BlackHole *hole, const Timeline &timeline)
{
    float collapse = timeline.get("collapse", -1.0f);
    hole->driven = collapse >= 0.0f;
    if (!hole->driven) return;
    float breakup = breakupOn(timeline, hole), formed = formedOn(timeline, hole);
    hole->collapse = glm::clamp(collapse, 0.0f, 1.0f);
    hole->playing = false;
    hole->playTime = timeline.clock();
    hole->delayElapsed = hole->collapseDelay;
    hole->drivenRate = formed > breakup ? 1.0f / (formed - breakup) : 0.0f;
    hole->formedAt = hole->collapse >= 1.0f ? formed : -1.0f;
}

std::string Aftermath::timelineJson(const Timeline &timeline)
{
    const BlackHole *hole = nullptr;
    for (const BlackHole *h : RenderingMaster::getInstance()->blackHoles) {
        if (h->sky && h->kugelblitz) { hole = h; break; }
    }
    float length = timeline.sceneLength();
    if (hole != nullptr) length = std::max(length, impactOn(timeline, hole) + 5.0f);
    char text[200];
    std::snprintf(text, sizeof(text), "\"length\": %.3f, \"firstSound\": %.3f, \"silent\": %s, \"starts\": [",
                  length, timeline.firstSoundScene(), hole != nullptr && hole->silent ? "true" : "false");
    std::string json = text;
    std::vector<float> starts = timeline.starts();
    for (size_t i = 0; i < starts.size(); i++) {
        std::snprintf(text, sizeof(text), "%s%.3f", i ? ", " : "", starts[i]);
        json += text;
    }
    json += "], \"moments\": [";
    bool first = true;
    auto moment = [&](const char *id, float t, const char *label) {
        std::snprintf(text, sizeof(text), "%s{\"id\": \"%s\", \"t\": %.2f, \"label\": \"%s\"}", first ? "" : ", ", id, t, label);
        json += text;
        first = false;
    };
    moment("start", 0.0f, "start");
    if (hole != nullptr) {
        float formed = formedOn(timeline, hole), breakup = breakupOn(timeline, hole);
        if (breakup > 0.05f) moment("breakup", breakup, "the Moon breaks up");
        moment("formed", formed, "black hole formed");
        moment("impact", impactOn(timeline, hole), "debris reaches you");
    }
    /* "all dust": the first moment the dust veil hides everything, if it ever does */
    for (float t = 0.0f; t <= length; t += 0.1f) {
        if (timeline.valueAtScene("dustVeil", t, 0.0f) >= 0.99f) {
            moment("dust", t, "all dust");
            break;
        }
    }
    return json + "]";
}

/* the n-th sound of one of the timeline's "noises" sets (they take turns); nullptr: it names none,
 * and that noise is silent (the storm names no sound of its own) */
static const Timeline::Noise *noiseOf(const Timeline &timeline, const char *name, long n)
{
    const std::vector<Timeline::Noise> *set = timeline.noises(name);
    if (set == nullptr || set->empty()) return nullptr;
    long size = (long) set->size();
    return &(*set)[((n % size) + size) % size];
}

/* a parameter as the timeline has it now, or its standard value */
static float value(const Timeline &timeline, const char *name)
{
    for (const Aftermath::Parameter &p : PARAMETERS) {
        if (std::strcmp(p.name, name) == 0) return timeline.get(name, p.standard);
    }
    return timeline.get(name);      /* (not in the list: a mistake in the code below) */
}

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
    int voice = AudioMaster::getInstance()->play(name, volume * m_gain, pitch, loop);
    return loop ? voice : remember(voice);
}

int Aftermath::remember(int voice)
{
    if (voice == 0) return 0;
    if (m_voices.size() >= 48) {
        AudioMaster *audio = AudioMaster::getInstance();
        m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(), [audio](int v) { return !audio->isPlaying(v); }),
                       m_voices.end());
    }
    m_voices.push_back(voice);
    return voice;
}

/* a jump in time (the timeline editor, Restart): what is still sounding belongs to where we were */
void Aftermath::cutVoices()
{
    AudioMaster *audio = AudioMaster::getInstance();
    for (int voice : m_voices) audio->cut(voice);
    m_voices.clear();
}

int Aftermath::play3D(const std::string &name, const glm::vec3 &position, float volume, float radius, float pitch)
{
    return remember(AudioMaster::getInstance()->play3D(name, position, volume * m_gain, radius, pitch, false));
}

void Aftermath::reset()
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    AudioMaster *audio = AudioMaster::getInstance();
    m_active = false;
    rm->setWeather(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    rm->setStormDirection(m_stormDirection, 0.0f);
    rm->getCamera()->setShake(glm::vec3(0.0f));
    rm->setFadeOut(0.0f);
    rm->setDust(0.0f, 0.0f);
    rm->setStormWater(0.0f, 0.0f);
    rm->setLightning(0.0f, 0.5f, 0.0f, 0.0f);
    rm->setDebris(-1.0f, -1.0f);
    putOutFires();
    m_gusts = m_booms = m_thuds = m_thunders = 0;
    m_lastGust = m_lastBoom = m_lastThud = m_lastThunder = 0.0f;
    m_applied.clear();
    m_lastBolt = NO_BOLT;
    m_nextGust = m_nextBoom = m_nextThud = UNSCHEDULED;
    m_delayed.clear();
    cutVoices();
    audio->setFade(1.0f);
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
    while ((int) m_fires.size() > std::max(count, 0)) {
        /* fewer now (the timeline edited, or scrubbed back): the last ones go out */
        Fire &fire = m_fires.back();
        for (ParticleEmitter *emitter : fire.emitters) {
            rm->removeParticleEmitter(emitter);
            delete emitter;
        }
        rm->removeLightFromScene(fire.light);
        m_fires.pop_back();
    }
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
    for (Fire &fire : m_fires) {
        for (ParticleEmitter *emitter : fire.emitters) {
            rm->removeParticleEmitter(emitter);
            delete emitter;
        }
        rm->removeLightFromScene(fire.light);
    }
    m_fires.clear();
}

void Aftermath::applied(const char *name, float v)
{
    for (auto &a : m_applied) {
        if (a.first == name) { a.second = v; return; }
    }
    m_applied.emplace_back(name, v);
}

std::string Aftermath::appliedJson() const
{
    std::string json = "\"applied\": {";
    char text[96];
    for (size_t i = 0; i < m_applied.size(); i++) {
        std::snprintf(text, sizeof(text), "%s\"%s\": %.4f", i ? ", " : "", m_applied[i].first.c_str(),
                      std::isfinite(m_applied[i].second) ? m_applied[i].second : -999.0f);
        json += text;
    }
    std::snprintf(text, sizeof(text), "}, \"played\": {\"gusts\": [%d, %.3f], \"booms\": [%d, %.3f], ",
                  m_gusts, m_lastGust, m_booms, m_lastBoom);
    json += text;
    std::snprintf(text, sizeof(text), "\"thuds\": [%d, %.3f], \"thunder\": [%d, %.3f]}",
                  m_thuds, m_lastThud, m_thunders, m_lastThunder);
    return json + text;
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
    /* the Moon's destruction itself, when the timeline sets it */
    if (hole != nullptr) driveCollapse(hole, timeline);
    /* the effects apply whenever a timeline plays, from its start, black hole or not. Their own
     * clock (their random patterns: lightning, gusts, shake; quitAfter) counts from the black hole
     * forming when there is one (negative before), else it is the timeline's clock. A rewind resets */
    float t = timeline.clock();
    if (hole != nullptr) t = hole->formedAt >= 0.0f ? hole->playTime - hole->formedAt : timeline.clock() - formedOn(timeline, hole);
    bool running = timeline.running();
    if (!running || (m_active && t < m_lastT - 0.05f)) {
        if (m_active) reset();
        m_lastT = t;
        if (!running) return false;
    }
    if (m_active && t > m_lastT + 0.5f) {
        cutVoices();                /* jumped ahead */
        m_delayed.clear();
    }
    m_lastT = t;
    if (!m_active) {
        m_active = true;
        /* the storm blows from the hole, along the ground (no hole: from the sky to the north) */
        m_holeDirection = hole != nullptr ? hole->direction : glm::vec3(0.0f, 0.6f, -0.8f);
        glm::vec2 flat(m_holeDirection.x, m_holeDirection.z);
        m_stormDirection = glm::length(flat) > 1e-3f ? -glm::normalize(flat) : glm::vec2(1.0f, 0.0f);
        m_nextGust = m_nextBoom = m_nextThud = UNSCHEDULED;
    }
    AudioMaster *audio = AudioMaster::getInstance();
    m_gain = value(timeline, "gain");
    /* the debris in the sky (not set: < 0, it follows the Moon) */
    rm->setDebris(timeline.get("debrisShell", -1.0f), timeline.get("ejecta", -1.0f));
    rm->moonDestruction().readTimeline(timeline);          /* (its own lanes: the blast) */

    /* the weather, each part on its own; the gale and the rain gusting round their levels */
    float gusting = 1.0f - value(timeline, "gustiness") * (0.18f - 0.18f * std::sin(t * 1.9f) * std::sin(t * 0.7f + 1.0f));
    rm->setWeather(value(timeline, "wind") * gusting, value(timeline, "fog"), value(timeline, "clouds"),
                   value(timeline, "waves"), value(timeline, "calm"));
    applied("wind", value(timeline, "wind") * gusting);
    applied("fog", value(timeline, "fog"));
    applied("clouds", value(timeline, "clouds"));
    applied("waves", value(timeline, "waves"));
    applied("calm", value(timeline, "calm"));
    applied("windTurn", value(timeline, "windTurn"));
    applied("gain", m_gain);
    rm->setStormDirection(m_stormDirection, value(timeline, "windTurn"));

    glm::vec3 shake(std::sin(t * 37.0f) * 0.6f + std::sin(t * 23.0f + 1.0f) * 0.4f,
                    std::sin(t * 29.0f + 2.0f) * 0.6f + std::sin(t * 17.0f + 4.0f) * 0.4f,
                    std::sin(t * 31.0f + 3.0f) * 0.5f);
    camera->setShake(shake * value(timeline, "shake"));
    applied("shake", value(timeline, "shake"));

    /* the dust, the lightning in it (a chance every 0.7 s; a flash flickers
     * twice, thunder follows, later the further away) */
    float dust = value(timeline, "dust");
    rm->setDust(dust, value(timeline, "dustVeil"));
    applied("dust", dust);
    applied("dustVeil", value(timeline, "dustVeil"));
    float chance = value(timeline, "lightning");
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
            if (const Timeline::Noise *thunder = noiseOf(timeline, "thunder", k)) {
                m_delayed.push_back({ t + delay, thunder->sound,
                                      glm::mix(1.0f, 0.45f, delay / 2.5f) * value(timeline, "thunderVolume") * thunder->volume,
                                      (0.8f + 0.35f * hash(k * 2.9f + 4.0f)) * thunder->pitch });
            }
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
    rm->setStormWater(value(timeline, "rain") * gusting, value(timeline, "lensWater"));
    applied("lightning", chance);
    applied("rain", value(timeline, "rain") * gusting);
    applied("lensWater", value(timeline, "lensWater"));

    /* what the storm throws, and the fires the debris lights */
    glm::vec3 downwind(m_stormDirection.x, 0.0f, m_stormDirection.y);
    float frontDistance = value(timeline, "frontDistance");
    placeDebris(camera, downwind, value(timeline, "groundDebris"), value(timeline, "wall"), frontDistance,
                value(timeline, "stormDust"), value(timeline, "debrisRain"), t);
    updateFires((int) value(timeline, "fires"), t, camera->getPosition());
    applied("groundDebris", value(timeline, "groundDebris"));
    applied("wall", value(timeline, "wall"));
    applied("frontDistance", frontDistance);
    applied("stormDust", value(timeline, "stormDust"));
    applied("debrisRain", value(timeline, "debrisRain"));
    applied("fires", (float) m_fires.size());

    /* the timeline's sounds ride the front in, and the upwind side */
    glm::vec3 eye = camera->getPosition();
    timeline.setAnchor("front", eye - downwind * std::max(frontDistance, 0.0f));
    timeline.setAnchor("upwind", eye - downwind * 60.0f);

    /* the picture and the sound fading out; the hole's quitAfter: then close */
    float fade = value(timeline, "fadeOut");
    bool quit = false;
    if (hole != nullptr && hole->quitAfter > 0.0f) {
        fade = std::max(fade, smoothstep(hole->quitAfter - 2.0f, hole->quitAfter, t));
        quit = t >= hole->quitAfter;
    }
    fade = glm::clamp(fade, 0.0f, 1.0f);
    rm->setFadeOut(fade);
    applied("fadeOut", fade);
    audio->setFade(1.0f - glm::clamp(fade, 0.0f, 1.0f));

    /* its own sounds: thunder, gusts, booms, thuds (scheduled, not scripted) */
    if (!audio->isAvailable()) {
        return quit;
    }
    for (size_t i = 0; i < m_delayed.size();) {
        if (t >= m_delayed[i].at) {
            if (t - m_delayed[i].at < 1.0f) {
                play(m_delayed[i].sound, m_delayed[i].volume, m_delayed[i].pitch);
                m_thunders++;
                m_lastThunder = m_delayed[i].volume * m_gain;
            }
            m_delayed.erase(m_delayed.begin() + i);
        } else {
            i++;
        }
    }
    glm::vec3 across = glm::normalize(glm::cross(glm::vec3(0, 1, 0), downwind));
    float gusts = value(timeline, "gusts");
    if (gusts > 0.0f) {
        if (m_nextGust == UNSCHEDULED || t - m_nextGust > 2.0f) m_nextGust = t + 0.4f;   /* (a jump ahead: not a burst of the ones missed) */
        while (t >= m_nextGust) {
            /* a gust rushing past, from upwind and to one side */
            float side = hash(m_gusts * 3.1f) * 2.0f - 1.0f;
            float volume = (0.55f + 0.45f * hash(m_gusts * 5.7f + 1.0f)) * value(timeline, "gustVolume");
            float pitch = 0.85f + 0.25f * hash(m_gusts * 7.3f + 2.0f);
            const Timeline::Noise *gust = noiseOf(timeline, "gusts", m_gusts);
            if (gust != nullptr) volume *= gust->volume;
            m_lastGust = gust != nullptr ? volume * m_gain : 0.0f;
            if (volume > 0.05f && gust != nullptr) {
                play3D(gust->sound, eye - downwind * 90.0f + across * (side * 140.0f), volume, 700.0f, pitch * gust->pitch);
            }
            m_gusts++;
            m_nextGust += (1.2f + 2.6f * hash(m_gusts * 11.1f)) * 0.5f / gusts;
        }
    }
    float booms = value(timeline, "booms");
    if (booms > 0.0f) {
        if (m_nextBoom == UNSCHEDULED || t - m_nextBoom > 5.0f) m_nextBoom = t;
        if (t >= m_nextBoom) {
            /* debris coming down further off */
            const Timeline::Noise *boom = noiseOf(timeline, "booms", m_booms);
            float volume = boom != nullptr ? value(timeline, "boomVolume") * boom->volume : 0.0f;
            if (boom != nullptr) play(boom->sound, volume, (0.6f + 0.3f * hash(m_booms * 3.7f + 9.0f)) * boom->pitch);
            m_lastBoom = volume * m_gain;
            m_booms++;
            m_nextBoom = t + (0.6f + 0.8f * hash(m_booms * 5.9f)) / booms;
        }
    }
    float thuds = value(timeline, "thuds");
    if (thuds > 0.0f) {
        if (m_nextThud == UNSCHEDULED || t - m_nextThud > 5.0f) m_nextThud = t;
        if (t >= m_nextThud) {
            /* debris thudding down close by */
            float a = hash(m_thuds * 4.3f) * 6.2831853f, d = 40.0f + 90.0f * hash(m_thuds * 6.1f + 1.0f);
            const Timeline::Noise *thud = noiseOf(timeline, "thuds", m_thuds);
            float volume = thud != nullptr ? (0.5f + 0.5f * hash(m_thuds * 8.9f + 2.0f)) * value(timeline, "thudVolume") * thud->volume : 0.0f;
            if (thud != nullptr) {
                play3D(thud->sound, eye + glm::vec3(std::cos(a) * d, -10.0f, std::sin(a) * d), volume, 300.0f,
                       (0.5f + 0.3f * hash(m_thuds * 2.2f + 3.0f)) * thud->pitch);
            }
            m_lastThud = volume * m_gain;
            m_thuds++;
            m_nextThud = t + (0.36f + 1.3f * hash(m_thuds * 3.3f)) / thuds;
        }
    }
    return quit;
}
