#version 150 core


in vec3 vf_col;

out vec4 out_col;

uniform usampler2DArray abuffer;
uniform sampler2D alpha_accum;
uniform float alpha;


// FIXME: Why isn't this 1.0 / 16777215.0?
// (Because the actual depth complexity is that low?)
#define EPSILON (100.0 / 16777215.0)


uint my_pack(vec2 v)
{
    return uint(round(clamp(v.x, 0.0, 1.0) * 255.0))
         | (uint(round(clamp(v.y, 0.0, 1.0) * 16777215.0)) << 8);
}

vec2 my_unpack(uint x)
{
    return vec2(float(x & 0xff) / 255.0,
                float(x >> 8) / 16777215.0);
}


void main(void)
{
    vec2 da[4];
    float trans;
    da[0] = my_unpack(texelFetch(abuffer, ivec3(gl_FragCoord.xy, 0), 0).r);
    da[1] = my_unpack(texelFetch(abuffer, ivec3(gl_FragCoord.xy, 1), 0).r);
    da[2] = my_unpack(texelFetch(abuffer, ivec3(gl_FragCoord.xy, 2), 0).r);
    da[3] = my_unpack(texelFetch(abuffer, ivec3(gl_FragCoord.xy, 3), 0).r);

    float comp_z_l = gl_FragCoord.z - EPSILON;
    float comp_z_g = gl_FragCoord.z + EPSILON;

    if (comp_z_l > da[3].y) {
        trans = da[3].x * alpha
              / max(alpha, texelFetch(alpha_accum, ivec2(gl_FragCoord.xy), 0).r);
    } else if (comp_z_g < da[1].y) {
        trans = da[0].x;
    } else if (comp_z_g < da[2].y) {
        trans = da[1].x;
    } else if (comp_z_g < da[3].y) {
        trans = da[2].x;
    } else {
        trans = da[3].x;
    }

    out_col = vec4(vf_col, trans);
}
