#version 150 core


in vec3 vf_nrm, vf_pos;

out vec4 out_col;

uniform sampler2D fb, depth;
uniform mat3 mat_nrp;


void main(void)
{
    if (texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r <= gl_FragCoord.z) {
        discard;
    }

    out_col = texture(fb, vf_pos.xy - 0.4 * normalize(mat_nrp * vf_nrm).xy);
}
