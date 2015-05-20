#version 150 core


in vec3 vf_col;

out vec4 out_col;

uniform float alpha;


void main(void)
{
    out_col = vec4(vf_col, 1.0) * alpha;
}
