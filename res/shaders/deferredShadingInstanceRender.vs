#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 modelMatrix;

uniform mat4 viewMatrix, projectionMatrix;
/* wind (InstanceRenderComponent "wind"): the bend grows with the height
 * above the instance's base (trunks stay planted), slow gusts roll across the
 * landscape, and leaves flutter */
uniform float windStrength;   /* per draw: 0 = rigid */
uniform vec2 windDirection;   /* xz, length = the map's wind strength */
uniform float windTime;
vec3 applyWind(vec3 world, vec3 base) {
    if (windStrength <= 0.0) return world;
    float h = max(world.y - base.y, 0.0);
    float phase = dot(base.xz, vec2(0.0131, 0.0217));
    vec2 dir = windDirection / max(length(windDirection), 1e-4);
    float gust = 0.55 + 0.45 * sin(windTime * 0.45 - dot(base.xz, dir) * 0.006);
    float sway = (0.5 + 0.35 * sin(windTime * 1.1 + phase) + 0.15 * sin(windTime * 2.7 + phase * 1.9)) * gust;
    float flutter = sin(windTime * 7.0 + dot(world, vec3(0.31, 0.23, 0.27))) * 0.02 * h;
    vec2 d = windDirection * (sway * 0.0006 * h * h + flutter) * windStrength;
    return world + vec3(d.x, 0.0, d.y);
}


/* the crown (InstanceRenderComponent "crownShadow"), in the mesh's units: centre, horizontal and
 * vertical radius (0: none) */
uniform vec3 crownCentre;
uniform vec2 crownSize;
/* how much light reaches this part of the crown: the leaves deep inside and underneath are
 * shaded by the rest (the sky and the sun both come from above and outside) */
out float crownLight;
out vec3 eyeSpaceNormal;
out vec2 textureCoords;
out mat3 fromTangentToModelSpace;
out mat4 frag_modelMatrix;

void main(){
	eyeSpaceNormal = (mat3(viewMatrix) * mat3(modelMatrix) * inNormal).xyz;
	textureCoords = inTexcoord;
	crownLight = 1.0;
	if (crownSize.x > 0.0) {
		vec3 rel = (inPosition - crownCentre) / vec3(crownSize.x, crownSize.y, crownSize.x);
		float depth = length(rel);                             /* 0 in the middle, 1 at the surface */
		float inside = mix(0.6, 1.0, smoothstep(0.3, 1.0, depth));
		float under = mix(0.72, 1.0, smoothstep(-0.9, 0.5, rel.y));
		crownLight = mix(1.0, inside * under, 1.0 - smoothstep(1.0, 1.4, depth));
	}
	frag_modelMatrix = modelMatrix;

	vec3 normalizedNormal = inNormal;
	vec3 normalizedTangent = inTangent;
	vec3 normalizedBiTangent = inBiTangent;

	// re-orthogonalize T with respect to N
	vec3 T = normalize(inTangent - dot(inTangent, inNormal) * inNormal);
	// then retrieve perpendicular vector B with the cross product of T and N
	normalizedBiTangent = cross(normalizedNormal, T);

	fromTangentToModelSpace = mat3 (normalizedTangent.x, normalizedTangent.y, normalizedTangent.z,
                                    normalizedBiTangent.x, normalizedBiTangent.y, normalizedBiTangent.z,
                                    normalizedNormal.x, normalizedNormal.y, normalizedNormal.z);

	vec3 world = applyWind((modelMatrix * vec4(inPosition, 1)).xyz, modelMatrix[3].xyz);
	gl_Position = projectionMatrix * viewMatrix * vec4(world, 1.0);
}
