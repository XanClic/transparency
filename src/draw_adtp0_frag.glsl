#version 330 core
#extension GL_ARB_shader_image_load_store: require


out vec4 out_col;

layout (rgba8_snorm) uniform coherent image2D alpha_tex;
layout (rgba16_snorm) uniform coherent image2D depth_tex;
layout (r32ui) uniform coherent uimage2D lock_tex;
uniform float alpha;


void do_it(void)
{
    vec4 av, dv;
    av = imageLoad(alpha_tex, ivec2(gl_FragCoord.xy));
    dv = imageLoad(depth_tex, ivec2(gl_FragCoord.xy));

    float al[5], dl[5];
    int i = 0, o, ii = -1;
    for (o = 0; o < 5; o++) {
        if (dv[i] > gl_FragCoord.z && ii < 0) {
            ii = o;
            if (o == 0) {
                al[o] = 1.0 - alpha;
            } else {
                al[o] = (1.0 - alpha) * al[o - 1];
            }
            dl[o] = gl_FragCoord.z;
        } else {
            if (ii < 0) {
                al[o] = av[i];
            } else {
                al[o] = av[i] * (1.0 - alpha);
            }
            dl[o] = dv[i];
            i++;
        }
    }

    float smallest_w = 2.0;
    int smallest_w_i = 0;
    for (i = 0; i < 4; i++) {
        float w = (al[i] - al[i + 1]) * (dl[i + 1] - dl[i]);
        if (w < smallest_w) {
            smallest_w = w;
            smallest_w_i = i;
        }
    }

    al[smallest_w_i] = al[smallest_w_i + 1];
    for (i = smallest_w_i + 1; i < 4; i++) {
        al[i] = al[i + 1];
        dl[i] = dl[i + 1];
    }

    for (i = 0; i < 4; i++) {
        av[i] = al[i];
        dv[i] = dl[i];
    }

    imageStore(alpha_tex, ivec2(gl_FragCoord.xy), av);
    imageStore(depth_tex, ivec2(gl_FragCoord.xy), dv);
}


void main(void)
{
    for (;;) {
        if (imageAtomicExchange(lock_tex, ivec2(gl_FragCoord.xy), 1u) == 0u) {
            do_it();
            memoryBarrier();
            imageAtomicExchange(lock_tex, ivec2(gl_FragCoord.xy), 0u);
            break;
        }
    }

    out_col = vec4(0.0, 0.0, 0.0, alpha);
}
