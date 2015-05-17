#version 150 core


in vec3 vf_nrm, vf_pos;

out vec4 out_col;

uniform sampler2D fb, depth;
uniform mat3 mat_nrp;


#define EPSILON 0.0001


void main(void)
{
    if (texture(depth, vf_pos.xy).r < vf_pos.z + EPSILON) {
        discard;
    }

    out_col = texture(fb, vf_pos.xy - 0.05 * normalize(mat_nrp * vf_nrm).xy);
}
