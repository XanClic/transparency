#version 150 core


in vec2 in_pos;

out vec2 vf_pos;


void main(void)
{
    gl_Position = vec4(in_pos, 0.0, 1.0);
    vf_pos = vec2(in_pos.x + 1.0, 1.0 - in_pos.y) / 2.0;
}
