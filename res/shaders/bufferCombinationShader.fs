#version 330

in vec2 textureCoords;

layout(location = 0) out vec3 outColor;

uniform sampler2D normalSampler;
uniform sampler2D depthSampler;
uniform sampler2D lightAccumulationSampler;
uniform sampler2D colorSampler;
uniform sampler2D blurredLightAccSampler;
uniform sampler2D spotLightDepthMap;
uniform sampler2D particlesSampler;
uniform sampler2D dirLightDepthSampler;
uniform sampler2D volumetricLightSampler;
uniform sampler2D flareSampler;
uniform float flareInComposite;   /* 1: the lens flare is added here; 0: later, by the tone mapping */
uniform sampler2D flareBlurSampler;
uniform sampler2D roughnessSampler;

uniform int outputType;

uniform float ambient;           /* engine default 0.1 */
uniform vec3 ambientSky;         /* hemisphere ambient: up-facing ... */
uniform vec3 ambientGround;      /* ... and down-facing surfaces */
uniform sampler2D aoSampler;     /* screen-space ambient occlusion (1 = open) */
uniform float aoStrength;        /* 0 = off */
uniform bool hdrEnabled;         /* false: light is clamped at 1.0 like the old 8-bit buffer */
uniform bool pbrEnabled;         /* light buffer already includes albedo; roughness target = (roughness, metallic) */
uniform bool skyReflections;     /* reflect the sky cube (blurrier for rougher surfaces) */
uniform samplerCube skyCube;
uniform float skyCubeMaxLod;


uniform float emissiveStrength;
uniform vec3 fogColor;
uniform float fogDensity;        /* 0: no fog */
uniform float fogHeightFalloff;
uniform float fogBaseHeight;
uniform mat4 inverseProjectionMatrix;
uniform mat4 inverseViewMatrix;
uniform float fogMoonGlow;
uniform vec3 moonDirection, moonColor;

/* screen-space reflections: the reflected ray marched through the depth
 * buffer; where it meets something on screen, that point's colour from the
 * previous frame (the finished HDR image). Glossy surfaces only (wet ground,
 * puddles, polished things); elsewhere, and where the ray leaves the screen,
 * the sky cube stands in */
uniform bool ssrEnabled;
uniform sampler2D previousFrameSampler;
uniform mat4 projectionMatrix;

float viewDepthAt(vec2 uv) {
    float d = texture(depthSampler, uv).x;
    vec4 v = inverseProjectionMatrix * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return d >= 1.0 ? -1e9 : v.z / v.w;
}

vec3 screenReflection(vec3 P, vec3 N, vec3 V, out float weight) {
    weight = 0.0;
    vec3 R = normalize(reflect(-V, N));
    /* rays coming back towards the camera hit what is behind it: off screen */
    float facing = 1.0 - smoothstep(-0.05, 0.45, R.z);
    if (facing <= 0.0) return vec3(0.0);
    float previousT = 0.0;
    const int STEPS = 40;
    const float MAX_T = 650.0;
    for (int i = 1; i <= STEPS; i++) {
        float t = 2.5 + float(i * i) * (MAX_T / float(STEPS * STEPS));
        vec3 Q = P + R * t;
        vec4 clip = projectionMatrix * vec4(Q, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;
        float behind = viewDepthAt(uv) - Q.z;               /* > 0: the ray went behind what is seen */
        /* crossed into what is seen since the last step: find where, then judge the thickness there.
         * (Judged at the step, a step longer than the thickness jumped from in front of a wall seen
         * edge-on to far behind it: no hit, and a room's floor reflected nothing) */
        if (behind > 0.0) {
            float lo = previousT, hi = t;
            for (int k = 0; k < 6; k++) {
                float mid = 0.5 * (lo + hi);
                vec3 M = P + R * mid;
                vec4 c = projectionMatrix * vec4(M, 1.0);
                vec2 u = c.xy / c.w * 0.5 + 0.5;
                if (viewDepthAt(u) - M.z > 0.0) hi = mid; else lo = mid;
            }
            vec3 H = P + R * hi;
            vec4 hc = projectionMatrix * vec4(H, 1.0);
            uv = hc.xy / hc.w * 0.5 + 0.5;
            /* behind something thin, not into it: the ray goes on */
            if (viewDepthAt(uv) - H.z > max(3.0, hi * 0.06)) {
                previousT = t;
                continue;
            }
            vec2 edge = min(uv, 1.0 - uv);
            weight = smoothstep(0.0, 0.07, edge.x) * smoothstep(0.0, 0.07, edge.y) *
                     (1.0 - smoothstep(0.55, 1.0, hi / MAX_T)) * facing;
            return textureLod(previousFrameSampler, uv, 0.0).rgb;
        }
        previousT = t;
    }
    return vec3(0.0);
}

/* PBR: lit color = direct light (from the light passes) + ambient light,
 * diffuse for dielectrics plus a Fresnel-weighted specular share from the sky */
vec3 pbrComposite(vec3 albedo) {
    vec3 direct = texture(lightAccumulationSampler, textureCoords).xyz;
    if (!hdrEnabled) {
        direct = min(direct, vec3(1.0));
    }
    vec3 surface = texture(roughnessSampler, textureCoords).rgb;
    float roughness = clamp(surface.r, 0.05, 1.0);
    float metallic = clamp(surface.g, 0.0, 1.0);

    vec3 nEye = texture(normalSampler, textureCoords).xyz * 2.0 - 1.0;
    float d = texture(depthSampler, textureCoords).x;
    vec4 view = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec3 V = normalize(-view.xyz / view.w);
    vec3 N = length(nEye) > 0.1 ? normalize(nEye) : V;
    float NdotV = max(dot(N, V), 0.0);
    vec3 worldN = normalize(mat3(inverseViewMatrix) * N);

    float occlusion = aoStrength > 0.0 ? mix(1.0, texture(aoSampler, textureCoords).r, aoStrength) : 1.0;
    vec3 hemisphere = mix(ambientGround, ambientSky, worldN.y * 0.5 + 0.5);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    vec3 diffuseAmbient = (1.0 - F) * (1.0 - metallic) * albedo * hemisphere;
    vec3 specularAmbient;
    if (skyReflections) {
        vec3 worldV = normalize(mat3(inverseViewMatrix) * V);
        vec3 R = reflect(-worldV, worldN);
        /* reflections pointing into the surface would be the ground: fade them */
        float horizon = clamp(1.0 + dot(R, worldN), 0.0, 1.0);
        vec3 sky = textureLod(skyCube, R, roughness * (skyCubeMaxLod - 1.0)).rgb;
        specularAmbient = F * sky * horizon * horizon;
        if (ssrEnabled && roughness < 0.3) {
            float hit;
            vec3 reflected = screenReflection(view.xyz / view.w, N, V, hit);
            hit *= smoothstep(0.3, 0.08, roughness);
            specularAmbient = mix(specularAmbient, F * reflected, hit);
        }
    } else {
        specularAmbient = F * ambientSky * (1.0 - 0.6 * roughness);
    }
    return direct + (diffuseAmbient + specularAmbient) * occlusion;
}

uniform float fogStart, fogEnd;

/* point lights glowing in the fog: the light each one scatters towards the
 * eye along the view ray, in closed form. With the falloff 1/r^2 - 1/R^2
 * (zero at the light's radius R), for the ray o + t d past a light at
 * distance h:  int dt / ((t + b)^2 + h^2) = atan((t + b) / h) / h  */
const int MAX_GLOW_LIGHTS = 16;
uniform int glowLightCount;
uniform vec3 glowLightPositions[MAX_GLOW_LIGHTS];
uniform vec3 glowLightColors[MAX_GLOW_LIGHTS];
uniform float glowLightRadii[MAX_GLOW_LIGHTS];
uniform float fogLightGlow;      /* strength; 0 = off */

vec3 lightGlow(vec3 o, vec3 d, float tMax) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < glowLightCount; i++) {
        vec3 oc = o - glowLightPositions[i];
        float R = glowLightRadii[i];
        float b = dot(oc, d);
        /* (the bulb is not a point: no brightening closer than ~7 units, or
         * the halo balloons when the camera walks past a lamp) */
        float h2 = max(dot(oc, oc) - b * b, 49.0);
        if (h2 >= R * R) continue;
        float s = sqrt(R * R - h2);
        float t0 = max(0.0, -b - s), t1 = min(tMax, -b + s);
        if (t1 <= t0) continue;
        float h = sqrt(h2);
        float scattered = (atan((t1 + b) / h) - atan((t0 + b) / h)) / h - (t1 - t0) / (R * R);
        /* the fog's density where the light is */
        float density = fogDensity * exp(-max(glowLightPositions[i].y - fogBaseHeight, 0.0) * fogHeightFalloff);
        sum += glowLightColors[i] * max(scattered, 0.0) * density;
    }
    return sum * fogLightGlow * 330.0;
}

/* the glow along this pixel's ray, up to depth d (1: the sky, as far as the fog reaches) */
vec3 fogLightGlowAt(float d) {
    if (glowLightCount == 0 || fogLightGlow <= 0.0 || fogDensity <= 0.0) return vec3(0.0);
    vec4 view = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    view /= view.w;
    vec3 rayDir = normalize(mat3(inverseViewMatrix) * view.xyz);
    float tMax = d >= 1.0 ? (fogEnd > 0.0 ? fogEnd : 4000.0) : length(view.xyz);
    return lightGlow(inverseViewMatrix[3].xyz, rayDir, tMax);
}
/* distance cut-off: 0 before fogStart, closes in exponentially and is
 * exactly 1 (fully fogged) at fogEnd, the far plane. fogEnd 0 = off */
float fogCutoff(float dist) {
    if (fogEnd <= 0.0) return 0.0;
    float d = clamp((dist - fogStart) / max(fogEnd - fogStart, 1.0), 0.0, 1.0);
    return (1.0 - exp(-4.0 * d * d)) / (1.0 - exp(-4.0));
}

/* fog factor for the pixel at textureCoords with depth buffer value d;
 * also returns the fog color, brightened toward the moon */
float fogAmount(float d, out vec3 color) {
    color = fogColor;
    if (fogDensity <= 0.0 && fogEnd <= 0.0) return 0.0;
    vec4 view = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    view /= view.w;
    vec3 world = (inverseViewMatrix * view).xyz;
    vec3 rayDir = normalize(mat3(inverseViewMatrix) * view.xyz);
    color += moonColor * fogMoonGlow * pow(max(dot(rayDir, moonDirection), 0.0), 6.0);
    float haze = 0.0;
    if (fogDensity > 0.0) {
        float heightFactor = exp(-max(world.y - fogBaseHeight, 0.0) * fogHeightFalloff);
        haze = clamp((1.0 - exp(-length(view.xyz) * fogDensity)) * heightFactor, 0.0, 1.0);
    }
    return 1.0 - (1.0 - haze) * (1.0 - fogCutoff(length(view.xyz)));
}

/* with a fog cut-off the sky's horizon band is the fog itself, so geometry
 * fading out at the draw distance melts into the sky instead of leaving a
 * silhouette */
float skyHaze(out vec3 color) {
    color = fogColor;
    if (fogEnd <= 0.0) return 0.0;
    vec4 view = inverseProjectionMatrix * vec4(textureCoords * 2.0 - 1.0, 1.0, 1.0);
    vec3 rayDir = normalize(mat3(inverseViewMatrix) * (view.xyz / view.w));
    color += moonColor * fogMoonGlow * pow(max(dot(rayDir, moonDirection), 0.0), 6.0);
    return 1.0 - smoothstep(-0.02, 0.22, rayDir.y);
}

vec4 volumetrics() {
    return texture(volumetricLightSampler, textureCoords);
}

vec4 particles () {
    return texture (particlesSampler, textureCoords);
}

vec3 depth() {
    float t2 = pow(texture(depthSampler, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}
vec3 spotLightDepth() {
    float t2 = pow(texture(spotLightDepthMap, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}
vec3 color() {
    vec3 albedoColor = texture(colorSampler, textureCoords).xyz;

    return albedoColor;
}
vec3 worldNormal() {
    return texture(normalSampler, textureCoords).xyz;
}
vec3 lightAccumulation() {
    vec3 lightAcc = texture (lightAccumulationSampler, textureCoords).xyz;
    if (!hdrEnabled) {
        lightAcc = min(lightAcc, vec3(1.0));
    }
    vec3 n = texture(normalSampler, textureCoords).xyz * 2.0 - 1.0;
    float up = length(n) > 0.1 ? normalize(mat3(inverseViewMatrix) * n).y : 1.0;
    float occlusion = aoStrength > 0.0 ? mix(1.0, texture(aoSampler, textureCoords).r, aoStrength) : 1.0;
    return lightAcc + mix(ambientGround, ambientSky, up * 0.5 + 0.5) * occlusion;	//diffuse + specular + ambiental
}

vec3 dirLightDepthMap() {
    float t2 = pow(texture(dirLightDepthSampler, textureCoords).x , 256);
    return vec3(t2, t2, t2);
}
vec4 flareMap() {
    return texture(flareSampler, textureCoords);
}
vec4 flareBlurMap() {
    return texture(flareBlurSampler, textureCoords);
}
vec3 roughnessMap() {
    return texture(roughnessSampler, textureCoords).xyz;
}

void main() {
    if (outputType == 1) outColor = color();
	if (outputType == 2) outColor = worldNormal();
    if (outputType == 3) outColor = depth();
	if (outputType == 4) outColor = lightAccumulation();
	if (outputType == 5) {
        vec4 particlesPixel = particles();
        vec4 volumetricPixel = volumetrics();
        vec3 albedoPixel = color();
        vec4 flarePixel = flareMap() * flareInComposite;   /* 0: added after the black holes (tonemap) */

        vec3 depthVal = depth();

        vec3 lightAcc = lightAccumulation();
        float exposure = 0.5f;
        // exposure tone mapping
        vec3 mapped = vec3(1.0) - exp(-lightAcc * exposure);

        if (depthVal.x != 1) {
            vec4 albedoEmissive = texture(colorSampler, textureCoords);
            float emissive = 1.0 - albedoEmissive.a;
            vec3 finalColor = (pbrEnabled ? pbrComposite(albedoPixel) : albedoPixel * lightAccumulation()) +
                              albedoPixel * emissive * emissiveStrength;
            vec3 hazeColor;
            float haze = fogAmount(texture(depthSampler, textureCoords).x, hazeColor);
            finalColor = mix(finalColor, hazeColor, haze);
            finalColor += fogLightGlowAt(texture(depthSampler, textureCoords).x);
            finalColor = particlesPixel.xyz + finalColor * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = finalColor;// * max(0.3, (1.0 - depthVal.x));
        } else {
            vec3 skyHazeColor;
            float horizonHaze = skyHaze(skyHazeColor);
            albedoPixel = mix(albedoPixel, skyHazeColor, horizonHaze);
            albedoPixel += fogLightGlowAt(1.0);
            vec3 finalColor = particlesPixel.xyz + albedoPixel * (1.0 - particlesPixel.a);
            finalColor = volumetricPixel.xyz + finalColor * (1.0 - volumetricPixel.a);
            finalColor = flarePixel.xyz * flarePixel.a + finalColor;
            outColor = finalColor;
        }

        //float gamma = 1.0f / 2.2f;
        //outColor.rgb = pow(outColor.rgb, vec3(gamma));
    }
    if (outputType == 6) outColor = lightAccumulation();
    if (outputType == 7) outColor = spotLightDepth();
    if (outputType == 8) outColor = dirLightDepthMap();
    //if (outputType == 9) outColor = volumetrics();
    if (outputType == 10) outColor = roughnessMap();
    if (outputType == 9) outColor = vec3(texture(aoSampler, textureCoords).r);
}
