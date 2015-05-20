#version 150 core


in vec3 vf_nrm, vf_pos;

out vec4 out_col;

uniform sampler2D fb, depth;
uniform mat3 mat_nrp;


void main(void)
{
    // This value must be low so relatively few points outside of the backface
    // get hit; but it may not be 0.0 so one sees the frontface doing some
    // refraction.
    vec2 refractd = vf_pos.xy - 0.05 * normalize(mat_nrp * vf_nrm).xy;

    float sd = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).r;
    float rd = texture(depth, refractd).r;

    if (sd <= gl_FragCoord.z) {
        discard;
    }

    vec4 c = texture(fb, refractd);

    float zb = rd < vf_pos.z ? sd : rd;
    float zb_a = zb * 10.0;
    float zf_a = vf_pos.z * 10.0;

    out_col = vec4(c.rgb *
                   vec3(1.0 - (zb_a - zf_a) * 0.5,
                        1.0 - (zb_a - zf_a) * 1.0,
                        1.0 - (zb_a - zf_a) * 1.5),
                   1.0);
}
