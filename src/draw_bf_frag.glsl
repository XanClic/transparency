#version 150 core


in vec3 vf_nrm, vf_pos;

out vec4 out_col;

uniform sampler2D fb;
uniform mat3 mat_nrp;


void main(void)
{
    out_col = texture(fb, vf_pos.xy - 0.05 * normalize(mat_nrp * vf_nrm).xy);
}
