#version 330

layout(location = 0) out vec4 outLight;

uniform sampler2D depthSampler;
uniform sampler2D volumetricLightDepthSampler;
uniform sampler2D blueNoiseSampler;

uniform mat4 volumetricLightProjectionMatrix;
uniform mat4 volumetricLightViewMatrix;

uniform vec3 lightColor;
uniform vec3 lightPosition;

uniform int screenWidth, screenHeight;
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

in vec3 positionModelSpace;

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
    vec2 texCoord = gl_FragCoord.xy / (vec2(screenWidth, screenHeight)/4);
    vec3 view = vec3(viewRay.xy/viewRay.z, 1.0);
    float depth = texture(depthSampler, texCoord).x;

    float eyeZNorm = -projectionMatrix[3][2]/((2.0 * depth - 1.0) + projectionMatrix[2][2]);

    float tmin, tmax;
    float pointsNotInShadow = 0;

    mat4 inverseViewMatrix = inverse(viewMatrix);

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

    for (float t = begin; t <= numSamplePoints; t++) {
        float currentT = tmin + sampleLength * t;
        vec3 viewPositionLocal = view * currentT;

        vec4 worldPosition = vec4(cameraPosition + ray*currentT, 1.0);//inverseViewMatrix * vec4(viewPositionLocal, 1.0));
        vec4 volumetricLightClip = (volumetricLightProjectionMatrix * volumetricLightViewMatrix * worldPosition);
        vec3 volumetricLightNDC = volumetricLightClip.xyz / volumetricLightClip.w;
        vec3 volumetricLightNDCNormalized = volumetricLightNDC.xyz * 0.5f + 0.5f;

        float eyeZObjectDepth = volumetricLightNDCNormalized.z;
        float linearObjectDepth = -volumetricLightProjectionMatrix[3][2]/(eyeZObjectDepth + volumetricLightProjectionMatrix[2][2]);

        float currentDepth = texture (volumetricLightDepthSampler, volumetricLightNDCNormalized.xy).x;
        float linearEyeZODepthMap = -volumetricLightProjectionMatrix[3][2]/(currentDepth + volumetricLightProjectionMatrix[2][2]);

        float volumetricLightAttCoef = dot(normalize(worldPosition.xyz - lightPosition),
                                            normalize(mat3(modelMatrix) * vec3(0,0,-1)));

        vec3 dir = worldPosition.xyz - lightPosition;
        float len = length(dir);

        float dist1 = virtualSphereDiameter*coef1;
        float dist2 = virtualSphereDiameter*coef2;
        float alpha1 = cos(alpha);
        float alpha2 = cos(0.1);

        float sm1 = smoothstep(1.0, 0, (len-dist2)/(dist1-dist2));
        float sm2 = smoothstep(1.0, 0, (volumetricLightAttCoef-alpha2)/(alpha1-alpha2));

        if (linearEyeZODepthMap < linearObjectDepth) {
            pointsNotInShadow += sm1 * sm2;
        }
    }

    outLight.xyz = lightColor;
    outLight.a = min(pointsNotInShadow / numSamplePoints, 0.8);
    outLight.xyz *= outLight.a;
}
