#version 150 core


in vec3 vf_nrm, vf_pos;

out vec4 out_col;

uniform sampler2D fb, depth;
uniform mat3 mat_nrp;


void main(void)
{
    vec2 straight = vf_pos.xy;
    vec2 refractd = straight - 0.05 * normalize(mat_nrp * vf_nrm).xy;

    float sd = texture(depth, straight).r;
    float rd = texture(depth, refractd).r;

    vec4 c = texture(fb, refractd);

    float zb = mix(rd, sd, c.a);
    float zb_a = 2.0 * 0.01 * 1.0 / (1.0 + 0.01 - zb * (1.0 - 0.01));
    float zf_a = 2.0 * 0.01 * 1.0 / (1.0 + 0.01 - vf_pos.z * (1.0 - 0.01));

    out_col = vec4(c.rgb *
                   vec3(1.0 - (zb_a - zf_a) * 10.0,
                        1.0 - (zb_a - zf_a) * 15.0,
                        1.0 - (zb_a - zf_a) * 20.0),
                   1.0);
}
