#version 330 core
#extension GL_ARB_shader_image_load_store: require
#extension GL_ARB_shading_language_packing: require


out vec4 out_col;

layout (r32ui) uniform uimage2D head;
layout (rgba32ui) uniform uimage2D list;
uniform int layer;


#define LW 2048
#define LH 2048

#define K 8


void main(void)
{
    uint ei = imageLoad(head, ivec2(gl_FragCoord.xy)).r;

    if (ei == 0) {
        discard;
    }

    int n = 0;
    uvec2 fragments[K];
    while (ei != 0 && n < K) {
        uvec4 element = imageLoad(list, ivec2(ei % LW, ei / LW));
        fragments[n++] = element.xy;
        ei = element.w;
    }

    if (layer >= n) {
        discard;
    }

    for (int i = 1; i < n; i++) {
        uvec2 f = fragments[i];
        int j;
        for (j = i; j > 0 && fragments[j - 1].y < f.y; j--) {
            fragments[j] = fragments[j - 1];
        }
        fragments[j] = f;
    }

    out_col = unpackUnorm4x8(fragments[layer].x);
}
