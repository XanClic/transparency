#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include <SDL2/SDL.h>

#include <dake/gl/gl.hpp>
#include <dake/gl/framebuffer.hpp>
#include <dake/gl/obj.hpp>
#include <dake/gl/shader.hpp>
#include <dake/gl/texture.hpp>
#include <dake/gl/vertex_array.hpp>
#include <dake/gl/vertex_attrib.hpp>
#include <dake/math.hpp>


#define WIDTH  1280
#define HEIGHT 720


using namespace dake::gl;
using namespace dake::math;


struct ObjectSection {
    vertex_array *va;
    vec3 color;
};


static void ss_refract(framebuffer *fbs, const mat4 &mvp, const mat3 &nrp,
                       program &draw_bf_prg, program &draw_ff_prg,
                       const std::vector<ObjectSection> &sections,
                       GLenum draw_mode)
{
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    fbs[1].bind();

    glCullFace(GL_FRONT);

    fbs[0][0].bind();
    draw_bf_prg.use();
    draw_bf_prg.uniform<texture>("fb") = fbs[0][0];
    draw_bf_prg.uniform<mat4>("mat_mvp") = mvp;
    draw_bf_prg.uniform<mat3>("mat_nrp") = nrp;
    for (const ObjectSection &sec: sections) {
        sec.va->draw(draw_mode);
    }

    fbs[0].bind();

    glCullFace(GL_BACK);

    fbs[1][0].bind();
    fbs[1].depth().bind();
    draw_ff_prg.use();
    draw_ff_prg.uniform<texture>("fb") = fbs[1][0];
    draw_ff_prg.uniform<texture>("depth") = fbs[1].depth();
    draw_ff_prg.uniform<mat4>("mat_mvp") = mvp;
    draw_ff_prg.uniform<mat3>("mat_nrp") = nrp;
    for (const ObjectSection &sec: sections) {
        sec.va->draw(draw_mode);
    }

    framebuffer::unbind();
    fbs[0].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}


static void ss_refract_dp(framebuffer *fbs, const mat4 &mvp, const mat3 &nrp,
                          program &draw_bfdp_prg, program &draw_ffdp_prg,
                          const std::vector<ObjectSection> &sections,
                          GLenum draw_mode)
{
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glClearDepth(0.f);

    for (int pass = 0; pass < 3; pass++) {
        fbs[1].bind();
        glClear(GL_DEPTH_BUFFER_BIT);

        glCullFace(GL_FRONT);

        fbs[0][0].bind();
        fbs[0].depth().bind();
        draw_bfdp_prg.use();
        draw_bfdp_prg.uniform<texture>("fb") = fbs[0][0];
        draw_bfdp_prg.uniform<texture>("depth") = fbs[0].depth();
        draw_bfdp_prg.uniform<mat4>("mat_mvp") = mvp;
        draw_bfdp_prg.uniform<mat3>("mat_nrp") = nrp;
        for (const ObjectSection &sec: sections) {
            sec.va->draw(draw_mode);
        }

        fbs[0].bind();
        glClear(GL_DEPTH_BUFFER_BIT);

        glCullFace(GL_BACK);

        fbs[1][0].bind();
        fbs[1].depth().bind();
        draw_ffdp_prg.use();
        draw_ffdp_prg.uniform<texture>("fb") = fbs[1][0];
        draw_ffdp_prg.uniform<texture>("depth") = fbs[1].depth();
        draw_ffdp_prg.uniform<mat4>("mat_mvp") = mvp;
        draw_ffdp_prg.uniform<mat3>("mat_nrp") = nrp;
        for (const ObjectSection &sec: sections) {
            sec.va->draw(draw_mode);
        }
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.f);

    framebuffer::unbind();
    fbs[0].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void blend_alpha_dp(framebuffer *fbs, const mat4 &mvp,
                           program &prg, float alpha,
                           const std::vector<ObjectSection> &sections,
                           GLenum draw_mode)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glClearDepth(0.f);

    int fb = 1;

    for (int pass = 0; pass < 3; pass++) {
        fbs[fb].bind();
        fbs[!fb].blit();

        glClear(GL_DEPTH_BUFFER_BIT);

        fbs[!fb][0].bind();
        fbs[!fb].depth().bind();
        prg.use();
        prg.uniform<texture>("fb") = fbs[!fb][0];
        prg.uniform<texture>("depth") = fbs[!fb].depth();
        prg.uniform<mat4>("mat_mvp") = mvp;
        for (const ObjectSection &sec: sections) {
            vec4 rgba = alpha * sec.color;
            rgba.a() = alpha;
            prg.uniform<vec4>("color") = rgba;
            sec.va->draw(draw_mode);
        }

        fb = !fb;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.f);

    framebuffer::unbind();
    fbs[!fb].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void simple_draw(const mat4 &mvp, program &prg, float alpha,
                        const std::vector<ObjectSection> &sections,
                        GLenum draw_mode)
{
    prg.use();
    prg.uniform<mat4>("mat_mvp") = mvp;
    for (const ObjectSection &sec: sections) {
        vec4 rgba = alpha * sec.color;
        rgba.a() = alpha;
        prg.uniform<vec4>("color") = rgba;
        sec.va->draw(draw_mode);
    }
}


static void abuffer_ll(const mat4 &mvp, program &abuf0_prg, program &abuf1_prg,
                       texture &head, texture &list, float alpha,
                       const std::vector<ObjectSection> &sections,
                       GLenum draw_mode, vertex_array *quad_va)
{
    static GLuint atomic_counter_buffer;

    if (!atomic_counter_buffer) {
        glGenBuffers(1, &atomic_counter_buffer);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_counter_buffer);
        glBufferStorage(GL_ATOMIC_COUNTER_BUFFER, 4, nullptr, 0);
        glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, atomic_counter_buffer, 0, 4);
    }

    //head.clear();
    glClearTexImage(head.glid(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    texture::unbind(head.tmu());
    texture::unbind(list.tmu());

    glBindImageTexture(0, head.glid(), 0, false, 0, GL_WRITE_ONLY, GL_R32UI);
    glBindImageTexture(1, list.glid(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA32UI);

    glClearBufferData(GL_ATOMIC_COUNTER_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    glColorMask(false, false, false, false);

    abuf0_prg.use();
    abuf0_prg.uniform<int32_t>("head") = 0;
    abuf0_prg.uniform<int32_t>("list") = 1;
    abuf0_prg.uniform<mat4>("mat_mvp") = mvp;
    for (const ObjectSection &sec: sections) {
        vec4 rgba = alpha * sec.color;
        rgba.a() = alpha;
        abuf0_prg.uniform<vec4>("color") = rgba;
        sec.va->draw(draw_mode);
    }

    glColorMask(true, true, true, true);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindImageTexture(0, head.glid(), 0, false, 0, GL_READ_ONLY, GL_R32UI);
    glBindImageTexture(1, list.glid(), 0, false, 0, GL_READ_ONLY, GL_RGBA32UI);

    head.bind();
    abuf1_prg.use();
    abuf1_prg.uniform<int32_t>("head") = 0;
    abuf1_prg.uniform<int32_t>("list") = 1;
    quad_va->draw(GL_TRIANGLE_STRIP);

    glBindImageTexture(0, 0, 0, false, 0, 0, GL_R32UI);
    glBindImageTexture(1, 0, 0, false, 0, 0, GL_RGBA32UI);

    glDisable(GL_BLEND);
}


static void blend_meshkin(framebuffer *fbs, const mat4 &mvp, program &prg,
                          float alpha,
                          const std::vector<ObjectSection> &sections,
                          GLenum draw_mode)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    fbs[0][0].bind();

    prg.use();
    prg.uniform<texture>("fb") = fbs[0][0];
    prg.uniform<mat4>("mat_mvp") = mvp;
    for (const ObjectSection &sec: sections) {
        vec4 rgba = alpha * sec.color;
        rgba.a() = alpha;
        prg.uniform<vec4>("color") = rgba;
        sec.va->draw(draw_mode);
    }

    glDisable(GL_BLEND);

    framebuffer::unbind();
    fbs[1].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


int main(int argc, char *argv[])
{
    if (argc < 2) {
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetSwapInterval(1);

    SDL_Window *wnd = SDL_CreateWindow("transp", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(wnd);


    glext.init();


    texture input(argv[1]);

    vertex_array *quad = new vertex_array;
    quad->set_elements(4);

    vec2 quad_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    quad->attrib(0)->format(2);
    quad->attrib(0)->data(quad_vertex_positions);


    texture abuf_ll_head, abuf_list_data;
    abuf_ll_head.format(GL_R32UI, WIDTH, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT);
    abuf_list_data.format(GL_RGBA32UI, 2048, 2048, GL_RGBA_INTEGER, GL_UNSIGNED_INT);

    abuf_list_data.tmu() = 1;


    program draw_tex_prg {shader(shader::VERTEX,   "draw_tex_vert.glsl"),
                          shader(shader::FRAGMENT, "draw_tex_frag.glsl")};
    draw_tex_prg.bind_attrib("in_pos", 0);
    draw_tex_prg.bind_frag("out_col", 0);

    program *draw_abuf1_prg = nullptr;
    if (glext.has_extension("GL_ARB_shader_image_load_store")) {
        draw_abuf1_prg = new program {shader(shader::VERTEX,   "draw_abuf1_vert.glsl"),
                                      shader(shader::FRAGMENT, "draw_abuf1_frag.glsl")};
        draw_abuf1_prg->bind_attrib("in_pos", 0);
        draw_abuf1_prg->bind_frag("out_col", 0);
    }

    shader *simple_vsh = new shader(shader::VERTEX, "draw_xf_vert.glsl");

    program draw_bf_prg     {shader(shader::FRAGMENT, "draw_bf_frag.glsl")};
    program draw_ff_prg     {shader(shader::FRAGMENT, "draw_ff_frag.glsl")};
    program draw_dp_prg     {shader(shader::FRAGMENT, "draw_dp_frag.glsl")};
    program draw_bfdp_prg   {shader(shader::FRAGMENT, "draw_bfdp_frag.glsl")};
    program draw_ffdp_prg   {shader(shader::FRAGMENT, "draw_ffdp_frag.glsl")};
    program draw_simple_prg {shader(shader::FRAGMENT, "draw_simple_frag.glsl")};
    program draw_meshk_prg  {shader(shader::FRAGMENT, "draw_meshk_frag.glsl")};

    program *draw_abuf0_prg = nullptr;
    if (glext.has_extension("GL_ARB_shader_image_load_store")) {
        draw_abuf0_prg = new program {shader(shader::FRAGMENT, "draw_abuf0_frag.glsl")};
    }

    for (program *prg: {&draw_bf_prg, &draw_ff_prg, &draw_dp_prg,
                        &draw_bfdp_prg, &draw_ffdp_prg, &draw_simple_prg,
                        &draw_meshk_prg, draw_abuf0_prg})
    {
        if (!prg) {
            continue;
        }

        *prg << *simple_vsh;
        prg->bind_attrib("in_pos", 0);
        prg->bind_attrib("in_nrm", 1);
        prg->bind_frag("out_col", 0);
    }

    obj entity = load_obj("entity.obj");
    std::vector<ObjectSection> entity_secs;
    for (obj_section &sec: entity.sections) {
        entity_secs.emplace_back();
        entity_secs.back().va = sec.make_vertex_array(0, -1, 1);
        entity_secs.back().color = sec.material.diffuse;
    }

    std::vector<ObjectSection> quad_secs;
    vec3 translated_quad[4];
    for (int x: {-1, 1}) {
        for (int rl = 0; rl < 3; rl++) {
            int l = x < 0 ? rl : 2 - rl;

            for (int v = 0; v < 4; v++) {
                vec2 xy = quad_vertex_positions[v]
                        + vec2(x * 2.f, 0.f)
                        + vec2(l * .2f, -l * .2f);
                translated_quad[v] = vec3(xy.x(), xy.y(), .1f * l);
            }

            quad_secs.emplace_back();
            quad_secs.back().va = new vertex_array;
            quad_secs.back().va->set_elements(4);
            quad_secs.back().va->attrib(0)->format(3);
            quad_secs.back().va->attrib(0)->data(translated_quad);
            quad_secs.back().color = l == 0 ? vec3(1.f, .5f, .5f)
                                   : l == 1 ? vec3(.5f, 1.f, .5f)
                                   :          vec3(.5f, .5f, 1.f);
        }
    }


    framebuffer fbs[2] = {
        framebuffer(1),
        framebuffer(1)
    };

    fbs[0].resize(WIDTH, HEIGHT);
    fbs[1].resize(WIDTH, HEIGHT);

    fbs[0].depth().tmu() = 1;
    fbs[1].depth().tmu() = 1;


    mat4 mv = mat4::identity().translated(vec3(0.f, 0.f, -5.f));
    mat4 p = mat4::projection(M_PIf / 4.f, static_cast<float>(WIDTH) / HEIGHT, .1f, 50.f);

    std::default_random_engine reng;
    std::uniform_real_distribution<float> dist(-.8f, .8f);
    float z_comp = 0.f, z_target = 0.f, z_comp_deriv = 0.f;

    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();

    enum Mode {
        BLEND_ALPHA,
        BLEND_ALPHA_DP,
        ABUFFER_LL,
        BLEND_MESHKIN,
        SS_REFRACT,
        SS_REFRACT_DP,
        BLEND_ADD,
        BLEND_MULT,

        MODE_MAX
    } mode = BLEND_ALPHA;

    enum Objects {
        SUZANNE,
        QUADS,

        OBJECTS_MAX
    } objects = SUZANNE;


    std::vector<ObjectSection> *cur_obj = &entity_secs;
    GLenum cur_draw_mode = GL_TRIANGLES;
    bool need_fbs = false;


    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 0;
            } else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        mode = static_cast<Mode>((static_cast<int>(mode) + 1) % MODE_MAX);
                        need_fbs = mode == BLEND_ALPHA_DP
                                || mode == BLEND_MESHKIN || mode == SS_REFRACT
                                || mode == SS_REFRACT_DP;
                        break;

                    case SDLK_RETURN:
                        objects = static_cast<Objects>((static_cast<int>(objects) + 1) % OBJECTS_MAX);
                        cur_obj = objects == SUZANNE ? &entity_secs : &quad_secs;
                        cur_draw_mode = objects == SUZANNE ? GL_TRIANGLES: GL_TRIANGLE_STRIP;
                        break;
                }
            }
        }

        std::chrono::system_clock::time_point ntp = std::chrono::system_clock::now();
        float diff = std::chrono::duration_cast<std::chrono::microseconds>(ntp - tp).count() / 1000000.f;
        tp = ntp;

        if (objects == SUZANNE) {
            mv.rotate(diff, vec3(0.f, 1.f, z_comp));

            if (fabsf(z_comp - z_target) < .01f) {
                z_target = dist(reng);
            }
            z_comp += z_comp_deriv * diff;
            z_comp_deriv += (z_target - z_comp) * (diff / 10.f);
        } else {
            mv = mat4::identity().translated(vec3(0.f, 0.f, -5.f));
            z_comp = z_target = z_comp_deriv = 0.f;
        }

        mat4 mvp = p * mv;
        mat3 nrp = mat3(mv).transposed_inverse() * mat3(p);

        if (need_fbs) {
            fbs[0].bind();
        } else {
            framebuffer::unbind();
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDepthMask(false);

        input.bind();
        draw_tex_prg.use();
        draw_tex_prg.uniform<texture>("fb") = input;
        quad->draw(GL_TRIANGLE_STRIP);

        glDepthMask(true);

        if (need_fbs) {
            fbs[1].bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            fbs[0].blit();
        }

        switch (mode) {
            case BLEND_ALPHA:
                glEnable(GL_BLEND);
                // Premultiplied source (simple_draw() does that premultiplication)
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                simple_draw(mvp, draw_simple_prg, .5f, *cur_obj, cur_draw_mode);
                glDisable(GL_BLEND);
                break;

            case BLEND_ALPHA_DP:
                blend_alpha_dp(fbs, mvp, draw_dp_prg, .5f, *cur_obj, cur_draw_mode);
                break;

            case ABUFFER_LL:
                if (draw_abuf0_prg && draw_abuf1_prg) {
                    abuffer_ll(mvp, *draw_abuf0_prg, *draw_abuf1_prg, abuf_ll_head,
                               abuf_list_data, .5f, *cur_obj, cur_draw_mode,
                               quad);
                }
                break;

            case BLEND_MESHKIN:
                blend_meshkin(fbs, mvp, draw_meshk_prg, .5f, *cur_obj, cur_draw_mode);
                break;

            case SS_REFRACT:
                ss_refract(fbs, mvp, nrp, draw_bf_prg, draw_ff_prg, *cur_obj, cur_draw_mode);
                break;

            case SS_REFRACT_DP:
                ss_refract_dp(fbs, mvp, nrp, draw_bfdp_prg, draw_ffdp_prg, *cur_obj, cur_draw_mode);
                break;

            case BLEND_ADD:
                glEnable(GL_BLEND);
                // Premultiplied source
                glBlendFunc(GL_ONE, GL_ONE);
                simple_draw(mvp, draw_simple_prg, .2f, *cur_obj, cur_draw_mode);
                glDisable(GL_BLEND);
                break;

            case BLEND_MULT:
                glEnable(GL_BLEND);
                // Premultiplied source
                glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                simple_draw(mvp, draw_simple_prg, .8f, *cur_obj, cur_draw_mode);
                glDisable(GL_BLEND);
                break;

            case MODE_MAX:
                abort();
        }

        SDL_GL_SwapWindow(wnd);
    }


    return 0;
}
