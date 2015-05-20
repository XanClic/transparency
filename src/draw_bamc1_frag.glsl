#version 150 core


out vec4 out_col;

uniform sampler2D accum, transp;


#define EPSILON 0.0001


void main(void)
{
    vec4 acc_tex = texelFetch(accum, ivec2(gl_FragCoord.xy), 0);
    float transp_tex = texelFetch(transp, ivec2(gl_FragCoord.xy), 0).r;

    out_col = vec4(acc_tex.rgb / max(EPSILON, acc_tex.a), transp_tex);
}
