#version 330

/* Mirrored-camera pass for water reflections (forward lit, clipped at the
 * water plane). Handles plain and instanced meshes. */

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 5) in mat4 instanceModelMatrix;

uniform mat4 modelMatrix, viewMatrix, projectionMatrix;
uniform bool instanced;
uniform float clipHeight;
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


out vec3 worldPosition;
out vec3 worldNormal;
out vec2 textureCoords;

void main() {
    mat4 model = instanced ? instanceModelMatrix : modelMatrix;
    vec4 world = model * vec4(inPosition, 1.0);
    if (instanced) {
        world.xyz = applyWind(world.xyz, model[3].xyz);
    }
    worldPosition = world.xyz;
    worldNormal = transpose(inverse(mat3(model))) * inNormal;
    textureCoords = inTexcoord;
    gl_ClipDistance[0] = world.y - clipHeight;
    gl_Position = projectionMatrix * viewMatrix * world;
}
