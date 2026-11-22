#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <glm/glm.hpp>

#include "rapidjson/document.h"

/*
 * Per-map atmosphere, from the map's optional "Environment" object.
 * The defaults reproduce the engine's original look (dusk gradient sky,
 * ambient 0.1, no fog), so maps without an "Environment" are unchanged.
 */
struct Environment
{
    int skyMode = 0;                    /* "sky": "dusk" (0) or "night" (1) */
    glm::vec3 zenithColor = glm::vec3(0.01f, 0.015f, 0.05f);
    glm::vec3 horizonColor = glm::vec3(0.08f, 0.1f, 0.2f);
    float starIntensity = 1.0f;

    glm::vec3 moonDirection = glm::normalize(glm::vec3(0.4f, 0.35f, -1.0f)); /* towards the moon */
    glm::vec3 moonColor = glm::vec3(0.55f, 0.65f, 0.9f);

    glm::vec3 fogColor = glm::vec3(0.0f);
    float fogDensity = 0.0f;            /* 0 disables fog */
    float fogHeightFalloff = 0.0f;      /* fog thins out above fogBaseHeight */
    float fogBaseHeight = 0.0f;
    float fogMoonGlow = 0.0f;           /* haze brightens toward the moon (forward scattering) */

    float ambient = 0.1f;               /* added to the light accumulation */

    /* post-processing (all off by default) */
    bool hdr = false;                   /* filmic tone mapping instead of clipping at 1.0 */
    float exposure = 1.0f;
    float bloomStrength = 0.0f;         /* needs hdr */
    float bloomThreshold = 1.0f;
    float aoStrength = 0.0f;            /* screen-space ambient occlusion, 0 = off */
    float aoRadius = 8.0f;              /* world units */
    /* hemisphere ambient: surfaces facing up get ambient * skyTint, facing
     * down ambient * groundTint (neutral defaults = flat ambient as before) */
    glm::vec3 ambientSkyTint = glm::vec3(1.0f);
    glm::vec3 ambientGroundTint = glm::vec3(1.0f);
    float emissiveStrength = 1.5f;      /* scale for material "emissive" */

    /* Day-night cycle (off when dayLength is 0). The moon stays at
     * moonDirection; the sun rises, culminates 60 degrees high on the side
     * opposite the moon at noon, and sets. The colors above are the night
     * look, these are blended in by day. */
    float dayLength = 0.0f;             /* seconds per full day */
    float timeOfDay = 0.0f;             /* 0 midnight, 0.25 sunrise, 0.5 noon, 0.75 sunset */
    glm::vec3 dayZenithColor = glm::vec3(0.16f, 0.36f, 0.75f);
    glm::vec3 dayHorizonColor = glm::vec3(0.6f, 0.72f, 0.86f);
    glm::vec3 sunsetColor = glm::vec3(1.0f, 0.42f, 0.15f);
    glm::vec3 sunColor = glm::vec3(1.0f, 0.93f, 0.8f);
    glm::vec3 dayFogColor = glm::vec3(0.55f, 0.65f, 0.78f);
    float dayFogDensityScale = 0.4f;
    float dayAmbient = 0.3f;
    glm::vec3 dayAmbientSkyTint = glm::vec3(0.8f, 0.9f, 1.2f);    /* blue sky light */
    glm::vec3 dayAmbientGroundTint = glm::vec3(0.75f, 0.68f, 0.5f); /* warm ground bounce */

    /* derived each tick (not read from JSON) */
    glm::vec3 sunDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    float sunVisibility = 0.0f;         /* 0 at night, 1 by day */
    float sunsetStrength = 0.0f;        /* horizon glow around sunrise/sunset */
    glm::vec3 lightDirection = moonDirection; /* dominant sky light (glints, haze glow) */
    glm::vec3 lightColor = moonColor;
    glm::vec3 ambientSky = glm::vec3(0.1f), ambientGround = glm::vec3(0.1f); /* final, per tick */

    void load(const rapidjson::Value &json);
    /* all configurable fields (what load() reads) */
    void save(rapidjson::Value &json, rapidjson::Document::AllocatorType &allocator) const;
    /* the environment at `timeOfDay` (night colors stay in `base`) */
    static Environment atTimeOfDay(const Environment &base, float timeOfDay);
};

#endif // ENVIRONMENT_H
