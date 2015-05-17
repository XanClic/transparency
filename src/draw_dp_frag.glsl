#version 150 core


in vec3 vf_pos;

out vec4 out_col;

uniform sampler2D fb, depth;
uniform vec4 color;


#define EPSILON 0.0001


void main(void)
{
    if (texture(depth, vf_pos.xy).r < vf_pos.z + EPSILON) {
        discard;
    }

    out_col = color + (1.0 - color.a) * texture(fb, vf_pos.xy);
}
