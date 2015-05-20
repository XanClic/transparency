#version 330 core
#extension GL_ARB_shader_image_load_store: require
#extension GL_ARB_shader_atomic_counters: require
#extension GL_ARB_shading_language_packing: require


in vec3 vf_pos, vf_col;

out vec4 out_col;

layout (r32ui) uniform coherent uimage2D head;
layout (rgba32ui) uniform coherent uimage2D list;
uniform float alpha;
layout (offset = 0, binding = 0) uniform atomic_uint counter;


#define LW 2048
#define LH 2048


void main(void)
{
    uint i = atomicCounterIncrement(counter) + 1;

    if (i < LW * LH) {
        uint prev = imageAtomicExchange(head, ivec2(gl_FragCoord.xy), i);
        imageStore(list, ivec2(i % LW, i / LW),
                   uvec4(packUnorm4x8(vec4(vf_col, 1.0) * alpha),
                         floatBitsToInt(gl_FragCoord.z),
                         0, prev));
    }

    out_col = vec4(0.0);
}
