#version 330 core
#extension GL_ARB_shader_image_load_store: require


in vec3 vf_col;

out vec4 out_col;

layout (rgba8_snorm) uniform writeonly image2DArray colors;
uniform usampler2DArray abuffer;
uniform float alpha;


#define EPSILON 0.0001


void main(void)
{
    float depths[4];

    for (int i = 0; i < 4; i++) {
        uint rd = texelFetch(abuffer, ivec3(gl_FragCoord.xy, i), 0).r;
        depths[i] = rd == 0xffffffffu ? 1.0 : uintBitsToFloat(rd);
    }

    // FIXME: If two fragments have the same depth values, this discards one of them
    float da[4];
    da[0] = 0.5 * (depths[0] + depths[1]);
    da[1] = 0.5 * (depths[1] + depths[2]);
    da[2] = 0.5 * (depths[2] + depths[3]);
    da[3] = depths[3] + 20.0;

    if (gl_FragCoord.z <= da[0]) {
        imageStore(colors, ivec3(gl_FragCoord.xy, 0), vec4(vf_col, 1.0) * alpha);
    } else if (gl_FragCoord.z <= da[1]) {
        imageStore(colors, ivec3(gl_FragCoord.xy, 1), vec4(vf_col, 1.0) * alpha);
    } else if (gl_FragCoord.z <= da[2]) {
        imageStore(colors, ivec3(gl_FragCoord.xy, 2), vec4(vf_col, 1.0) * alpha);
    } else if (gl_FragCoord.z <= da[3]) {
        imageStore(colors, ivec3(gl_FragCoord.xy, 3), vec4(vf_col, 1.0) * alpha);
    } else {
        out_col = vec4(vf_col, 1.0) * alpha;
        return;
    }

    out_col = vec4(0.0);
    return;
}
