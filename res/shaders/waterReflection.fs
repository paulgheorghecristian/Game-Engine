#version 330

layout(location = 0) out vec4 outColor;

struct Material {
    vec3 diffuse;
    bool isBlackAlpha;
    float emissive;
};

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 textureCoords;

uniform Material material;
uniform sampler2D textureSampler;
uniform bool hasTexture;
uniform vec3 cameraPosition;

/* ---- terrain material (world-space tiled, slope / height blended) ---- */
uniform bool terrainEnabled;
uniform bool terrainHasRockNormal;
uniform sampler2D terrainGrass, terrainGrass2, terrainRock, terrainRockNormal, terrainDirt, terrainSoil;
uniform float terrainTileSize;
uniform float terrainShoreHeight;

float terrainHash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float terrainNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(terrainHash(i), terrainHash(i + vec2(1, 0)), u.x),
               mix(terrainHash(i + vec2(0, 1)), terrainHash(i + vec2(1, 1)), u.x), u.y);
}

/* large-scale variation (hides texture repetition) */
float terrainMacro(vec2 p) {
    return terrainNoise(p * 0.0035) * 0.65 + terrainNoise(p * 0.011) * 0.35;
}

/* albedo + (world) normal + specular strength for a terrain point */
vec3 terrainSurface(vec3 position, vec3 geometricNormal, bool wantNormal, out vec3 normal, out float specular) {
    vec3 n = normalize(geometricNormal);
    float scale = 1.0 / terrainTileSize;
    vec2 uv = position.xz * scale;
    float macro = terrainMacro(position.xz);

    /* grass: two textures and two scales, mixed by the macro noise */
    vec3 grassNear = texture(terrainGrass2, uv).rgb;
    vec3 grassAlt = texture(terrainGrass, uv * 1.37 + vec2(0.31, 0.17)).rgb;
    vec3 grassFar = texture(terrainGrass2, uv * 0.23 + vec2(0.5)).rgb;
    vec3 grass = mix(grassNear, grassAlt, smoothstep(0.35, 0.7, macro));
    grass = mix(grass, grassFar, 0.35);

    /* rock: triplanar so steep slopes never stretch */
    vec3 w = pow(abs(n), vec3(4.0));
    w /= (w.x + w.y + w.z);
    float rockScale = scale * 1.6;
    vec3 rockNear = texture(terrainRock, position.zy * rockScale).rgb * w.x +
                    texture(terrainRock, position.xz * rockScale).rgb * w.y +
                    texture(terrainRock, position.xy * rockScale).rgb * w.z;
    /* a second, larger scale (rotated offset) breaks up the repetition */
    float farScale = rockScale * 0.29;
    vec3 rockFar = texture(terrainRock, position.zy * farScale + vec2(0.37, 0.11)).rgb * w.x +
                   texture(terrainRock, position.xz * farScale + vec2(0.37, 0.11)).rgb * w.y +
                   texture(terrainRock, position.xy * farScale + vec2(0.37, 0.11)).rgb * w.z;
    vec3 rock = mix(rockNear, rockFar, 0.45 + 0.3 * (macro - 0.5));

    /* shoreline: wet mud at the water, soil just above it */
    float h = position.y - terrainShoreHeight;
    vec3 shore = mix(texture(terrainDirt, uv * 2.1).rgb * 0.75, texture(terrainSoil, uv * 1.3).rgb,
                     smoothstep(-2.0, 7.0, h));

    float slope = 1.0 - n.y;
    float rockAmount = smoothstep(0.28, 0.5, slope + (macro - 0.5) * 0.2);
    float shoreAmount = 1.0 - smoothstep(4.0, 16.0, h + (macro - 0.5) * 8.0);

    vec3 albedo = mix(grass, shore, shoreAmount);
    albedo = mix(albedo, rock, rockAmount);
    albedo *= mix(0.82, 1.12, macro);

    specular = mix(0.06, 0.22, rockAmount) + (1.0 - smoothstep(-1.0, 3.0, h)) * 0.4;

    normal = n;
    if (wantNormal && terrainHasRockNormal && rockAmount > 0.01) {
        /* triplanar normal mapping (UDN blend) on the rock */
        vec3 tx = texture(terrainRockNormal, position.zy * rockScale).xyz * 2.0 - 1.0;
        vec3 ty = texture(terrainRockNormal, position.xz * rockScale).xyz * 2.0 - 1.0;
        vec3 tz = texture(terrainRockNormal, position.xy * rockScale).xyz * 2.0 - 1.0;
        vec3 nx = vec3(0.0, tx.y, tx.x) * 0.8;
        vec3 ny = vec3(ty.x, 0.0, ty.y) * 0.8;
        vec3 nz = vec3(tz.x, tz.y, 0.0) * 0.8;
        vec3 rockNormal = normalize(n + nx * w.x + ny * w.y + nz * w.z);
        normal = normalize(mix(n, rockNormal, rockAmount));
    }
    return albedo;
}


uniform vec3 sunDirection, sunColor; /* the scene's directional light */
uniform float ambient, emissiveStrength;
uniform vec3 fogColor;
uniform float fogDensity, fogHeightFalloff, fogBaseHeight, fogMoonGlow;

const int MAX_LIGHTS = 8;
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform vec4 lightSpots[MAX_LIGHTS];
uniform float lightRadii[MAX_LIGHTS];

void main() {
    vec3 albedo = material.diffuse;
    if (terrainEnabled) {
        vec3 terrainNormal;
        float terrainSpecular;
        albedo *= terrainSurface(worldPosition, worldNormal, false, terrainNormal, terrainSpecular);
    } else if (hasTexture) {
        vec3 texel = texture(textureSampler, textureCoords).rgb;
        if (material.isBlackAlpha && (texel.r + texel.g + texel.b) / 3.0 <= 0.1) {
            discard;
        }
        albedo *= texel;
    }

    vec3 N = normalize(worldNormal);
    if (!gl_FrontFacing) N = -N;

    vec3 light = vec3(ambient) + sunColor * max(dot(N, sunDirection), 0.0);
    for (int i = 0; i < numLights; i++) {
        vec3 L = lightPositions[i] - worldPosition;
        float d = length(L);
        float att = clamp(1.0 - d / lightRadii[i], 0.0, 1.0);
        if (att <= 0.0) continue;
        L /= d;
        if (lightSpots[i].w >= -1.0 && dot(-L, lightSpots[i].xyz) < lightSpots[i].w) continue;
        light += lightColors[i] * att * att * 2.0 * max(dot(N, L), 0.0);
    }

    vec3 color = albedo * light + albedo * material.emissive * emissiveStrength;

    if (fogDensity > 0.0) {
        float dist = length(cameraPosition - worldPosition);
        float heightFactor = exp(-max(worldPosition.y - fogBaseHeight, 0.0) * fogHeightFalloff);
        color = mix(color, fogColor, clamp((1.0 - exp(-dist * fogDensity)) * heightFactor, 0.0, 1.0));
    }
    outColor = vec4(color, 1.0);
}
