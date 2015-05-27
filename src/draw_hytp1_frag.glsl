#version 330 core
#extension GL_ARB_shader_image_load_store: require


out vec4 out_col;

layout (r32ui) uniform uimage2DArray abuffer;
uniform sampler2D visibility;


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
    float vis = 1.0;
    for (int i = 0; i < 4; i++) {
        vec2 da = my_unpack(imageLoad(abuffer, ivec3(gl_FragCoord.xy, i)).r);
        vis *= 1.0 - da.x;
        imageStore(abuffer, ivec3(gl_FragCoord.xy, i), uvec4(my_pack(vec2(vis, da.y))));
    }

    out_col = vec4(0.0, 0.0, 0.0, texelFetch(visibility, ivec2(gl_FragCoord.xy), 0).r);
}
