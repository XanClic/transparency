#version 330 core
#extension GL_ARB_shader_image_load_store: require


out float out_transp, out_vis;

layout (r32ui) uniform coherent uimage2DArray abuffer;
uniform float alpha;


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
    uint da = my_pack(vec2(alpha, gl_FragCoord.z));

    float tail_alpha = 1.0;

    uint cur_val = da;
    for (int i = 0; i < 4; i++) {
        // Yes, this works.
        uint tex_val = imageAtomicMin(abuffer, ivec3(gl_FragCoord.xy, i), cur_val);
        if (tex_val >= 0xffff0000u) {
            tail_alpha = 0.0;
            break;
        }
        cur_val = max(cur_val, tex_val);
    }

    tail_alpha *= my_unpack(cur_val).x;

    out_transp = tail_alpha;
    out_vis = 1.0 - alpha;
}
