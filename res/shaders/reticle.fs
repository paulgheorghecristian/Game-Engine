#version 330

/* The aim (Rendering/Reticle): a ring in the middle of the screen, drawn over the finished picture.
 * Its radius and line width in pixels; fill: how much of its inside is filled (a dot at rest). The
 * edges are a pixel soft, so it does not shimmer as it grows. */

layout(location = 0) out vec4 outColor;

uniform vec2 screenSize;
uniform float radius;
uniform float thickness;
uniform float fill;
uniform vec4 color;

void main() {
    float d = length(gl_FragCoord.xy - screenSize * 0.5);
    float ring = 1.0 - smoothstep(thickness * 0.5 - 0.6, thickness * 0.5 + 0.6, abs(d - radius));
    float inside = (1.0 - smoothstep(radius - 0.6, radius + 0.6, d)) * fill;
    float alpha = max(ring, inside) * color.a;
    if (alpha < 0.002) discard;
    outColor = vec4(color.rgb, alpha);
}
