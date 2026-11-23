#version 330

/* Planar water (ported from World2's waterShader.fs): DuDv-distorted
 * reflection + refraction, normal map, plus Fresnel, depth murk, soft
 * shorelines, light glints and fog for the deferred pipeline. */

layout(location = 0) out vec4 outColor;

in vec3 worldPosition;
in vec2 waveCoords;

uniform sampler2D reflectionSampler;
uniform sampler2D refractionSampler;
uniform sampler2D dudvSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D depthSampler;
uniform sampler2D particlesSampler;
uniform sampler2D volumetricLightSampler;
uniform sampler2D flareSampler;
uniform sampler2D reflectionParticlesSampler; /* mirrored billboards / flames, premultiplied */

uniform mat4 projectionMatrix;
uniform vec2 screenSize;
uniform vec3 cameraPosition;
uniform float time;
uniform int hasReflection;

uniform float waveStrength, waveSpeed;
uniform vec3 shallowColor, deepColor;
uniform float murkDepth, softEdge, reflectivity;
uniform vec3 ambientSky;       /* sky light: lights the water body by day */
uniform float reflectionGlow;   /* bright reflections bloom into a soft, rippling streak */

uniform vec3 moonDirection, moonColor;
uniform vec3 holeGlintDirection, holeGlintColor;   /* a sky black hole's disk (0: none) */
uniform vec3 fogColor;
uniform float fogDensity, fogHeightFalloff, fogBaseHeight, fogMoonGlow;

const int MAX_LIGHTS = 16;
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform vec4 lightSpots[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

float linearDepth(float d) {
    return projectionMatrix[3][2] / ((2.0 * d - 1.0) + projectionMatrix[2][2]);
}

uniform float fogStart, fogEnd;
/* distance cut-off: 0 before fogStart, closes in exponentially and is
 * exactly 1 (fully fogged) at fogEnd, the far plane. fogEnd 0 = off */
float fogCutoff(float dist) {
    if (fogEnd <= 0.0) return 0.0;
    float d = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    return (1.0 - exp(-4.0 * d * d)) / (1.0 - exp(-4.0));
}

float fogAmount(float dist, float height) {
    float haze = 0.0;
    if (fogDensity > 0.0) {
        float heightFactor = exp(-max(height - fogBaseHeight, 0.0) * fogHeightFalloff);
        haze = clamp((1.0 - exp(-dist * fogDensity)) * heightFactor, 0.0, 1.0);
    }
    return 1.0 - (1.0 - haze) * (1.0 - fogCutoff(dist));
}

/* ---- wind waves: a sum of wave trains around the wind direction, from ~40
 * units long down to ~half a unit, each moving at its deep-water speed. A
 * train too short for the pixels it covers is not drawn (it would only
 * shimmer); its slopes go into the surface roughness instead, which spreads
 * the sun into a glitter path the way real water does ---- */
uniform vec2 windDirection;    /* xz, length = the map's wind strength */
const int WAVES = 28;
const float GRAVITY = 140.0;   /* world units / s^2 (a unit is ~7 cm) */

float waterHash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float waterNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(waterHash(i), waterHash(i + vec2(1, 0)), u.x),
               mix(waterHash(i + vec2(0, 1)), waterHash(i + vec2(1, 1)), u.x), u.y);
}

vec2 waveSlopes(vec2 p, float t, float footprint, out float unresolved) {
    float wind = length(windDirection);
    vec2 w = wind > 1e-3 ? windDirection / wind : vec2(1.0, 0.0);
    float strength = clamp(wind, 0.25, 3.5);
    /* cat's paws: gusts roughen the water in patches that drift downwind */
    vec2 drift = p - w * t * 9.0;
    float gust = waterNoise(drift * 0.007) * 0.65 + waterNoise(drift * 0.019 + 7.3) * 0.35;
    strength *= mix(0.35, 1.45, smoothstep(0.25, 0.75, gust));
    /* sine trains have dead straight crests that cross into a lattice: bend
     * them with a slow large-scale warp */
    p += (vec2(waterNoise(p * 0.011), waterNoise(p * 0.011 + 31.7)) - 0.5) * 30.0 +
         (vec2(waterNoise(p * 0.043 + 5.1), waterNoise(p * 0.043 + 12.9)) - 0.5) * 6.0;
    vec2 slope = vec2(0.0);
    unresolved = 0.0;
    float wavelength = 30.0;
    /* several waves at each size (a slow 0.88 step), not one: one wave per size
     * meant two straight trains crossing at each scale, a diamond lattice.
     * Same total energy as 16 waves */
    float energy = sqrt(16.0 / float(WAVES));
    for (int i = 0; i < WAVES; i++) {
        float h1 = fract(sin(float(i) * 12.9898 + 1.3) * 43758.5453);
        float h2 = fract(sin(float(i) * 78.233 + 4.1) * 24634.6345);
        float h3 = fract(sin(float(i) * 39.425 + 7.7) * 31415.9265);
        /* directions spread all round (golden angle steps), mostly downwind */
        float angle = float(i) * 2.39996 + (h1 - 0.5) * 1.3;
        vec2 d = vec2(cos(angle), sin(angle));
        float downwind = dot(d, w);
        if (downwind < 0.0) {
            d = -d;               /* waves run with the wind... */
            downwind = -downwind;
        }
        float spread = 0.35 + 0.65 * downwind * downwind;   /* ...and crosswind ones are weaker */
        float lambda = wavelength * (0.85 + 0.3 * h2);
        float k = 6.2831853 / lambda;
        float omega = sqrt(GRAVITY * k);
        /* a lake's chop: long waves gentle, the energy in the short ones */
        float steepness = mix(0.025, 0.09, smoothstep(30.0, 4.0, lambda)) * spread * strength *
                          waveStrength / 0.016;
        float phase = dot(d, p) * k - omega * t + h2 * 6.2831853;
        float resolved = smoothstep(3.0, 8.0, lambda / max(footprint, 1e-4));
        /* wave groups: each train comes and goes in patches a few wavelengths
         * across (drifting with it), instead of running on forever uniformly */
        vec2 g = (p - d * omega / k * 0.5 * t) / (lambda * 4.5) + vec2(h1, h3) * 57.0;
        float group = smoothstep(0.15, 0.85, waterNoise(g));
        steepness *= energy * (0.25 + 1.5 * group);
        slope += d * steepness * cos(phase) * resolved;
        unresolved += 0.5 * steepness * steepness * (1.0 - resolved);
        wavelength *= 0.88;
    }
    return slope;
}

/* GGX highlight of a light of color c from direction L */
vec3 glint(vec3 n, vec3 V, vec3 L, vec3 c, float alpha2) {
    vec3 H = normalize(L + V);
    float NdotH = max(dot(n, H), 0.0);
    float NdotL = max(dot(n, L), 0.0);
    float denominator = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (3.14159265 * denominator * denominator);
    float F = 0.02 + 0.98 * pow(1.0 - max(dot(V, H), 0.0), 5.0);
    return c * D * F * NdotL / max(4.0 * max(dot(n, V), 0.05) * NdotL, 1e-3) * step(0.0, NdotL - 1e-4);
}

void main() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    /* world size of this pixel (before any discard: derivatives) */
    vec2 footprintXZ = fwidth(worldPosition.xz);
    float footprint = max(footprintXZ.x, footprintXZ.y);
    float sceneDepth = texture(depthSampler, uv).r;
    if (gl_FragCoord.z > sceneDepth) {
        discard; /* terrain / objects in front of the water */
    }
    float surfaceZ = linearDepth(gl_FragCoord.z);
    float waterDepth = linearDepth(sceneDepth) - surfaceZ;

    float unresolved;
    vec2 slope = waveSlopes(worldPosition.xz, time, footprint, unresolved);
    vec3 normal = normalize(vec3(-slope.x, 1.0, -slope.y));
    vec2 distortion = slope * 0.03 * clamp(waterDepth / 20.0, 0.0, 1.0);

    vec3 toCamera = cameraPosition - worldPosition;
    float distanceToCamera = length(toCamera);
    vec3 V = toCamera / distanceToCamera;
    /* roughness (GGX alpha^2): calm water is nearly a mirror; slopes too
     * small to draw, and distance, roughen it */
    float alpha2 = 0.0006 + 2.0 * unresolved + 0.0035 * smoothstep(30.0, 500.0, distanceToCamera);

    /* refraction: don't pull in geometry that is in front of the water */
    vec2 refractionUV = clamp(uv + distortion, vec2(0.001), vec2(0.999));
    float refractionDepth = texture(depthSampler, refractionUV).r;
    if (refractionDepth < gl_FragCoord.z) {
        refractionUV = uv;
        refractionDepth = sceneDepth;
    }
    vec3 refraction = texture(refractionSampler, refractionUV).rgb * shallowColor;
    float murk = clamp((linearDepth(refractionDepth) - surfaceZ) / murkDepth, 0.0, 1.0);
    /* deep water is lit from inside by the sky and sun: teal-blue by day,
     * near black at night */
    float daylight = clamp(dot(ambientSky, vec3(0.3333)) * 2.6, 0.0, 1.0);
    vec3 deep = deepColor + vec3(0.03, 0.12, 0.14) * daylight;
    refraction = mix(refraction, deep, murk);

    vec3 reflection = deepColor;
    vec3 sharpReflection = vec3(0.0);   /* unblurred: picks out small bright sources */
    vec3 reflectionHalo = vec3(0.0);
    vec4 reflectedParticles = vec4(0.0);
    if (hasReflection != 0) {
        vec2 reflectionUV = clamp(uv + distortion, vec2(0.001), vec2(0.999));
        /* slightly soft: a mirror-sharp edge on a lamp reads as a cut-out disc.
         * And streaked: ripples tilt the surface towards and away from the
         * viewer, smearing every reflection (trees, hills, the sky) into
         * vertical streaks, longer on rougher and more distant water */
        float lod = 0.7 + clamp(sqrt(alpha2) * 12.0, 0.0, 3.0);
        float stretch = clamp(0.004 + sqrt(alpha2) * 0.3, 0.0, 0.03);
        reflection = vec3(0.0);
        float weights = 0.0;
        for (int k = -3; k <= 3; k++) {
            float w = exp(-0.25 * float(k * k));
            vec2 tap = clamp(reflectionUV + vec2(0.0, float(k) * stretch), vec2(0.001), vec2(0.999));
            reflection += textureLod(reflectionSampler, tap, lod).rgb * w;
            weights += w;
        }
        reflection /= weights;
        sharpReflection = textureLod(reflectionSampler, reflectionUV, 0.0).rgb;
        reflectedParticles = texture(reflectionParticlesSampler, reflectionUV);
        if (reflectionGlow > 0.0) {
            /* light scattered by the ripples: the bright parts of the blurred
             * reflection (mip chain), stretched into a vertical streak towards
             * the viewer and wobbling with the waves */
            vec2 wobble = slope * 0.04;
            for (int k = -3; k <= 3; k++) {
                float w = exp(-0.35 * float(k * k));
                vec2 tap = clamp(reflectionUV + wobble + vec2(0.0, float(k) * 0.028), vec2(0.001), vec2(0.999));
                vec3 blurred = textureLod(reflectionSampler, tap, 3.5).rgb * 0.6 +
                               textureLod(reflectionSampler, tap, 5.0).rgb * 0.4;
                /* only light sources (HDR values above ~1) glow, not the sky or
                 * lit surfaces: soft-knee threshold on the brightness */
                float brightness = max(blurred.r, max(blurred.g, blurred.b));
                float over = max(brightness - 0.9, 0.0);
                reflectionHalo += blurred * (over * over / (over + 0.6) / max(brightness, 1e-3)) * w;
            }
            reflectionHalo *= reflectionGlow * 0.35;
        }
    }

    float cosTheta = max(dot(V, normalize(mix(vec3(0, 1, 0), normal, 0.3))), 0.0);
    float fresnel = clamp((0.02 + 0.98 * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0)) * reflectivity, 0.0, 1.0);
    /* soft shoreline: the water body fades out over softEdge, but the surface
     * (reflection, glints) stays until the last half unit, so shallow flats
     * still read as water. The body is faded before anything is layered on
     * it (fading it afterwards by subtraction went negative, black, under
     * reflected billboards) */
    float edge = clamp(waterDepth / softEdge, 0.0, 1.0);
    float surfaceEdge = clamp(waterDepth / 0.5, 0.0, 1.0);
    vec3 behind = texture(refractionSampler, uv).rgb;
    refraction = mix(behind, refraction, edge);
    vec3 color = mix(refraction, reflection, fresnel);
    /* the glow is light scattered off the surface: not weighted by Fresnel */
    color += reflectionHalo;
    /* reflected billboards, composited like the main pass composites
     * particles (premultiplied; additive ones have alpha 0) */
    float particleReflect = mix(0.75, 1.0, fresnel);
    /* light sources (glowing globes, flames) reflect as strongly as their
     * billboards do: by Fresnel alone a lamp globe looking down was a few
     * percent, a dark hole in the middle of its bright reflected halo */
    float source = smoothstep(0.7, 1.3, max(sharpReflection.r, max(sharpReflection.g, sharpReflection.b)));
    color += sharpReflection * source * max(particleReflect - fresnel, 0.0);
    color = reflectedParticles.rgb * particleReflect + color * (1.0 - reflectedParticles.a * particleReflect);

    /* glints: the sky's light (sun by day, moon by night) and nearby lights */
    vec3 specular = glint(normal, V, normalize(moonDirection), moonColor * 1.5, alpha2);
    /* the black hole's disk: sparkles on the wave facets that face it, and
     * a broader sheen, together a glittering path */
    if (dot(holeGlintColor, holeGlintColor) > 0.0) {
        specular += glint(normal, V, holeGlintDirection, holeGlintColor, alpha2 + 0.002)
                  + glint(normal, V, holeGlintDirection, holeGlintColor * 0.3, alpha2 + 0.008);
    }
    vec3 scatter = vec3(0.0);
    for (int i = 0; i < numLights; i++) {
        vec3 L = lightPositions[i] - worldPosition;
        float d = length(L);
        float att = clamp(1.0 - d / lightRadii[i], 0.0, 1.0);
        if (att <= 0.0) continue;
        L /= d;
        if (lightSpots[i].w >= -1.0 && dot(-L, lightSpots[i].xyz) < lightSpots[i].w) continue;
        att *= att;
        float NdotL = max(dot(normal, L), 0.0);
        specular += glint(normal, V, L, lightColors[i] * att * 0.6, alpha2 + 0.004)
                  /* a broad sheen: the ripples spread a lamp's highlight into a
                   * soft glossy patch (a bare light has no globe to reflect) */
                  + glint(normal, V, L, lightColors[i] * att * 0.6, alpha2 + 0.05);
        /* light scattered back out of the water body (murky water glows
         * under a lamp), on top of the lit bed seen through clear water */
        scatter += lightColors[i] * att * (0.15 + 0.85 * NdotL) * (1.0 - 0.6 * fresnel);
    }
    /* the water's own hue (its colors are dark: use their tint, not their level) */
    vec3 hue = shallowColor / max(max(shallowColor.r, max(shallowColor.g, shallowColor.b)), 1e-3);
    color += scatter * hue * mix(0.9, 0.45, murk) * edge;
    color = mix(behind, color, surfaceEdge);
    color += specular * (1.0 - murk * 0.3) * surfaceEdge;

    vec3 hazeColor = fogColor + moonColor * fogMoonGlow * pow(max(dot(-V, moonDirection), 0.0), 6.0);
    color = mix(color, hazeColor, fogAmount(distanceToCamera, worldPosition.y));

    /* re-apply what the combination pass composited over the scene */
    vec4 particles = texture(particlesSampler, uv);
    vec4 volumetric = texture(volumetricLightSampler, uv);
    vec4 flare = texture(flareSampler, uv);
    color = particles.rgb + color * (1.0 - particles.a);
    color = volumetric.rgb + color * (1.0 - volumetric.a);
    color = flare.rgb * flare.a + color;

    outColor = vec4(color, 1.0);
}
