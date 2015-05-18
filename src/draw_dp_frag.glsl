#version 150 core


out vec4 out_col;

uniform sampler2D fb, depth;
uniform vec4 color;


void main(void)
{
    if (texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r <= gl_FragCoord.z) {
        discard;
    }

    out_col = color + (1.0 - color.a) * texelFetch(fb, ivec2(gl_FragCoord.xy), 0);
}
