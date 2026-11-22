#version 330

layout(location = 0) out vec4 outLight;

uniform sampler2D depthSampler;
uniform sampler2D volumetricLightDepthSampler;
uniform sampler2D blueNoiseSampler;

uniform mat4 volumetricLightProjectionMatrix;
uniform mat4 volumetricLightViewMatrix;
uniform mat4 volumetricLightViewProj; /* projection * view, computed on the CPU */
uniform mat4 inverseViewMatrix;       /* inverse(viewMatrix), computed on the CPU */

uniform vec3 lightColor;
uniform vec3 lightPosition;

uniform int screenWidth, screenHeight;
uniform int volumetricDivisor; /* this pass renders at (screen size / volumetricDivisor) */
uniform vec3 cameraPosition;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

in vec3 viewRay;
flat in vec3 lightPositionEyeSpace;

uniform float virtualSphereDiameter;
uniform float alpha;

uniform int numSamplePoints;
uniform float coef1;
uniform float coef2;
uniform float beamDensity; /* 0: original model; > 0: beam fading exponentially along its length */
uniform float beamFalloff; /* distance over which the beam density drops by e */
uniform float beamSparkle; /* droplets glittering inside the beam (0 = none) */
uniform float time;

float beamHash3(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

/* drifting droplets in world space, lit only where the beam passes */
float droplets(vec3 world) {
    const float cellSize = 6.0;
    vec3 p = world + vec3(0.0, time * 9.0, 0.0);   /* slowly falling */
    vec3 cell = floor(p / cellSize);
    float h = beamHash3(cell);
    if (h < 0.93) return 0.0;
    vec3 center = (cell + 0.25 + 0.5 * vec3(beamHash3(cell + 3.1), beamHash3(cell + 7.7), beamHash3(cell + 1.3))) * cellSize;
    return smoothstep(1.6, 0.0, length(p - center)) * (0.5 + 0.5 * (h - 0.93) / 0.07);
}
uniform float secondFraction;

in vec3 positionModelSpace;

float beamHash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float beamNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(beamHash(i), beamHash(i + vec2(1, 0)), u.x),
               mix(beamHash(i + vec2(0, 1)), beamHash(i + vec2(1, 1)), u.x), u.y);
}

bool coneIntersection(vec3 rayDir, inout float t1, inout float t2)
{
    vec3 lightDir = normalize(mat3(modelMatrix) * vec3(0,0,-1));
    vec3 negateLightPositionEyeSpace = cameraPosition - lightPosition;
    float cos_alpha2 = pow(cos(alpha), 2); // 0.91266780745 = cos(0.3) ^ 2
    float light_ray_dot = dot(lightDir, rayDir);
    float light_pos_dir_dot = dot(negateLightPositionEyeSpace, lightDir);
    float ray_dir_light_pos_dot = dot(negateLightPositionEyeSpace, rayDir);

    float a = light_ray_dot * light_ray_dot - cos_alpha2;
    float b = 2.0 * (light_pos_dir_dot * light_ray_dot - ray_dir_light_pos_dot * cos_alpha2);
    float c = light_pos_dir_dot * light_pos_dir_dot - dot(negateLightPositionEyeSpace, negateLightPositionEyeSpace) * cos_alpha2;

    float delta = b*b - 4.0*a*c;

    if (delta < 0) {
        return false;
    } else if (delta == 0) {
        t1 = -b / (2 * a);
        t2 = t1;
    } else {
        t1 = (-b + sqrt(delta)) / (2*a);
        t2 = (-b - sqrt(delta)) / (2*a);
    }

    return true;
}

bool sphereIntersection(vec3 rayDir, inout float t1, inout float t2)
{
    vec3 negateLightPositionEyeSpace = cameraPosition - lightPosition;
    float a = 1;
    float b = 2.0 * dot(rayDir, negateLightPositionEyeSpace);
    float c = dot(negateLightPositionEyeSpace,negateLightPositionEyeSpace) - virtualSphereDiameter * virtualSphereDiameter;

    float delta = b*b - 4.0*a*c;

    if (delta < 0) {
        return false;
    } else if (delta == 0) {
        t1 = -b / (2 * a);
        t2 = t1;
    } else {
        t1 = (-b + sqrt(delta)) / (2*a);
        t2 = (-b - sqrt(delta)) / (2*a);
    }

    return true;
}

bool spotLightIntersection(vec3 rayDir, inout float tmin, inout float tmax)
{
    float tc1, tc2, ts1, ts2;
    bool coneIntersect = coneIntersection(rayDir, tc1, tc2);
    if (!coneIntersect)
        return false;
    bool sphereIntersect = false;

    vec3 lightDir = normalize(mat3(modelMatrix) * vec3(0,0,-1));

    sphereIntersect = sphereIntersection(rayDir, ts1, ts2);
    if (!sphereIntersect)
        return false;

	vec3 c1 = cameraPosition + rayDir * tc1;
	vec3 c2 = cameraPosition + rayDir * tc2;
	vec3 s1 = cameraPosition + rayDir * ts1;
	vec3 s2 = cameraPosition + rayDir * ts2;

	vec3 coneToC1 = c1 - lightPosition;
	vec3 coneToC2 = c2 - lightPosition;
	vec3 coneToS1 = s1 - lightPosition;
	vec3 coneToS2 = s2 - lightPosition;

	float hits[4]; // Needs 4 for literal edge cases.
	int index = 0;
	float coneCosAngle = cos(alpha);

	hits[0] = 0;
	hits[1] = 0;
	hits[2] = 0;
	hits[3] = 0;

	if (dot(coneToC1, lightDir) > 0 && length(coneToC1) < virtualSphereDiameter) {
		hits[index++] = tc1;
	}
	if (dot(coneToC2, lightDir) > 0 && length(coneToC2) < virtualSphereDiameter) {
		hits[index++] = tc2;
	}
	if (dot(normalize(coneToS1), lightDir) > coneCosAngle) {
		hits[index++] = ts1;
	}
	if (dot(normalize(coneToS2), lightDir) > coneCosAngle) {
		hits[index++] = ts2;
	}

	float closest = min(hits[0], hits[1]);
	float farthest = max(hits[0], hits[1]);

	tmin = max(0, closest);
	tmax = max(0, farthest);

	return index == 2 && farthest >= 0;
}

void main() {
    vec2 texCoord = gl_FragCoord.xy / (vec2(screenWidth, screenHeight) / float(volumetricDivisor));
    vec3 view = vec3(viewRay.xy/viewRay.z, 1.0);
    float depth = texture(depthSampler, texCoord).x;

    float eyeZNorm = -projectionMatrix[3][2]/((2.0 * depth - 1.0) + projectionMatrix[2][2]);

    float tmin, tmax;
    float pointsNotInShadow = 0;
    float beamDistanceSum = 0.0;
    float sparkle = 0.0;


    vec3 ray = normalize(positionModelSpace - cameraPosition);

    bool spotIntersect = spotLightIntersection(ray, tmin, tmax);

    if (!spotIntersect) {
        discard;
    }

    vec4 worldPosMin = vec4(cameraPosition + ray*tmin, 1.0);
    vec4 worldPosMinClip = (projectionMatrix * viewMatrix * worldPosMin);
    vec3 worldPosMinNDC = worldPosMinClip.xyz / worldPosMinClip.w;
    vec3 worldPosMinNDCNormalized = worldPosMinNDC.xyz;;

    float eyeZMin = worldPosMinNDCNormalized.z;
    float linearMinDepth = -projectionMatrix[3][2]/(eyeZMin + projectionMatrix[2][2]);

    vec4 worldPosMax = vec4(cameraPosition + ray*tmax, 1.0);
    vec4 worldPosMaxClip = (projectionMatrix * viewMatrix * worldPosMax);
    vec3 worldPosMaxNDC = worldPosMaxClip.xyz / worldPosMaxClip.w;
    vec3 worldPosMaxNDCNormalized = worldPosMaxNDC.xyz;

    float eyeZMax = worldPosMaxNDCNormalized.z;
    float linearMaxDepth = -projectionMatrix[3][2]/(eyeZMax + projectionMatrix[2][2]);

    if (linearMinDepth < eyeZNorm) {
        discard;
    }

    if (linearMaxDepth < eyeZNorm) {
        vec4 modelPos = inverseViewMatrix * vec4(view * eyeZNorm, 1.0);
        tmax = length(modelPos.xyz - cameraPosition);
    }

    float rayLength = abs(tmax - tmin);
    float sampleLength = rayLength / numSamplePoints;

    float begin = (rayLength == 0 ? numSamplePoints : 0);

    /* loop invariants */
    float offset = texture(blueNoiseSampler, texCoord + vec2(secondFraction*0.005f)).r;
    vec3 spotDir = normalize(mat3(modelMatrix) * vec3(0,0,-1));
    float dist1 = virtualSphereDiameter*coef1;
    float dist2 = virtualSphereDiameter*coef2;
    float alpha1 = cos(alpha);
    float alpha2 = cos(0.1);

    for (float t = begin; t <= numSamplePoints; t++) {

        float currentT = tmin + sampleLength * (t - offset*2.5f);
        vec3 viewPositionLocal = view * currentT;

        vec4 worldPosition = vec4(cameraPosition + ray*currentT, 1.0);//inverseViewMatrix * vec4(viewPositionLocal, 1.0));
        vec4 volumetricLightClip = volumetricLightViewProj * worldPosition;
        vec3 volumetricLightNDC = volumetricLightClip.xyz / volumetricLightClip.w;
        vec3 volumetricLightNDCNormalized = volumetricLightNDC.xyz * 0.5f + 0.5f;

        float eyeZObjectDepth = volumetricLightNDCNormalized.z;
        float linearObjectDepth = -volumetricLightProjectionMatrix[3][2]/(eyeZObjectDepth + volumetricLightProjectionMatrix[2][2]);

        float currentDepth = texture (volumetricLightDepthSampler, volumetricLightNDCNormalized.xy).x;
        float linearEyeZODepthMap = -volumetricLightProjectionMatrix[3][2]/(currentDepth + volumetricLightProjectionMatrix[2][2]);

        float volumetricLightAttCoef = dot(normalize(worldPosition.xyz - lightPosition), spotDir);

        vec3 dir = worldPosition.xyz - lightPosition;
        float len = length(dir);

        float sm1 = smoothstep(1.0, 0, (len-dist2)/(dist1-dist2));
        float sm2 = smoothstep(1.0, 0, (volumetricLightAttCoef-alpha2)/(alpha1-alpha2));

        if (linearEyeZODepthMap < linearObjectDepth) {
            if (beamDensity > 0.0) {
                /* even fill across the beam with soft edges; the fade along the
                 * length is applied after the loop */
                float across = clamp(acos(clamp(volumetricLightAttCoef, -1.0, 1.0)) / alpha, 0.0, 1.0);
                float radial = 1.0 - smoothstep(0.7, 1.0, across);
                /* faint streaks running along the beam (dust / a lamp grille):
                 * noise over the beam's cross-section, rotating with the light */
                vec3 local = (volumetricLightViewMatrix * worldPosition).xyz;
                vec2 crossSection = local.xy / max(-local.z, 1.0) / max(tan(alpha), 1e-3);
                float streaks = 0.7 + 0.3 * beamNoise(crossSection * 7.0);
                pointsNotInShadow += radial * streaks;
                beamDistanceSum += radial * streaks * len;
                if (beamSparkle > 0.0) {
                    sparkle += droplets(worldPosition.xyz) * radial * exp(-len / beamFalloff);
                }
            } else {
                pointsNotInShadow += sm1 * sm2;
            }
        }
    }

    outLight.xyz = lightColor;
    //float gamma = 1.0f / 2.2f;
    //outLight.rgb = pow(outLight.rgb, vec3(gamma));

    if (beamDensity > 0.0) {
        /* full, even fill across the beam's width (Beer-Lambert) ... */
        float opticalDepth = pointsNotInShadow * sampleLength * beamDensity;
        /* ... with an exponential cut-off along its length, from how far from
         * the lamp the lit part of this view ray is */
        float distanceAlong = beamDistanceSum / max(pointsNotInShadow, 1e-4);
        float lengthFade = exp(-distanceAlong / beamFalloff);
        /* a dimmed light (e.g. a nightOnly lamp by day) also scatters less */
        float brightness = clamp(max(lightColor.r, max(lightColor.g, lightColor.b)), 0.0, 1.0);
        float beam = min(1.0 - exp(-opticalDepth), 0.8) * lengthFade * brightness;
        /* translucent: the beam lights the haze but the background stays visible */
        outLight.rgb = lightColor * (beam + sparkle * beamSparkle * brightness);
        outLight.a = beam * 0.55;
        return;
    } else {
        outLight.a = min(pointsNotInShadow / numSamplePoints, 0.8);
    }
    outLight.xyz *= outLight.a;
}
