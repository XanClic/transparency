#version 150 core


in vec3 vf_col;

out vec4 out_col;

uniform sampler2D fb;
uniform float alpha;


void main(void)
{
    vec3 old_col = texelFetch(fb, ivec2(gl_FragCoord.xy), 0).rgb;
    out_col = vec4(alpha * (vf_col - old_col), 1.0);
}
