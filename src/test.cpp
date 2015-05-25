#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <getopt.h>
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


static int WIDTH = 1280, HEIGHT = 720;


using namespace dake::gl;
using namespace dake::math;


struct ObjectSection {
    vertex_array *va;
    mat4 rel_mv;
};


static void ss_refract(framebuffer *fbs, const mat4 &mv, const mat4 &proj,
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

    for (const ObjectSection &sec: sections) {
        draw_bf_prg.uniform<mat3>("mat_nrp") = mat3(proj) * (mat3(sec.rel_mv) * mat3(mv)).transposed_inverse();
        draw_bf_prg.uniform<mat4>("mat_mvp") = proj * sec.rel_mv * mv;
        sec.va->draw(draw_mode);
    }

    fbs[0].bind();

    glCullFace(GL_BACK);

    fbs[1][0].bind();
    fbs[1].depth().bind();
    draw_ff_prg.use();
    draw_ff_prg.uniform<texture>("fb") = fbs[1][0];
    draw_ff_prg.uniform<texture>("depth") = fbs[1].depth();
    for (const ObjectSection &sec: sections) {
        draw_ff_prg.uniform<mat3>("mat_nrp") = mat3(proj) * (mat3(sec.rel_mv) * mat3(mv)).transposed_inverse();
        draw_ff_prg.uniform<mat4>("mat_mvp") = proj * sec.rel_mv * mv;
        sec.va->draw(draw_mode);
    }

    framebuffer::unbind();
    fbs[0].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}


static void ss_refract_dp(framebuffer *fbs, const mat4 &mv, const mat4 &proj,
                          program &draw_bfdp_prg, program &draw_ffdp_prg,
                          int layer,
                          const std::vector<ObjectSection> &sections,
                          GLenum draw_mode)
{
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glClearDepth(0.f);

    for (int pass = 0; pass < 4; pass++) {
        fbs[1].bind();
        if (layer == -1) {
            // If no fragments are drawn to a certain pixel, it will have to
            // remain unchanged from the previous pass, and not from the
            // pre-previous
            fbs[0].blit();
        } else {
            bool hit = layer == pass;
            glColorMask(hit, hit, hit, hit);
        }
        glClear(GL_DEPTH_BUFFER_BIT);

        glCullFace(GL_FRONT);

        fbs[0][0].bind();
        fbs[0].depth().bind();
        draw_bfdp_prg.use();
        draw_bfdp_prg.uniform<texture>("fb") = fbs[0][0];
        draw_bfdp_prg.uniform<texture>("depth") = fbs[0].depth();
        for (const ObjectSection &sec: sections) {
            draw_bfdp_prg.uniform<mat3>("mat_nrp") = mat3(proj) * (mat3(sec.rel_mv) * mat3(mv)).transposed_inverse();
            draw_bfdp_prg.uniform<mat4>("mat_mvp") = proj * sec.rel_mv * mv;
            sec.va->draw(draw_mode);
        }

        fbs[0].bind();
        if (layer == -1) {
            fbs[1].blit();
        }
        glClear(GL_DEPTH_BUFFER_BIT);

        glCullFace(GL_BACK);

        fbs[1][0].bind();
        fbs[1].depth().bind();
        draw_ffdp_prg.use();
        draw_ffdp_prg.uniform<texture>("fb") = fbs[1][0];
        draw_ffdp_prg.uniform<texture>("depth") = fbs[1].depth();
        for (const ObjectSection &sec: sections) {
            draw_ffdp_prg.uniform<mat3>("mat_nrp") = mat3(proj) * (mat3(sec.rel_mv) * mat3(mv)).transposed_inverse();
            draw_ffdp_prg.uniform<mat4>("mat_mvp") = proj * sec.rel_mv * mv;
            sec.va->draw(draw_mode);
        }

        if (pass == layer) {
            break;
        }
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.f);

    framebuffer::unbind();
    fbs[0].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void draw_with_alpha(program &prg, const mat4 &mv, const mat4 &proj,
                            float alpha,
                            const std::vector<ObjectSection> &sections,
                            GLenum draw_mode)
{
    for (const ObjectSection &sec: sections) {
        prg.uniform<float>("alpha") = alpha;
        prg.uniform<mat4>("mat_mvp") = proj * sec.rel_mv * mv;
        sec.va->draw(draw_mode);
    }
}


static void blend_alpha_dp(framebuffer *fbs, const mat4 &mv, const mat4 &proj,
                           program &prg, int layer, float alpha,
                           const std::vector<ObjectSection> &sections,
                           GLenum draw_mode)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glClearDepth(0.f);

    int fb = 1;

    for (int pass = 0; pass < 8; pass++) {
        fbs[fb].bind();

        if (layer == -1) {
            // If no fragments are drawn to a certain pixel, it will have to
            // remain unchanged from the previous pass, and not from the
            // pre-previous
            fbs[!fb].blit();
        } else {
            bool hit = layer == pass;
            glColorMask(hit, hit, hit, hit);
        }

        glClear(GL_DEPTH_BUFFER_BIT);

        fbs[!fb][0].bind();
        fbs[!fb].depth().bind();
        prg.use();
        prg.uniform<texture>("fb") = fbs[!fb][0];
        prg.uniform<texture>("depth") = fbs[!fb].depth();
        draw_with_alpha(prg, mv, proj, alpha, sections, draw_mode);

        fb = !fb;

        if (pass == layer) {
            break;
        }
    }

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepth(1.f);

    framebuffer::unbind();
    fbs[!fb].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void simple_draw(const mat4 &mv, const mat4 &proj, program &prg,
                        float alpha, const std::vector<ObjectSection> &sections,
                        GLenum draw_mode)
{
    prg.use();
    draw_with_alpha(prg, mv, proj, alpha, sections, draw_mode);
}


static void abuffer_ll(const mat4 &mv, const mat4 &proj, program &abuf0_prg,
                       program &abuf1_prg, texture &head, texture &list,
                       int layer, float alpha,
                       const std::vector<ObjectSection> &sections,
                       GLenum draw_mode, vertex_array &quad_va)
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
    draw_with_alpha(abuf0_prg, mv, proj, alpha, sections, draw_mode);

    glColorMask(true, true, true, true);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindImageTexture(0, head.glid(), 0, false, 0, GL_READ_ONLY, GL_R32UI);
    glBindImageTexture(1, list.glid(), 0, false, 0, GL_READ_ONLY, GL_RGBA32UI);

    head.bind();
    abuf1_prg.use();
    abuf1_prg.uniform<int32_t>("head") = 0;
    abuf1_prg.uniform<int32_t>("list") = 1;
    if (layer >= 0) {
        abuf1_prg.uniform<int32_t>("layer") = layer;
    }
    quad_va.draw(GL_TRIANGLE_STRIP);

    glBindImageTexture(0, 0, 0, false, 0, 0, GL_R32UI);
    glBindImageTexture(1, 0, 0, false, 0, 0, GL_RGBA32UI);

    glDisable(GL_BLEND);
}


static void blend_meshkin(framebuffer *fbs, const mat4 &mv, const mat4 &proj,
                          program &prg, float alpha,
                          const std::vector<ObjectSection> &sections,
                          GLenum draw_mode)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    fbs[0][0].bind();

    prg.use();
    prg.uniform<texture>("fb") = fbs[0][0];
    draw_with_alpha(prg, mv, proj, alpha, sections, draw_mode);

    glDisable(GL_BLEND);

    framebuffer::unbind();
    fbs[1].blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void blend_bamy(framebuffer &fb_in, framebuffer &fb_bamy,
                       const mat4 &mv, const mat4 &proj, program &prg_draw,
                       program &prg_resolve, float alpha,
                       const std::vector<ObjectSection> &sections,
                       GLenum draw_mode, vertex_array &quad_va)
{
    fb_bamy.bind();
    /* We're not using the depth test anyway, so we don't need this
     * fb_in.blit(0, 0, -1, -1, 0, 0, -1, -1, GL_DEPTH_BUFFER_BIT);
     */

    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    prg_draw.use();
    draw_with_alpha(prg_draw, mv, proj, alpha, sections, draw_mode);

    glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);

    fb_in.bind();
    fb_bamy[0].bind();
    fb_bamy[1].bind();

    prg_resolve.use();
    prg_resolve.uniform<texture>("accum") = fb_bamy[0];
    prg_resolve.uniform<texture>("count") = fb_bamy[1];
    quad_va.draw(GL_TRIANGLE_STRIP);

    glDisable(GL_BLEND);

    framebuffer::unbind();
    fb_in.blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void blend_bamc(framebuffer &fb_in, framebuffer &fb_bamc,
                       const mat4 &mv, const mat4 &proj, program &prg_draw,
                       program &prg_resolve, float alpha,
                       const std::vector<ObjectSection> &sections,
                       GLenum draw_mode, vertex_array &quad_va)
{
    /* We're not using the depth test anyway, so we don't need this
     * fb_bamc.bind();
     * fb_in.blit(0, 0, -1, -1, 0, 0, -1, -1, GL_DEPTH_BUFFER_BIT);
     */

    fb_bamc.mask(1);
    fb_bamc.bind();
    glClear(GL_COLOR_BUFFER_BIT);

    fb_bamc.unmask(1);
    fb_bamc.mask(0);
    fb_bamc.bind();
    glClearColor(1.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.f, 0.f, 0.f, 0.f);

    fb_bamc.unmask(0);
    fb_bamc.bind();

    glEnable(GL_BLEND);
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_SRC_COLOR);

    prg_draw.use();
    draw_with_alpha(prg_draw, mv, proj, alpha, sections, draw_mode);

    glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);

    fb_in.bind();
    fb_bamc[0].bind();
    fb_bamc[1].bind();

    prg_resolve.use();
    prg_resolve.uniform<texture>("accum") = fb_bamc[0];
    prg_resolve.uniform<texture>("transp") = fb_bamc[1];
    quad_va.draw(GL_TRIANGLE_STRIP);

    glDisable(GL_BLEND);

    framebuffer::unbind();
    fb_in.blit(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT);
}


static void adaptive_transp(texture &tex_a, texture &tex_d, texture &tex_l,
                            const mat4 &mv, const mat4 &proj,
                            program &col_vis_prg, program &draw_prg,
                            float alpha,
                            const std::vector<ObjectSection> &sections,
                            GLenum draw_mode)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

    glClearTexImage(tex_a.glid(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    vec4 dc(1.f, 1.f, 1.f, 1.f);
    glClearTexImage(tex_d.glid(), 0, GL_RGBA, GL_FLOAT, &dc);
    glClearTexImage(tex_l.glid(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    texture::unbind(tex_a.tmu());
    texture::unbind(tex_d.tmu());

    glBindImageTexture(0, tex_a.glid(), 0, false, 0, GL_READ_WRITE, GL_RGBA8_SNORM);
    glBindImageTexture(1, tex_d.glid(), 0, false, 0, GL_READ_WRITE, GL_RGBA16_SNORM);
    glBindImageTexture(2, tex_l.glid(), 0, false, 0, GL_READ_WRITE, GL_R32UI);

    col_vis_prg.use();
    col_vis_prg.uniform<int32_t>("alpha_tex") = 0;
    col_vis_prg.uniform<int32_t>("depth_tex") = 1;
    col_vis_prg.uniform<int32_t>("lock_tex")  = 2;
    draw_with_alpha(col_vis_prg, mv, proj, alpha, sections, draw_mode);

    glBindImageTexture(0, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8_SNORM);
    glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA16_SNORM);
    glBindImageTexture(2, 0, 0, false, 0, GL_READ_WRITE, GL_R32UI);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    tex_a.bind();
    tex_d.bind();
    draw_prg.use();
    draw_prg.uniform<texture>("alpha_tex") = tex_a;
    draw_prg.uniform<texture>("depth_tex") = tex_d;
    draw_with_alpha(draw_prg, mv, proj, alpha, sections, draw_mode);

    glDisable(GL_BLEND);
}


int main(int argc, char *argv[])
{
    const char *bg_tex_name, *entity_name = "entity.obj";
    bool entity_gradient = true, borderless = false, two_objects = true;

    static const struct option options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"entity", required_argument, nullptr, 'e'},
        {"material", no_argument, nullptr, 'm'},
        {"borderless", no_argument, nullptr, 'b'},
        {"single", no_argument, nullptr, 's'},

        {nullptr, 0, nullptr, 0}
    };

    for (;;) {
        int option = getopt_long(argc, argv, "he:mbs", options, nullptr);
        if (option == -1) {
            break;
        }

        switch (option) {
            case 'h':
            case '?':
                fprintf(stderr, "Usage: %s [options...] <bg-texture.(png|jpg)>\n\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -h, --help                   Shows this information\n");
                fprintf(stderr, "  -e, --entity=<entity.obj>    Chooses a mesh to be displayed\n");
                fprintf(stderr, "  -m, --material               Uses the material as specified by the entity,\n");
                fprintf(stderr, "                               instead of generating a gradient\n");
                fprintf(stderr, "  -b, --borderless             Run borderless in 1920x1080\n");
                fprintf(stderr, "  -s, --single                 Draw only a single object\n");
                return 0;

            case 'e':
                entity_name = optarg;
                break;

            case 'm':
                entity_gradient = false;
                break;

            case 'b':
                WIDTH = 1920;
                HEIGHT = 1080;
                borderless = true;
                break;

            case 's':
                two_objects = false;
                break;
        }
    }

    if (optind != argc - 1) {
        fprintf(stderr, "Expecting exactly one non-option argument\n");
        return 1;
    }

    bg_tex_name = argv[optind];

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetSwapInterval(1);

    SDL_Window *wnd = SDL_CreateWindow("transp", SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                                       SDL_WINDOW_OPENGL
                                       | (borderless * SDL_WINDOW_BORDERLESS));
    SDL_GL_CreateContext(wnd);


    glext.init();


    texture input(bg_tex_name);

    vertex_array quad;
    quad.set_elements(4);

    vec2 quad_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    quad.attrib(0)->format(2);
    quad.attrib(0)->data(quad_vertex_positions);


    texture abuf_ll_head, abuf_list_data;
    abuf_ll_head.format(GL_R32UI, WIDTH, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT);
    abuf_list_data.format(GL_RGBA32UI, 2048, 2048, GL_RGBA_INTEGER, GL_UNSIGNED_INT);

    abuf_list_data.tmu() = 1;


    shader *pass_vsh = new shader(shader::VERTEX, "draw_tex_vert.glsl");

    program draw_tex_prg   {shader(shader::FRAGMENT, "draw_tex_frag.glsl")};
    program draw_bamy1_prg {shader(shader::FRAGMENT, "draw_bamy1_frag.glsl")};
    program draw_bamc1_prg {shader(shader::FRAGMENT, "draw_bamc1_frag.glsl")};

    program *draw_abuf1_prg = nullptr, *draw_abuf1l_prg = nullptr;
    if (glext.has_extension("GL_ARB_shader_image_load_store")) {
        draw_abuf1_prg  = new program {shader(shader::FRAGMENT,
                                       "draw_abuf1_frag.glsl")};
        draw_abuf1l_prg = new program {shader(shader::FRAGMENT,
                                       "draw_abuf1l_frag.glsl")};
    }

    for (program *prg: {&draw_tex_prg, &draw_bamy1_prg, &draw_bamc1_prg,
                        draw_abuf1_prg, draw_abuf1l_prg})
    {
        if (!prg) {
            continue;
        }

        *prg << *pass_vsh;
        prg->bind_attrib("in_pos", 0);
        prg->bind_frag("out_col", 0);
    }

    shader *simple_vsh = new shader(shader::VERTEX, "draw_xf_vert.glsl");

    program draw_bf_prg     {shader(shader::FRAGMENT, "draw_bf_frag.glsl")};
    program draw_ff_prg     {shader(shader::FRAGMENT, "draw_ff_frag.glsl")};
    program draw_dp_prg     {shader(shader::FRAGMENT, "draw_dp_frag.glsl")};
    program draw_bfdp_prg   {shader(shader::FRAGMENT, "draw_bfdp_frag.glsl")};
    program draw_ffdp_prg   {shader(shader::FRAGMENT, "draw_ffdp_frag.glsl")};
    program draw_simple_prg {shader(shader::FRAGMENT, "draw_simple_frag.glsl")};
    program draw_meshk_prg  {shader(shader::FRAGMENT, "draw_meshk_frag.glsl")};
    program draw_bamy0_prg  {shader(shader::FRAGMENT, "draw_bamy0_frag.glsl")};
    program draw_bamc0_prg  {shader(shader::FRAGMENT, "draw_bamc0_frag.glsl")};
    program draw_bamc0w_prg {shader(shader::FRAGMENT, "draw_bamc0w_frag.glsl")};
    program draw_adtp0_prg  {shader(shader::FRAGMENT, "draw_adtp0_frag.glsl")};
    program draw_adtp1_prg  {shader(shader::FRAGMENT, "draw_adtp1_frag.glsl")};

    program *draw_abuf0_prg = nullptr;
    if (glext.has_extension("GL_ARB_shader_image_load_store")) {
        draw_abuf0_prg = new program {shader(shader::FRAGMENT, "draw_abuf0_frag.glsl")};
    }

    for (program *prg: {&draw_bf_prg, &draw_ff_prg, &draw_dp_prg,
                        &draw_bfdp_prg, &draw_ffdp_prg, &draw_simple_prg,
                        &draw_meshk_prg, &draw_bamy0_prg, &draw_bamc0_prg,
                        &draw_bamc0w_prg, &draw_adtp0_prg, &draw_adtp1_prg,
                        draw_abuf0_prg})
    {
        if (!prg) {
            continue;
        }

        *prg << *simple_vsh;
        prg->bind_attrib("in_pos", 0);
        prg->bind_attrib("in_nrm", 1);
        prg->bind_attrib("in_col", 2);
        prg->bind_frag("out_col", 0);
    }

    draw_bamy0_prg.bind_frag("out_count", 1);
    draw_bamc0_prg.bind_frag("out_transp", 1);
    draw_bamc0w_prg.bind_frag("out_transp", 1);

    obj entity = load_obj(entity_name), entity_copy = entity;
    std::vector<ObjectSection> entity_secs;
    for (obj_section &sec: entity.sections) {
        entity_secs.emplace_back();
        entity_secs.back().va = sec.make_vertex_array(0, -1, 1);
        entity_secs.back().rel_mv = mat4::identity();
        if (two_objects) {
            entity_secs.back().rel_mv.translate(vec3(-2.f, 0.f, 0.f));
        }

        vec3 *col_arr = new vec3[sec.positions.size()];
        for (size_t i = 0; i < sec.positions.size(); i++) {
            if (entity_gradient) {
                float green = (sec.positions[i].z() - entity.lower_left.z())
                            / (entity.upper_right.z() - entity.lower_left.z());
                col_arr[i] = vec3(1.f, green, 0.f);
            } else {
                col_arr[i] = sec.material.diffuse;
            }
        }
        entity_secs.back().va->attrib(2)->format(3);
        entity_secs.back().va->attrib(2)->data(col_arr);
    }

    if (two_objects) {
        for (obj_section &sec: entity_copy.sections) {
            // Invert triangle order
            size_t l = sec.positions.size();
            assert(!(l % 3) && l == sec.normals.size());
            // Works with uneven l, too
            for (size_t i = 0; i < l / 2; i += 3) {
                for (std::vector<vec3> *vtx: {&sec.positions, &sec.normals}) {
                    for (int j = 0; j < 3; j++) {
                        vec3 v = (*vtx)[i + j];
                        (*vtx)[i + j] = (*vtx)[l - i - 3 + j];
                        (*vtx)[l - i - 3 + j] = v;
                    }
                }
            }

            entity_secs.emplace_back();
            entity_secs.back().va = sec.make_vertex_array(0, -1, 1);
            entity_secs.back().rel_mv = mat4::identity().translated(vec3(2.f, 0.f, 0.f));

            vec3 *col_arr = new vec3[sec.positions.size()];
            for (size_t i = 0; i < sec.positions.size(); i++) {
                if (entity_gradient) {
                    float green = (sec.positions[i].z() - entity.lower_left.z())
                                / (entity.upper_right.z() - entity.lower_left.z());
                    col_arr[i] = vec3(1.f, green, 0.f);
                } else {
                    col_arr[i] = sec.material.diffuse;
                }
            }
            entity_secs.back().va->attrib(2)->format(3);
            entity_secs.back().va->attrib(2)->data(col_arr);
        }
    }

    std::vector<ObjectSection> quad_secs;
    for (int x: {-1, 1}) {
        if (!two_objects) {
            if (x < 0) {
                x = 0;
            } else {
                break;
            }
        }
        for (int rl = 0; rl < 3; rl++) {
            int l = x < 0 ? rl : 2 - rl;

            vec3 pos[4], nrm[4], col[4];
            for (int i = 0; i < 4; i++) {
                pos[i] = vec3(quad_vertex_positions[i]);
                nrm[i] = vec3(0.f, 0.f, 1.f);
                col[i] = l == 0 ? vec3(1.f, .5f, .5f)
                       : l == 1 ? vec3(.5f, 1.f, .5f)
                       :          vec3(.5f, .5f, 1.f);
            }


            quad_secs.emplace_back();
            quad_secs.back().va = new vertex_array;
            quad_secs.back().va->set_elements(4);
            quad_secs.back().va->attrib(0)->format(3);
            quad_secs.back().va->attrib(0)->data(pos);
            quad_secs.back().va->attrib(1)->format(3);
            quad_secs.back().va->attrib(1)->data(nrm);
            quad_secs.back().va->attrib(2)->format(3);
            quad_secs.back().va->attrib(2)->data(col);
            quad_secs.back().rel_mv = mat4::identity().translated(vec3(x * 2.f + l * .2f, -l * .2f, l * .1f));
        }
    }


    framebuffer fbs[2] = {
        framebuffer(1),
        framebuffer(1)
    };
    framebuffer fb_bamy(2, GL_RGB16F), fb_bamc(2);

    fb_bamc.color_format(0, GL_RGBA16F);
    fb_bamc.color_format(1, GL_RED);

    fbs[0].resize(WIDTH, HEIGHT);
    fbs[1].resize(WIDTH, HEIGHT);
    fb_bamy.resize(WIDTH, HEIGHT);
    fb_bamc.resize(WIDTH, HEIGHT);

    fbs[0].depth().tmu() = 1;
    fbs[1].depth().tmu() = 1;
    fb_bamy[1].tmu() = 1;
    fb_bamc[1].tmu() = 1;

    texture adtp_a, adtp_d, adtp_l;
    adtp_a.format(GL_RGBA8_SNORM, WIDTH, HEIGHT);
    adtp_d.format(GL_RGBA16_SNORM, WIDTH, HEIGHT);
    adtp_d.tmu() = 1;
    adtp_l.format(GL_R32UI, WIDTH, HEIGHT, GL_RED_INTEGER);


    mat4 mv = mat4::identity().translated(vec3(0.f, 0.f, -5.f));
    float aspect = static_cast<float>(WIDTH) / HEIGHT;
    //mat4 p = mat4::projection(M_PIf / 32.f, aspect, .1f, 10.f);
    float lr = two_objects ? 4.f : 2.f;
    mat4 p = mat4::orthographic(-lr, lr, lr / aspect, -lr / aspect, 0.f, 10.f);

    std::default_random_engine reng;
    std::uniform_real_distribution<float> dist(-.8f, .8f);
    float z_comp = 0.f, z_target = 0.f, z_comp_deriv = 0.f;

    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();

    enum Mode {
        BLEND_ALPHA,
        BLEND_ALPHA_DP,
        ABUFFER_LL,
        ADAPTIVE_TRANSPARENCY,
        BLEND_MESHKIN,
        BLEND_BAVOIL_MYER,
        BLEND_BAVOIL_MCGUIRE,
        BLEND_BAVOIL_MCGUIRE_WEIGHT,
        SS_REFRACT,
        SS_REFRACT_DP,
        BLEND_ADD,
        BLEND_MULT,

        MODE_MAX
    } mode = BLEND_ALPHA;

    const char *mode_str[] = {
        "plain alpha blending",
        "alpha blending with depth peeling",
        "alpha blending with a A-Buffer (linked list)",
        "adaptive transparency",
        "Meshkin's blending",
        "Bavoil's and Myer's blending",
        "Bavoil's and McGuire's blending",
        "Bavoil's and McGuire's blending with depth weighting",
        "screen-space refraction and absorption",
        "screen-space refraction and absorption with depth peeling",
        "additive blending",
        "multiplicative blending"
    };

    char window_title[128];
    snprintf(window_title, sizeof(window_title), "transp - %s", mode_str[mode]);
    SDL_SetWindowTitle(wnd, window_title);

    enum Objects {
        SUZANNE,
        QUADS,

        OBJECTS_MAX
    } objects = SUZANNE;


    std::vector<ObjectSection> *cur_obj = &entity_secs;
    GLenum cur_draw_mode = GL_TRIANGLES;
    bool need_fbs = false;
    bool pause_motion = false;
    int dp_layer = -1;


    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 0;
            } else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                    case SDLK_BACKSPACE:
                        if (event.key.keysym.sym == SDLK_SPACE) {
                            mode = static_cast<Mode>((static_cast<int>(mode) + 1) % MODE_MAX);
                        } else {
                            mode = static_cast<Mode>((static_cast<int>(mode) + MODE_MAX - 1) % MODE_MAX);
                        }
                        need_fbs = mode == BLEND_ALPHA_DP
                                || mode == BLEND_MESHKIN
                                || mode == BLEND_BAVOIL_MYER
                                || mode == BLEND_BAVOIL_MCGUIRE
                                || mode == BLEND_BAVOIL_MCGUIRE_WEIGHT
                                || mode == SS_REFRACT || mode == SS_REFRACT_DP;
                        snprintf(window_title, sizeof(window_title), "transp - %s", mode_str[mode]);
                        SDL_SetWindowTitle(wnd, window_title);
                        break;

                    case SDLK_RETURN:
                        objects = static_cast<Objects>((static_cast<int>(objects) + 1) % OBJECTS_MAX);
                        cur_obj = objects == SUZANNE ? &entity_secs : &quad_secs;
                        cur_draw_mode = objects == SUZANNE ? GL_TRIANGLES: GL_TRIANGLE_STRIP;
                        break;

                    case SDLK_p:
                        pause_motion ^= true;
                        break;

                    case SDLK_l:
                        dp_layer += 1;
                        break;
                }

                if (dp_layer >=
                       (mode == BLEND_ALPHA_DP ? 8
                      : mode == ABUFFER_LL     ? 8
                      : mode == SS_REFRACT_DP  ? 4
                      : 0))
                {
                    dp_layer = -1;
                }
            }
        }

        std::chrono::system_clock::time_point ntp = std::chrono::system_clock::now();
        float diff = std::chrono::duration_cast<std::chrono::microseconds>(ntp - tp).count() / 1000000.f;
        tp = ntp;

        if (objects == SUZANNE && !pause_motion) {
            mv.rotate(diff, vec3(0.f, 1.f, z_comp));

            if (fabsf(z_comp - z_target) < .01f) {
                z_target = dist(reng);
            }
            z_comp += z_comp_deriv * diff;
            z_comp_deriv += (z_target - z_comp) * (diff / 10.f);
        } else if (objects != SUZANNE) {
            mv = mat4::identity().translated(vec3(0.f, 0.f, -5.f));
            z_comp = z_target = z_comp_deriv = 0.f;
        }

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
        quad.draw(GL_TRIANGLE_STRIP);

        glDepthMask(true);

        if (need_fbs) {
            fbs[1].bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            fbs[0].blit();
        }

        switch (mode) {
            case BLEND_ALPHA:
                glEnable(GL_BLEND);
                // Premultiplied source (the shaders do that premultiplication)
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                simple_draw(mv, p, draw_simple_prg, .5f, *cur_obj,
                            cur_draw_mode);
                glDisable(GL_BLEND);
                break;

            case BLEND_ALPHA_DP:
                blend_alpha_dp(fbs, mv, p, draw_dp_prg, dp_layer,
                               dp_layer >= 0 ? 1.f : .5f, *cur_obj,
                               cur_draw_mode);
                break;

            case ABUFFER_LL:
                if (draw_abuf0_prg && draw_abuf1_prg && draw_abuf1l_prg) {
                    abuffer_ll(mv, p, *draw_abuf0_prg,
                               dp_layer >= 0 ? *draw_abuf1l_prg
                                             : *draw_abuf1_prg,
                               abuf_ll_head, abuf_list_data, dp_layer,
                               dp_layer >= 0 ? 1.f : .5f, *cur_obj,
                               cur_draw_mode, quad);
                }
                break;

            case ADAPTIVE_TRANSPARENCY:
                adaptive_transp(adtp_a, adtp_d, adtp_l, mv, p, draw_adtp0_prg,
                                draw_adtp1_prg, .5f, *cur_obj, cur_draw_mode);
                break;

            case BLEND_MESHKIN:
                blend_meshkin(fbs, mv, p, draw_meshk_prg, .5f, *cur_obj,
                              cur_draw_mode);
                break;

            case BLEND_BAVOIL_MYER:
                blend_bamy(fbs[0], fb_bamy, mv, p, draw_bamy0_prg,
                           draw_bamy1_prg, .5f, *cur_obj, cur_draw_mode, quad);
                break;

            case BLEND_BAVOIL_MCGUIRE:
                blend_bamc(fbs[0], fb_bamc, mv, p, draw_bamc0_prg,
                           draw_bamc1_prg, .5f, *cur_obj, cur_draw_mode, quad);
                break;

            case BLEND_BAVOIL_MCGUIRE_WEIGHT:
                blend_bamc(fbs[0], fb_bamc, mv, p, draw_bamc0w_prg,
                           draw_bamc1_prg, .5f, *cur_obj, cur_draw_mode, quad);
                break;

            case SS_REFRACT:
                ss_refract(fbs, mv, p, draw_bf_prg, draw_ff_prg, *cur_obj,
                           cur_draw_mode);
                break;

            case SS_REFRACT_DP:
                ss_refract_dp(fbs, mv, p, draw_bfdp_prg, draw_ffdp_prg,
                              dp_layer, *cur_obj, cur_draw_mode);
                break;

            case BLEND_ADD:
                glEnable(GL_BLEND);
                // Premultiplied source
                glBlendFunc(GL_ONE, GL_ONE);
                simple_draw(mv, p, draw_simple_prg, .2f, *cur_obj,
                            cur_draw_mode);
                glDisable(GL_BLEND);
                break;

            case BLEND_MULT:
                glEnable(GL_BLEND);
                // Premultiplied source
                glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                simple_draw(mv, p, draw_simple_prg, .8f, *cur_obj,
                            cur_draw_mode);
                glDisable(GL_BLEND);
                break;

            case MODE_MAX:
                abort();
        }

        SDL_GL_SwapWindow(wnd);
    }


    return 0;
}
