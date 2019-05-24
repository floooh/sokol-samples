//------------------------------------------------------------------------------
//  sokol-gl-test.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include "utest.h"
#include <float.h>

#define T(b) EXPECT_TRUE(b)
#define TFLT(f0,f1,epsilon) {T(fabs((f0)-(f1))<=(epsilon));}

static void init(void) {
    sg_setup(&(sg_desc){0});
    sgl_setup(&(sgl_desc_t){0});
}

static void shutdown(void) {
    sgl_shutdown();
    sg_shutdown();
}

UTEST(sokol_gl, default_init_shutdown) {
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
    T(_sgl.def_pip.id != SG_INVALID_ID);
    T(_sgl.pip_pool.pool.size == (_SGL_DEFAULT_PIPELINE_POOL_SIZE + 1));
    TFLT(_sgl.u, 0.0f, FLT_MIN);
    TFLT(_sgl.v, 0.0f, FLT_MIN);
    T(_sgl.rgba == 0xFFFFFFFF);
    T(_sgl.cur_img.id == _sgl.def_img.id);
    shutdown();
}

UTEST(sokol_gl, viewport) {
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

UTEST(sokol_gl, scissor_rect) {
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

UTEST(sokol_gl, texture) {
    init();
    T(_sgl.cur_img.id == _sgl.def_img.id);
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
    T(_sgl.cur_img.id == img.id);
    shutdown();
}

UTEST(sokol_gl, begin_end) {
    init();
    sgl_begin_triangles();
    sgl_v3f(1.0f, 2.0f, 3.0f);
    sgl_v3f(4.0f, 5.0f, 6.0f);
    sgl_v3f(7.0f, 8.0f, 9.0f);
    sgl_end();
    T(_sgl.base_vertex == 0);
    T(_sgl.cur_vertex == 3);
    T(_sgl.cur_command == 1);
    T(_sgl.cur_uniform == 1);
    T(_sgl.commands[0].cmd == SGL_COMMAND_DRAW);
    T(_sgl.commands[0].args.draw.pip.id == _sgl_pipeline_at(_sgl.def_pip.id)->pip[SGL_PRIMITIVETYPE_TRIANGLES].id);
    T(_sgl.commands[0].args.draw.base_vertex == 0);
    T(_sgl.commands[0].args.draw.num_vertices == 3);
    T(_sgl.commands[0].args.draw.uniform_index == 0);
    shutdown();
}

UTEST(sokol_gl, matrix_mode) {
    init();
    sgl_matrix_mode_modelview(); T(_sgl.cur_matrix_mode == SGL_MATRIXMODE_MODELVIEW);
    sgl_matrix_mode_projection(); T(_sgl.cur_matrix_mode == SGL_MATRIXMODE_PROJECTION);
    sgl_matrix_mode_texture(); T(_sgl.cur_matrix_mode == SGL_MATRIXMODE_TEXTURE);
    shutdown();
}

UTEST(sokol_gl, load_identity) {
    init();
    sgl_load_identity();
    const _sgl_matrix_t* m = _sgl_matrix_modelview();
    TFLT(m->v[0][0], 1.0f, FLT_MIN); TFLT(m->v[0][1], 0.0f, FLT_MIN); TFLT(m->v[0][2], 0.0f, FLT_MIN); TFLT(m->v[0][3], 0.0f, FLT_MIN);
    TFLT(m->v[1][0], 0.0f, FLT_MIN); TFLT(m->v[1][1], 1.0f, FLT_MIN); TFLT(m->v[1][2], 0.0f, FLT_MIN); TFLT(m->v[1][3], 0.0f, FLT_MIN);
    TFLT(m->v[2][0], 0.0f, FLT_MIN); TFLT(m->v[2][1], 0.0f, FLT_MIN); TFLT(m->v[2][2], 1.0f, FLT_MIN); TFLT(m->v[2][3], 0.0f, FLT_MIN);
    TFLT(m->v[3][0], 0.0f, FLT_MIN); TFLT(m->v[3][1], 0.0f, FLT_MIN); TFLT(m->v[3][2], 0.0f, FLT_MIN); TFLT(m->v[3][3], 1.0f, FLT_MIN);
    shutdown();
}

UTEST(sokol_gl, load_matrix) {
    init();
    const float m[16] = {
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        2.0f, 3.0f, 4.0f, 1.0f
    };
    sgl_load_matrix(m);
    const _sgl_matrix_t* m0 = _sgl_matrix_modelview();
    TFLT(m0->v[0][0], 0.5f, FLT_MIN);
    TFLT(m0->v[1][1], 0.5f, FLT_MIN);
    TFLT(m0->v[2][2], 0.5f, FLT_MIN);
    TFLT(m0->v[3][0], 2.0f, FLT_MIN);
    TFLT(m0->v[3][1], 3.0f, FLT_MIN);
    TFLT(m0->v[3][2], 4.0f, FLT_MIN);
    TFLT(m0->v[3][3], 1.0f, FLT_MIN);
    sgl_load_transpose_matrix(m);
    const _sgl_matrix_t* m1 = _sgl_matrix_modelview();
    TFLT(m1->v[0][0], 0.5f, FLT_MIN);
    TFLT(m1->v[1][1], 0.5f, FLT_MIN);
    TFLT(m1->v[2][2], 0.5f, FLT_MIN);
    TFLT(m1->v[0][3], 2.0f, FLT_MIN);
    TFLT(m1->v[1][3], 3.0f, FLT_MIN);
    TFLT(m1->v[2][3], 4.0f, FLT_MIN);
    TFLT(m1->v[3][3], 1.0f, FLT_MIN);
    shutdown();
}

UTEST(sokol_gl, make_destroy_pipelines) {
    sg_setup(&(sg_desc){0});
    sgl_setup(&(sgl_desc_t){
        /* one pool slot is used by soko-gl itself */
        .pipeline_pool_size = 4
    });

    sgl_pipeline pip[3] = { {0} };
    sg_pipeline_desc desc = {
        .depth_stencil = {
            .depth_write_enabled = true,
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL
        }
    };
    for (int i = 0; i < 3; i++) {
        pip[i] = sgl_make_pipeline(&desc);
        T(pip[i].id != SG_INVALID_ID);
        T((2-i) == _sgl.pip_pool.pool.queue_top);
        const _sgl_pipeline_t* pip_ptr = _sgl_lookup_pipeline(pip[i].id);
        T(pip_ptr);
        T(pip_ptr->slot.id == pip[i].id);
        T(pip_ptr->slot.state == SG_RESOURCESTATE_VALID);
    }
    /* trying to create another one fails because buffer is exhausted */
    T(sgl_make_pipeline(&desc).id == SG_INVALID_ID);

    for (int i = 0; i < 3; i++) {
        sgl_destroy_pipeline(pip[i]);
        T(0 == _sgl_lookup_pipeline(pip[i].id));
        const _sgl_pipeline_t* pip_ptr = _sgl_pipeline_at(pip[i].id);
        T(pip_ptr);
        T(pip_ptr->slot.id == SG_INVALID_ID);
        T(pip_ptr->slot.state == SG_RESOURCESTATE_INITIAL);
        T((i+1) == _sgl.pip_pool.pool.queue_top);
    }
    sgl_shutdown();
    sg_shutdown();
}
