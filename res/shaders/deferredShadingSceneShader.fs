#version 330

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outEyeSpaceNormal;
layout(location = 2) out vec3 outRoughness;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    float normalMapStrength;
    bool isBlackAlpha;
    float emissive; /* 0..1, stored as (1 - alpha) in the albedo target */
    float specularStrength; /* 1.0 unless a map asks for duller surfaces */
    float roughness;        /* PBR */
    float metallic;
    float tileSize;         /* > 0: world units per texture repeat, projected from three sides (no UVs) */
};

uniform sampler2D textureSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D roughnessSampler;
uniform Material material;
uniform bool hasTexture;
uniform bool hasNormalMap;
uniform bool hasRoughness;
uniform bool pbrEnabled; /* the "roughness" target holds (roughness, metallic) instead of a specular mask */
uniform mat4 viewMatrix, modelMatrix;

in vec3 eyeSpaceNormal;
in vec2 textureCoords;
in mat3 fromTangentToModelSpace;
in vec3 worldPosition;
in vec3 worldNormal;
in vec3 objectPosition, objectNormal;

/* ---- terrain material (world-space tiled, slope / height blended) ---- */
uniform bool terrainEnabled;
uniform bool terrainHasRockNormal;
uniform sampler2D terrainGrass, terrainGrass2, terrainRock, terrainRockNormal, terrainDirt, terrainSoil;
uniform float terrainTileSize;
uniform float terrainShoreHeight;
uniform sampler2D terrainGrassNormal, terrainDirtNormal, terrainSoilNormal;
uniform bool terrainHasGroundNormals;
uniform float terrainNormalStrength;
uniform float terrainVariation; /* 0..1: dry / lush patches, sparser higher up */
uniform float terrainRockDetail; /* 0..1: relief, strata and ledges on the rock */
uniform float terrainWetness;    /* 0..1: damp ground with puddles (darker, glossy: reflects) */

/* planar (xz) tangent-space normal map -> world-space perturbation */
vec3 terrainGroundBump(sampler2D map, vec2 uv) {
    vec3 t = texture(map, uv).xyz * 2.0 - 1.0;
    return vec3(t.x, 0.0, -t.y);
}

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

/* value noise and its gradient: x = value, yz = d/dp */
vec3 terrainNoiseD(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    vec2 du = 6.0 * f * (1.0 - f);
    float a = terrainHash(i), b = terrainHash(i + vec2(1, 0));
    float c = terrainHash(i + vec2(0, 1)), d = terrainHash(i + vec2(1, 1));
    float k = a - b - c + d;
    return vec3(a + (b - a) * u.x + (c - a) * u.y + k * u.x * u.y,
                du * vec2(b - a + k * u.y, c - a + k * u.x));
}

/* gradient (per world unit) of a few octaves of noise: bulges and hollows
 * from ~260 units down to ~50 */
vec2 rockReliefGradient(vec2 p) {
    vec2 g = vec2(0.0);
    float frequency = 1.0 / 260.0, amplitude = 1.0;
    for (int octave = 0; octave < 3; octave++) {
        g += terrainNoiseD(p * frequency + float(octave) * 17.3).yz * frequency * amplitude;
        frequency *= 2.2;
        amplitude *= 0.55;
    }
    return g;
}

/* the same relief projected onto the rock triplanar-wise (world vector). On
 * the side planes it is stretched downhill: erosion gullies and buttresses */
vec3 rockRelief(vec3 p, vec3 w) {
    const float STRETCH = 0.3;
    vec2 gx = rockReliefGradient(vec2(p.z, p.y * STRETCH)) * vec2(1.0, STRETCH);
    vec2 gy = rockReliefGradient(p.xz + 31.0);
    vec2 gz = rockReliefGradient(vec2(p.x, p.y * STRETCH) + 57.0) * vec2(1.0, STRETCH);
    return vec3(0.0, gx.y, gx.x) * w.x + vec3(gy.x, 0.0, gy.y) * w.y + vec3(gz.x, gz.y, 0.0) * w.z;
}

/* large-scale variation (hides texture repetition) */
float terrainMacro(vec2 p) {
    return terrainNoise(p * 0.0035) * 0.65 + terrainNoise(p * 0.011) * 0.35;
}

/* albedo + (world) normal + specular strength for a terrain point */
vec3 terrainSurface(vec3 position, vec3 geometricNormal, bool wantNormal, out vec3 normal, out float specular,
                    out float roughness) {
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
    if (terrainVariation > 0.0) {
        /* real grassland is patchy: straw-coloured dry patches, lush darker
         * hollows, and thinner, browner growth higher up the hills */
        float dryNoise = terrainNoise(position.xz * 0.0019 + vec2(3.7, 1.3)) * 0.65 +
                         terrainNoise(position.xz * 0.0071 + vec2(9.1, 4.2)) * 0.35;
        float altitude = smoothstep(terrainShoreHeight + 90.0, terrainShoreHeight + 420.0, position.y);
        float dry = clamp(smoothstep(0.48, 0.78, dryNoise) * 0.8 + altitude * 0.45, 0.0, 1.0) * terrainVariation;
        float lush = (1.0 - smoothstep(0.2, 0.45, dryNoise)) * (1.0 - altitude) * terrainVariation;
        float g = dot(grass, vec3(0.3333));
        vec3 straw = vec3(g * 1.25, g * 1.08, g * 0.62);
        grass = mix(grass, straw, dry * 0.7);
        grass = mix(grass, grass * vec3(0.78, 0.9, 0.72), lush * 0.6);
    }

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

    /* rock detail: the hills are smooth domes; give the rock relief that
     * catches the light, horizontal strata with ledges and dark crevices, and
     * grass hanging on to the ledge tops */
    vec3 rockNormalBase = n;
    float ledgeGrass = 0.0;
    if (terrainRockDetail > 0.0) {
        /* rock pushes through the soil where it stands proud (its own
         * brightness as a height): a crisp, broken edge, not a fade */
        float rockHeight = dot(rockNear, vec3(0.3333));
        float crisp = smoothstep(0.34, 0.44, slope + (macro - 0.5) * 0.2 + (rockHeight - 0.42) * 0.35);
        rockAmount = mix(rockAmount, crisp, terrainRockDetail);

        if (rockAmount > 0.01) {
            rockNormalBase = normalize(n - rockRelief(position, w) * 85.0 * terrainRockDetail);
        }

        /* strata: layers ~40 units thick, warped so they wander; only some
         * of them form a ledge, and a ledge breaks up along its length */
        float warp = (terrainNoise(position.xz * 0.003) - 0.5) * 90.0 +
                     (terrainNoise(position.xz * 0.013 + 5.1) - 0.5) * 18.0;
        float layer = (position.y + warp) / 40.0;
        float f = fract(layer);
        float layerId = floor(layer);
        float formsLedge = step(0.45, terrainHash(vec2(layerId, 3.17)));
        float along = smoothstep(0.42, 0.68, terrainNoise(position.xz * 0.012 + vec2(layerId * 7.3, layerId * 1.9)));
        float steep = smoothstep(0.3, 0.6, slope) * terrainRockDetail * step(0.01, rockAmount);
        float ledge = formsLedge * along * steep;
        float ledgeTop = smoothstep(0.66, 0.8, f) * (1.0 - smoothstep(0.92, 0.99, f)) * ledge;
        float crevice = (1.0 - smoothstep(0.0, 0.14, f) * (1.0 - smoothstep(0.94, 1.0, f))) * ledge;
        vec3 uphill = vec3(0.0, 1.0, 0.0) - n * n.y;
        uphill = length(uphill) > 1e-3 ? normalize(uphill) : vec3(0.0);
        /* ledge tops are a little flatter (face up more) */
        rockNormalBase = normalize(rockNormalBase + uphill * ledgeTop * 0.3);

        /* broad bands of tone (grey to ochre) and dark crevices under ledges */
        float tone = terrainNoise(vec2(layer * 0.35, 0.5) + position.xz * 0.0015);
        vec3 tint = mix(vec3(0.9), vec3(1.1, 1.0, 0.86), smoothstep(0.3, 0.8, tone));
        rock *= mix(vec3(1.0), tint, steep) * (1.0 - 0.45 * crevice);

        /* tufts of grass on some ledge tops */
        float tufts = smoothstep(0.35, 0.6, terrainNoise(position.xz * 0.05 + vec2(layerId * 3.7)));
        ledgeGrass = ledgeTop * tufts;
    }
    float shoreAmount = 1.0 - smoothstep(4.0, 16.0, h + (macro - 0.5) * 8.0);

    vec3 albedo = mix(grass, shore, shoreAmount);
    float g = dot(grass, vec3(0.3333));
    rock = mix(rock, mix(grass, vec3(g * 1.2, g * 1.05, g * 0.62), 0.45) * 0.9, ledgeGrass);
    albedo = mix(albedo, rock, rockAmount);
    albedo *= mix(0.82, 1.12, macro);

    /* wet only in a band at the waterline: deeper down the mud is under the
     * water, which does the reflecting (a glossy lake bed showed through
     * shallow water as shiny, plastic-looking bumps with sun glints) */
    float wet = smoothstep(-3.0, -0.5, h) * (1.0 - smoothstep(-1.0, 4.0, h));
    specular = mix(0.06, 0.22, rockAmount) + wet * 0.3;
    /* PBR: grass is very rough, rock less so, wet mud at the waterline is smooth */
    roughness = mix(0.92, 0.65, rockAmount);
    roughness = mix(roughness, 0.35, wet);

    normal = n;
    if (wantNormal && terrainHasGroundNormals) {
        /* grass / shore relief, blended with the same weights as the colors */
        vec3 grassBump = mix(terrainGroundBump(terrainGrassNormal, uv),
                             terrainGroundBump(terrainGrassNormal, uv * 1.37 + vec2(0.31, 0.17)),
                             smoothstep(0.35, 0.7, macro));
        vec3 shoreBump = mix(terrainGroundBump(terrainDirtNormal, uv * 2.1),
                             terrainGroundBump(terrainSoilNormal, uv * 1.3), smoothstep(-2.0, 7.0, h));
        /* the lake bed is soft silt: flatter under the water */
        vec3 groundBump = mix(grassBump, shoreBump, shoreAmount) * terrainNormalStrength *
                          mix(0.35, 1.0, smoothstep(-3.0, 0.0, h));
        normal = normalize(n + groundBump);
    }
    if (wantNormal && terrainHasRockNormal && rockAmount > 0.01) {
        /* triplanar normal mapping (UDN blend) on the rock */
        vec3 tx = texture(terrainRockNormal, position.zy * rockScale).xyz * 2.0 - 1.0;
        vec3 ty = texture(terrainRockNormal, position.xz * rockScale).xyz * 2.0 - 1.0;
        vec3 tz = texture(terrainRockNormal, position.xy * rockScale).xyz * 2.0 - 1.0;
        vec3 nx = vec3(0.0, tx.y, tx.x) * 0.8;
        vec3 ny = vec3(ty.x, 0.0, ty.y) * 0.8;
        vec3 nz = vec3(tz.x, tz.y, 0.0) * 0.8;
        vec3 rockNormal = normalize(rockNormalBase + (nx * w.x + ny * w.y + nz * w.z) * terrainNormalStrength);
        normal = normalize(mix(normal, rockNormal, rockAmount));
    } else if (wantNormal && terrainRockDetail > 0.0) {
        normal = normalize(mix(normal, rockNormalBase, rockAmount));
    }
    return albedo;
}


/* a texture projected from the three sides of a surface, blended by how much it faces each */
vec4 triplanar(sampler2D map, vec3 p, vec3 weights) {
    return texture(map, p.zy) * weights.x + texture(map, p.xz) * weights.y + texture(map, p.xy) * weights.z;
}

void main(){
    if (terrainEnabled) {
        vec3 terrainNormal;
        float terrainSpecular, terrainRoughness;
        vec3 albedo = terrainSurface(worldPosition, worldNormal, true, terrainNormal, terrainSpecular, terrainRoughness);
        if (terrainWetness > 0.0) {
            /* damp everywhere, puddles in the low spots of a noise pattern (and
             * only on fairly flat ground): water darkens the ground and makes
             * it smooth; puddles are mirror flat */
            float level = smoothstep(0.85, 0.97, worldNormal.y);
            float pattern = terrainNoise(worldPosition.xz * 0.012) * 0.65 + terrainNoise(worldPosition.xz * 0.05 + 7.0) * 0.35;
            float puddle = smoothstep(0.52, 0.6, pattern) * level;
            float wet = terrainWetness * mix(0.55, 1.0, puddle);
            albedo *= mix(1.0, 0.45, wet);
            terrainRoughness = mix(terrainRoughness, 0.04, mix(wet * 0.7, 1.0, puddle * terrainWetness));
            terrainSpecular = mix(terrainSpecular, 1.0, wet);
            terrainNormal = normalize(mix(terrainNormal, worldNormal, puddle * terrainWetness));   /* still water is flat */
        }
        outColor = vec4(albedo * material.diffuse, 1.0 - material.emissive);
        outEyeSpaceNormal = (normalize(mat3(viewMatrix) * terrainNormal) + vec3(1.0)) * 0.5;
        outRoughness = pbrEnabled ? vec3(terrainRoughness, 0.0, 0.0) : vec3(terrainSpecular);
        return;
    }
    /* tiled by size: the textures projected onto the surface from its three sides, in the mesh's own
     * space at world scale (moves with the entity; its scale never stretches them) */
    bool tiled = material.tileSize > 0.0;
    vec3 objectScale = vec3(length(modelMatrix[0].xyz), length(modelMatrix[1].xyz), length(modelMatrix[2].xyz));
    vec3 tilePosition = objectPosition * objectScale / max(material.tileSize, 1e-4);
    vec3 tileNormal = normalize(objectNormal / objectScale);    /* the normal in that scaled space */
    vec3 tileWeights = pow(abs(tileNormal), vec3(4.0));
    tileWeights /= tileWeights.x + tileWeights.y + tileWeights.z;

    if (hasTexture){
        vec3 color = tiled ? triplanar(textureSampler, tilePosition, tileWeights).rgb
                           : texture (textureSampler, textureCoords).rgb;
        if (material.isBlackAlpha == true && (color.r+color.g+color.b)/3.0f <= 0.1f)
            discard;
        outColor = vec4 (color * material.diffuse, 1.0 - material.emissive);
    } else {
        outColor = vec4 (material.diffuse, 1.0 - material.emissive);
    }

    int frontCond = -(1 - int(gl_FrontFacing)*2);

    if (!hasNormalMap) {
        outEyeSpaceNormal = (normalize(eyeSpaceNormal * frontCond) + vec3(1)) * 0.5;
	} else if (tiled) {
        /* each side's normal map laid onto the surface (whiteout blend), then back out of the scaled space */
        vec3 n = tileNormal * float(frontCond);
        vec3 tx = texture(normalMapSampler, tilePosition.zy).xyz * 2.0 - 1.0;
        vec3 ty = texture(normalMapSampler, tilePosition.xz).xyz * 2.0 - 1.0;
        vec3 tz = texture(normalMapSampler, tilePosition.xy).xyz * 2.0 - 1.0;
        tx.xy *= material.normalMapStrength;
        ty.xy *= material.normalMapStrength;
        tz.xy *= material.normalMapStrength;
        tx = vec3(tx.xy + n.zy, abs(tx.z) * n.x);
        ty = vec3(ty.xy + n.xz, abs(ty.z) * n.y);
        tz = vec3(tz.xy + n.xy, abs(tz.z) * n.z);
        vec3 bumped = normalize(tx.zyx * tileWeights.x + ty.xzy * tileWeights.y + tz.xyz * tileWeights.z);
        vec3 world = normalize(mat3(modelMatrix) * (bumped / objectScale));
        outEyeSpaceNormal = (normalize(mat3(viewMatrix) * world) + vec3(1.0)) * 0.5;
	} else {
        vec3 normalMapNormal = texture (normalMapSampler, textureCoords).xyz;
        normalMapNormal = normalMapNormal * 2.0 - 1.0;
        normalMapNormal.xy *= material.normalMapStrength;

        outEyeSpaceNormal = (normalize(mat3(viewMatrix) * mat3(modelMatrix) * fromTangentToModelSpace * normalMapNormal * frontCond) + vec3(1.0)) * 0.5;
	}

	if (pbrEnabled) {
        float roughness = !hasRoughness ? material.roughness
                        : tiled ? triplanar(roughnessSampler, tilePosition, tileWeights).r
                        : texture(roughnessSampler, textureCoords).r;
        outRoughness = vec3(roughness, material.metallic, 0.0);
	} else if (!hasRoughness) {
        outRoughness = vec3(material.specularStrength);
	} else {
        outRoughness = (tiled ? triplanar(roughnessSampler, tilePosition, tileWeights).xyz
                              : texture(roughnessSampler, textureCoords).xyz) * material.specularStrength;
	}
}
