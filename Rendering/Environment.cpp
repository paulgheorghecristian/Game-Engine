#include "Environment.h"

#include <string>

static void readVec3(const rapidjson::Value &json, const char *key, glm::vec3 &out) {
    if (json.HasMember(key) && json[key].IsArray() && json[key].Size() >= 3) {
        out = glm::vec3(json[key][0].GetFloat(), json[key][1].GetFloat(), json[key][2].GetFloat());
    }
}

static void readFloat(const rapidjson::Value &json, const char *key, float &out) {
    if (json.HasMember(key) && json[key].IsNumber()) {
        out = json[key].GetFloat();
    }
}

void Environment::load(const rapidjson::Value &json)
{
    if (!json.IsObject()) {
        return;
    }
    if (json.HasMember("sky") && json["sky"].IsString()) {
        skyMode = std::string(json["sky"].GetString()) == "night" ? 1 : 0;
    }
    readVec3(json, "zenithColor", zenithColor);
    readVec3(json, "horizonColor", horizonColor);
    readFloat(json, "starIntensity", starIntensity);
    readVec3(json, "moonDirection", moonDirection);
    if (glm::length(moonDirection) > 1e-4f) {
        moonDirection = glm::normalize(moonDirection);
    }
    readVec3(json, "moonColor", moonColor);
    readVec3(json, "fogColor", fogColor);
    readFloat(json, "fogDensity", fogDensity);
    readFloat(json, "fogHeightFalloff", fogHeightFalloff);
    readFloat(json, "fogBaseHeight", fogBaseHeight);
    readFloat(json, "fogMoonGlow", fogMoonGlow);
    readFloat(json, "ambient", ambient);
    readFloat(json, "emissiveStrength", emissiveStrength);

    readFloat(json, "dayLength", dayLength);
    readFloat(json, "timeOfDay", timeOfDay);
    readVec3(json, "dayZenithColor", dayZenithColor);
    readVec3(json, "dayHorizonColor", dayHorizonColor);
    readVec3(json, "sunsetColor", sunsetColor);
    readVec3(json, "sunColor", sunColor);
    readVec3(json, "dayFogColor", dayFogColor);
    readFloat(json, "dayFogDensityScale", dayFogDensityScale);
    readFloat(json, "dayAmbient", dayAmbient);
    lightDirection = moonDirection;
    lightColor = moonColor;
}

static float smoothstepf(float e0, float e1, float x) {
    float t = glm::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Environment Environment::atTimeOfDay(const Environment &base, float t)
{
    Environment e = base;
    e.timeOfDay = t - glm::floor(t);

    /* The moon stays where the map puts it (it is the night sky's key light).
     * The sun rises opposite-ish, culminates 60 degrees high on the side away
     * from the moon at noon, and sets: a great circle through that noon point. */
    glm::vec3 moonFlat = glm::vec3(base.moonDirection.x, 0.0f, base.moonDirection.z);
    moonFlat = glm::length(moonFlat) > 1e-4f ? glm::normalize(moonFlat) : glm::vec3(0, 0, -1);
    const float noonElevation = glm::radians(60.0f);
    glm::vec3 noon = glm::normalize(-moonFlat * glm::cos(noonElevation) + glm::vec3(0, glm::sin(noonElevation), 0));
    glm::vec3 east = glm::normalize(glm::cross(glm::vec3(0, 1, 0), noon));
    float phase = e.timeOfDay * 6.2831853f;
    e.sunDirection = glm::normalize(glm::sin(phase) * east - glm::cos(phase) * noon);
    glm::vec3 moon = glm::normalize(base.moonDirection);

    float day = smoothstepf(-0.12f, 0.18f, e.sunDirection.y);
    float night = 1.0f - day;
    e.sunVisibility = smoothstepf(-0.05f, 0.05f, e.sunDirection.y);
    e.sunsetStrength = smoothstepf(-0.22f, 0.0f, e.sunDirection.y) *
                       (1.0f - smoothstepf(0.0f, 0.35f, e.sunDirection.y));

    e.zenithColor = glm::mix(base.zenithColor, base.dayZenithColor, day);
    e.horizonColor = glm::mix(base.horizonColor, base.dayHorizonColor, day);
    e.starIntensity = base.starIntensity * night * night;
    e.moonColor = base.moonColor * (0.25f + 0.75f * night);

    e.fogColor = glm::mix(base.fogColor, base.dayFogColor, day) +
                 base.sunsetColor * (0.25f * e.sunsetStrength);
    e.fogDensity = base.fogDensity * glm::mix(1.0f, base.dayFogDensityScale, day);
    e.ambient = glm::mix(base.ambient, base.dayAmbient, day);

    /* dominant sky light: the (warm, low) sun by day, the moon by night */
    float sunHeight = smoothstepf(-0.02f, 0.3f, e.sunDirection.y);
    glm::vec3 sunLight = base.sunColor * glm::mix(base.sunsetColor * 1.4f, glm::vec3(1.0f),
                                                  smoothstepf(0.0f, 0.4f, e.sunDirection.y));
    if (e.sunDirection.y > -0.02f) {
        e.lightDirection = e.sunDirection;
        e.lightColor = sunLight * sunHeight;
    } else {
        e.lightDirection = moon;
        e.lightColor = base.moonColor * smoothstepf(-0.02f, 0.3f, moon.y);
    }
    return e;
}
