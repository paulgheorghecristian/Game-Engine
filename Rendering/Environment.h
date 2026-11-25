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
    float moonPhase = 0.0f;             /* 0 full, 0.5 half, 1 new */
    float moonSize = 0.026f;            /* angular radius of the moon (radians, max 0.09) */
    bool moon = true;                   /* false: no moon (no disc, halo or moonlight) */
    /* the Milky Way (night sky): strength (0 = none), the pole of its band,
     * and the direction of its bright core */
    float milkyWay = 0.0f;
    glm::vec3 milkyWayNormal = glm::vec3(0.35f, 0.5f, 0.79f);
    glm::vec3 milkyWayCenter = glm::vec3(0.0f, 0.4f, -0.9f);
    bool timePaused = false;            /* the day-night cycle starts paused at timeOfDay */

    glm::vec3 fogColor = glm::vec3(0.0f);
    float fogDensity = 0.0f;            /* 0 disables fog */
    float fogHeightFalloff = 0.0f;      /* fog thins out above fogBaseHeight */
    float fogBaseHeight = 0.0f;
    float fogMoonGlow = 0.0f;           /* haze brightens toward the moon (forward scattering) */
    float fogLightGlow = 1.0f;          /* point lights glow in the fog (0 = off) */
    /* the camera's lens (HDR): grime lit by bright light, colour fringes at
     * the edges, film grain (0 = off) */
    bool reflections = true;            /* screen-space reflections on glossy surfaces (HDR + PBR) */
    float autoExposure = 0.6;           /* the eye adapts to bright and dark (0 = off) */
    bool temporalAA = true;             /* temporal anti-aliasing (steadier edges, wires, grass) */
    float lensDirt = 0.5f;
    float chromaticAberration = 0.35f;
    float filmGrain = 0.03f;
    /* distance cut-off on top of the haze: from fogStart the fog closes in
     * exponentially and is fully opaque at fogEnd, which also becomes the
     * camera's far plane (draw distance). 0 = off (the original 5000 far plane) */
    /* drifting cloud layer in the sky, and its shadows on the ground
     * (directional light). 0 = no clouds */
    float cloudCoverage = 0.0f;
    float cloudSpeed = 14.0f;           /* world units per second, for clouds at 1500 (higher ones in proportion) */
    float cloudHeight = 1500.0f;        /* world units: higher slides past less as the camera moves */
    float wind = 1.0f;                  /* how hard swaying things (trees, grass) move; blows like the clouds */
    float fogStart = 0.0f;
    float fogEnd = 0.0f;

    float ambient = 0.1f;               /* added to the light accumulation */

    /* post-processing (all off by default) */
    bool hdr = false;                   /* filmic tone mapping instead of clipping at 1.0 */
    bool pbr = false;                   /* physically based shading (roughness / metallic materials) */
    float exposure = 1.0f;
    float dayExposure = -1.0f;          /* exposure by day (blended with the sun); < 0: same as exposure */
    float saturation = 1.0f;            /* colour grade (needs hdr): 1 = untouched */
    float grade = 0.0f;                 /* 0..1: cool shadows, warm highlights, vignette */
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
