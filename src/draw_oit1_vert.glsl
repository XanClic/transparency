#version 150 core


in vec2 in_pos;


void main(void)
{
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
