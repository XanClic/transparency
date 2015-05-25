#version 150 core


in vec3 vf_col;

out vec4 out_col;

uniform sampler2D alpha_tex, depth_tex;
uniform float alpha;


// Resolution of depth_tex
#define EPSILON (1.0 / 65536.0)


void main(void)
{
    vec4 av, dv;

    av = texelFetch(alpha_tex, ivec2(gl_FragCoord.xy), 0);
    dv = texelFetch(depth_tex, ivec2(gl_FragCoord.xy), 0);

    float visibility = 1.0;
    for (int i = 0; i < 4 && dv[i] < gl_FragCoord.z - EPSILON; i++) {
        visibility = av[i];
    }

    out_col = vec4(vf_col, visibility * alpha);
}
