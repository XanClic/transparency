#version 150 core


in vec3 in_pos, in_col, in_nrm;
in vec2 in_txc;

out vec3 vf_nrm, vf_pos, vf_col;
out vec2 vf_txc;

uniform mat4 mat_mvp;


void main(void)
{
    vec4 pos = mat_mvp * vec4(in_pos, 1.0);
    gl_Position = pos;
    vf_pos = (pos.xyz / pos.w) / 2.0 + vec3(0.5);
    vf_txc = in_txc;
    vf_nrm = in_nrm;
    vf_col = in_col;
}
