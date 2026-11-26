#version 330

/* Waking up (Rendering/Waking): the picture through eyes that fight to stay open. Blurred (through
 * the picture's mipmaps) and doubled until it clears; the lids closing in from top and bottom, curved like an eye's, lit
 * through (dark red, not black) */
in vec2 textureCoords;
layout(location = 0) out vec4 outColor;

uniform sampler2D pictureSampler;
uniform float eyesOpen;     /* 0 shut .. 1 open */
uniform float blur;         /* 0 clear .. 1 blurred and doubled */
uniform float time;

void main() {
    vec2 uv = textureCoords;
    /* blurred: read from a smaller copy of the picture (its mipmaps) the more blurred, blended
     * between sizes, so the blur grows and fades evenly; four reads half a texel apart at that size
     * smooth the smallest copies' blocks. Doubled: a ghost drifting beside it */
    float lod = blur * 6.0;
    vec2 texel = (exp2(lod) - 1.0) * 0.5 / vec2(textureSize(pictureSampler, 0));
    vec3 c = (textureLod(pictureSampler, uv + vec2(-texel.x, -texel.y), lod).rgb +
              textureLod(pictureSampler, uv + vec2(texel.x, -texel.y), lod).rgb +
              textureLod(pictureSampler, uv + vec2(-texel.x, texel.y), lod).rgb +
              textureLod(pictureSampler, uv + vec2(texel.x, texel.y), lod).rgb) * 0.25;
    vec2 ghost = vec2(blur * 0.03 * (0.75 + 0.25 * sin(time * 0.9)), blur * 0.006);
    c = mix(c, textureLod(pictureSampler, uv + ghost, lod).rgb, 0.4 * blur * blur);

    /* the lids: an eye-shaped opening, its half height (in the picture's -1..1) by how open they are,
     * smaller towards the corners; shut, none at all */
    float open = clamp(eyesOpen, 0.0, 1.0);
    float x = uv.x * 2.0 - 1.0, y = uv.y * 2.0 - 1.0;
    float halfHeight = (open * 1.45 - 0.15) * (1.0 - 0.35 * x * x);
    float edge = 0.12 + 0.25 * (1.0 - open);
    float lid = smoothstep(halfHeight, halfHeight + edge, abs(y));
    vec3 lidColor = vec3(0.045, 0.01, 0.006) * (0.3 + 0.7 * open);   /* lit through: dark red */
    c *= mix(0.45, 1.0, open);                                      /* the light dim until open */
    outColor = vec4(mix(c, lidColor, lid), 1.0);
}
