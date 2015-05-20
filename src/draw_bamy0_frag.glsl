#version 150 core


in vec3 vf_col;

out vec3 out_col, out_count;

uniform float alpha;


void main(void)
{
    out_col = vf_col * alpha;
    out_count = vec3(alpha, 1.0, 0.0);
}
