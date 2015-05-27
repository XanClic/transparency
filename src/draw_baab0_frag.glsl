#version 330 core
#extension GL_ARB_shader_image_load_store: require


out vec4 out_col;

layout (r32ui) uniform coherent uimage2DArray abuffer;
uniform float alpha;


void main(void)
{
    uint cur_val = floatBitsToUint(gl_FragCoord.z);
    for (int i = 0; i < 4; i++) {
        // Yes, this works.
        uint tex_val = imageAtomicMin(abuffer, ivec3(gl_FragCoord.xy, i), cur_val);
        if (tex_val == 0xffffffffu || tex_val == cur_val) {
            break;
        }
        cur_val = max(cur_val, tex_val);
    }

    out_col = vec4(0.0, 0.0, 0.0, alpha);
}
