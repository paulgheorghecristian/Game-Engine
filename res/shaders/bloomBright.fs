#version 330

/* Bloom source: the part of the HDR image brighter than the threshold
 * (soft knee so it does not pop in) */

layout(location = 0) out vec4 outColor;

in vec2 textureCoords;

uniform sampler2D hdrSampler;
uniform float threshold;

void main() {
    vec3 color = texture(hdrSampler, textureCoords).rgb;
    float brightness = max(color.r, max(color.g, color.b));
    float knee = threshold * 0.5;
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4);
    outColor = vec4(color * contribution, 1.0);
}
