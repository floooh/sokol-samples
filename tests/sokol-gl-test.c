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
    T(_sgl.state[SGL_ORIGIN_TOP_LEFT]);
    T(!_sgl.state[SGL_ALPHABLEND]);
    T(!_sgl.state[SGL_TEXTURING]);
    T(!_sgl.state[SGL_CULL_FACE]);
    T((_sgl.u == 0) && (_sgl.v == 0));
    TFLT(_sgl.u_scale, 1.0f, FLT_MIN);
    TFLT(_sgl.v_scale, 1.0f, FLT_MIN);
    T(_sgl.rgba == 0xFFFFFFFF);
    T(_sgl.tex.id == SG_INVALID_ID);
    shutdown();
}

static void test_enable_disable(void) {
    test("enable/disable");
    init();
    sgl_enable(SGL_ORIGIN_TOP_LEFT); T(sgl_is_enabled(SGL_ORIGIN_TOP_LEFT));
    sgl_enable(SGL_ALPHABLEND); T(sgl_is_enabled(SGL_ALPHABLEND));
    sgl_enable(SGL_TEXTURING); T(sgl_is_enabled(SGL_TEXTURING));
    sgl_enable(SGL_CULL_FACE); T(sgl_is_enabled(SGL_CULL_FACE));
    sgl_disable(SGL_ORIGIN_TOP_LEFT); T(!sgl_is_enabled(SGL_ORIGIN_TOP_LEFT));
    sgl_disable(SGL_ALPHABLEND); T(!sgl_is_enabled(SGL_ALPHABLEND));
    sgl_disable(SGL_TEXTURING); T(!sgl_is_enabled(SGL_TEXTURING));
    sgl_disable(SGL_CULL_FACE); T(!sgl_is_enabled(SGL_CULL_FACE));
    shutdown();
}

static void test_viewport(void) {
    test("viewport");
    init();
    sgl_viewport(1, 2, 3, 4);
    T(_sgl.cur_command == 1);
    T(_sgl.commands[0].cmd = SGL_COMMAND_VIEWPORT);
    T(_sgl.commands[0].args.viewport.x == 1);
    T(_sgl.commands[0].args.viewport.y == 2);
    T(_sgl.commands[0].args.viewport.w == 3);
    T(_sgl.commands[0].args.viewport.h == 4);
    T(_sgl.commands[0].args.viewport.origin_top_left);
    sgl_disable(SGL_ORIGIN_TOP_LEFT);
    sgl_viewport(5, 6, 7, 8);
    T(_sgl.cur_command == 2);
    T(_sgl.commands[1].cmd = SGL_COMMAND_VIEWPORT);
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
    sgl_scissor_rect(10, 20, 30, 40);
    T(_sgl.cur_command == 1);
    T(_sgl.commands[0].cmd = SGL_COMMAND_SCISSOR_RECT);
    T(_sgl.commands[0].args.scissor_rect.x == 10);
    T(_sgl.commands[0].args.scissor_rect.y == 20);
    T(_sgl.commands[0].args.scissor_rect.w == 30);
    T(_sgl.commands[0].args.scissor_rect.h == 40);
    T(_sgl.commands[0].args.scissor_rect.origin_top_left);
    sgl_disable(SGL_ORIGIN_TOP_LEFT);
    sgl_scissor_rect(50, 60, 70, 80);
    T(_sgl.cur_command == 2);
    T(_sgl.commands[1].cmd = SGL_COMMAND_SCISSOR_RECT);
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
    T(_sgl.tex.id == SG_INVALID_ID);
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
    sgl_texcoord_int_bits(3);
    TFLT(_sgl.u_scale, 8.0f, FLT_MIN);
    TFLT(_sgl.v_scale, 8.0f, FLT_MIN);
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
    return test_end();
}