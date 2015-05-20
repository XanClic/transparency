#version 150 core


out vec4 out_col;

uniform sampler2D accum, count;


#define EPSILON 0.0001


void main(void)
{
    vec3 acc_tex = texelFetch(accum, ivec2(gl_FragCoord.xy), 0).rgb;
    vec2 cnt_tex = texelFetch(count, ivec2(gl_FragCoord.xy), 0).rg;

    float n = max(1.0, cnt_tex.g);
    vec4 acc_col = vec4(acc_tex, cnt_tex.r);

    out_col = vec4(acc_col.rgb / max(EPSILON, acc_col.a),
                   pow(max(0.0, 1.0 - acc_col.a / n), n));
}
