#version 330 core


out vec4 out_col;

uniform sampler2DArray colors;
uniform sampler2D accum;


void main(void)
{
    vec3 color = vec3(0.0);
    float vis = 1.0;

    for (int i = 0; i < 4; i++) {
        vec4 entry = texelFetch(colors, ivec3(gl_FragCoord.xy, i), 0);
        color += vis * entry.rgb;
        vis *= 1.0 - entry.a;
    }

    vec4 tail = texelFetch(accum, ivec2(gl_FragCoord.xy), 0);
    color += vis * tail.rgb / max(0.001, tail.a);

    out_col = vec4(color, 1.0);
}
