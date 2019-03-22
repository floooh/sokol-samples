//------------------------------------------------------------------------------
//  sokol-gl-test.c
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_DUMMY_BACKEND
#include "sokol_gfx.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "test.h"

static void init(void) {
    sg_setup(&(sg_desc){0});
    sgl_setup(&(sgl_desc_t){0});
}

static void shutdown(void) {
    sgl_shutdown();
    sg_shutdown();
}

static void test_default_init_shutdown(void) {
    test("default init/shutdown");
    init();
    T(_sgl.init_cookie == _SGL_INIT_COOKIE);
    T(_sgl.num_vertices == 65536);
    T(_sgl.num_commands == 16384);
    T(_sgl.num_uniforms == 16384);
    T(_sgl.cur_vertex == 0);
    T(_sgl.cur_command == 0);
    T(_sgl.cur_uniform == 0);
    T(_sgl.vertices != 0);
    T(_sgl.uniforms != 0);
    T(_sgl.commands != 0);
    T(_sgl.error == SGL_NO_ERROR);
    T(!_sgl.in_begin);
    T(!_sgl_state(SGL_DEPTH_TEST, _sgl.state_bits));
    T(!_sgl_state(SGL_BLEND, _sgl.state_bits));
    T(!_sgl_state(SGL_TEXTURING, _sgl.state_bits));
    T(!_sgl_state(SGL_CULL_FACE, _sgl.state_bits));
    T((_sgl.u == 0) && (_sgl.v == 0));
    TFLT(_sgl.u_scale, 1.0f, FLT_MIN);
    TFLT(_sgl.v_scale, 1.0f, FLT_MIN);
    T(_sgl.rgba == 0xFFFFFFFF);
    T(_sgl.tex.id == _sgl.img.id);
    shutdown();
}

static void test_enable_disable(void) {
    test("enable/disable");
    init();
    /* also test that primitive type doesn't mess up state bits */
    for (int prim_type = 0; prim_type < SGL_NUM_PRIMITIVE_TYPES; prim_type++) {
        sgl_begin(prim_type);
        T(_sgl_prim_type(_sgl.state_bits) == prim_type);
        sgl_end();
        T(_sgl_prim_type(_sgl.state_bits) == prim_type);
        sgl_enable(SGL_DEPTH_TEST); T(sgl_is_enabled(SGL_DEPTH_TEST));
        sgl_enable(SGL_BLEND); T(sgl_is_enabled(SGL_BLEND));
        sgl_enable(SGL_TEXTURING); T(sgl_is_enabled(SGL_TEXTURING));
        sgl_enable(SGL_CULL_FACE); T(sgl_is_enabled(SGL_CULL_FACE));
        sgl_disable(SGL_DEPTH_TEST); T(!sgl_is_enabled(SGL_DEPTH_TEST));
        sgl_disable(SGL_BLEND); T(!sgl_is_enabled(SGL_BLEND));
        sgl_disable(SGL_TEXTURING); T(!sgl_is_enabled(SGL_TEXTURING));
        sgl_disable(SGL_CULL_FACE); T(!sgl_is_enabled(SGL_CULL_FACE));
        T(_sgl_prim_type(_sgl.state_bits) == prim_type);
    }
    shutdown();
}

static void test_viewport(void) {
    test("viewport");
    init();
    sgl_viewport(1, 2, 3, 4, true);
    T(_sgl.cur_command == 1);
    T(_sgl.commands[0].cmd == SGL_COMMAND_VIEWPORT);
    T(_sgl.commands[0].args.viewport.x == 1);
    T(_sgl.commands[0].args.viewport.y == 2);
    T(_sgl.commands[0].args.viewport.w == 3);
    T(_sgl.commands[0].args.viewport.h == 4);
    T(_sgl.commands[0].args.viewport.origin_top_left);
    sgl_viewport(5, 6, 7, 8, false);
    T(_sgl.cur_command == 2);
    T(_sgl.commands[1].cmd == SGL_COMMAND_VIEWPORT);
    T(_sgl.commands[1].args.viewport.x == 5);
    T(_sgl.commands[1].args.viewport.y == 6);
    T(_sgl.commands[1].args.viewport.w == 7);
    T(_sgl.commands[1].args.viewport.h == 8);
    T(!_sgl.commands[1].args.viewport.origin_top_left);
    shutdown();
}

static void test_scissor_rect(void) {
    test("scissor rect");
    init();
    sgl_scissor_rect(10, 20, 30, 40, true);
    T(_sgl.cur_command == 1);
    T(_sgl.commands[0].cmd == SGL_COMMAND_SCISSOR_RECT);
    T(_sgl.commands[0].args.scissor_rect.x == 10);
    T(_sgl.commands[0].args.scissor_rect.y == 20);
    T(_sgl.commands[0].args.scissor_rect.w == 30);
    T(_sgl.commands[0].args.scissor_rect.h == 40);
    T(_sgl.commands[0].args.scissor_rect.origin_top_left);
    sgl_scissor_rect(50, 60, 70, 80, false);
    T(_sgl.cur_command == 2);
    T(_sgl.commands[1].cmd == SGL_COMMAND_SCISSOR_RECT);
    T(_sgl.commands[1].args.scissor_rect.x == 50);
    T(_sgl.commands[1].args.scissor_rect.y == 60);
    T(_sgl.commands[1].args.scissor_rect.w == 70);
    T(_sgl.commands[1].args.scissor_rect.h == 80);
    T(!_sgl.commands[1].args.scissor_rect.origin_top_left);
    shutdown();
}

static void test_texture(void) {
    test("texture");
    init();
    T(_sgl.tex.id == _sgl.img.id);
    uint32_t pixels[64] = { 0 };
    sg_image img = sg_make_image(&(sg_image_desc){
        .type = SG_IMAGETYPE_2D,
        .width = 8,
        .height = 8,
        .content = {
            .subimage[0][0] = {
                .ptr = pixels,
                .size = sizeof(pixels)
            }
        }
    });
    sgl_texture(img);
    T(_sgl.tex.id == img.id);
    shutdown();
}

static void test_texcoord_int_bits(void) {
    test("texcoord-int-bits");
    init();
    sgl_texcoord_int_bits(3, 4);
    TFLT(_sgl.u_scale, 8.0f, FLT_MIN);
    TFLT(_sgl.v_scale, 16.0f, FLT_MIN);
    shutdown();
}

static void test_begin_end(void) {
    test("begin end");
    init();
    sgl_enable(SGL_DEPTH_TEST);
    sgl_begin(SGL_TRIANGLES);
    sgl_v3f(1.0f, 2.0f, 3.0f);
    sgl_v3f(4.0f, 5.0f, 6.0f);
    sgl_v3f(7.0f, 8.0f, 9.0f);
    sgl_end();
    T(_sgl.base_vertex == 0);
    T(_sgl.cur_vertex == 3);
    T(_sgl.cur_command == 1);
    T(_sgl.cur_uniform == 1);
    T(_sgl.commands[0].cmd == SGL_COMMAND_DRAW);
    T(_sgl_prim_type(_sgl.commands[0].args.draw.state_bits) == SGL_TRIANGLES);
    T(_sgl_state(SGL_DEPTH_TEST, _sgl.commands[0].args.draw.state_bits));
    T(_sgl.commands[0].args.draw.base_vertex == 0);
    T(_sgl.commands[0].args.draw.num_vertices == 3);
    T(_sgl.commands[0].args.draw.uniform_index == 0);
    shutdown();
}

int main() {
    test_begin("sokol-gl-test");
    test_default_init_shutdown();
    test_enable_disable();
    test_viewport();
    test_scissor_rect();
    test_texture();
    test_texcoord_int_bits();
    test_begin_end();
    return test_end();
}