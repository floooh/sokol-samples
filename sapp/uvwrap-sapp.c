//------------------------------------------------------------------------------
//  uvwrap-sapp.c
//  Demonstrates and tests texture coordinate wrapping modes.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "uvwrap-sapp.glsl.h"

static struct {
    sg_buffer vbuf;
    sg_image img[_SG_WRAP_NUM];
    sg_pipeline pip;
    sg_pass_action pass_action;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    /* a quad vertex buffer with "oversized" texture coords */
    const float quad_vertices[] = {
        -1.0f, +1.0f,
        +1.0f, +1.0f,
        -1.0f, -1.0f,
        +1.0f, -1.0f,
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .content = quad_vertices,
        .size = sizeof(quad_vertices)
    });

    /* one test image per UV-wrap mode */
    const uint32_t o = 0xFF555555;
    const uint32_t W = 0xFFFFFFFF;
    const uint32_t R = 0xFF0000FF;
    const uint32_t G = 0xFF00FF00;
    const uint32_t B = 0xFFFF0000;
    const uint32_t test_pixels[8][8] = {
        { R, R, R, R, G, G, G, G },
        { R, o, o, o, o, o, o, G },
        { R, o, o, o, o, o, o, G },
        { R, o, o, W, W, o, o, G },
        { B, o, o, W, W, o, o, R },
        { B, o, o, o, o, o, o, R },
        { B, o, o, o, o, o, o, R },
        { B, B, B, B, R, R, R, R },
    };
    for (int i = SG_WRAP_REPEAT; i <= SG_WRAP_MIRRORED_REPEAT; i++) {
        state.img[i] = sg_make_image(&(sg_image_desc){
            .width = 8,
            .height = 8,
            .wrap_u = (sg_wrap) i,
            .wrap_v = (sg_wrap) i,
            .border_color = SG_BORDERCOLOR_OPAQUE_BLACK,
            .content.subimage[0][0] = {
                .ptr = test_pixels,
                .size = sizeof(test_pixels)
            }
        });
    }

    /* a pipeline state object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(uvwrap_shader_desc()),
        .layout = {
            .attrs[ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT2
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
    });

    /* pass action to clear to a background color */
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .val={0.0f, 0.5f, 0.7f, 1.0f } }
    };
}

static void frame(void) {
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    for (int i = SG_WRAP_REPEAT; i <= SG_WRAP_MIRRORED_REPEAT; i++) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .fs_images[0] = state.img[i]
        });
        float x_offset = 0, y_offset = 0;
        switch (i) {
            case SG_WRAP_REPEAT:            x_offset = -0.5f; y_offset = 0.5f; break;
            case SG_WRAP_CLAMP_TO_EDGE:     x_offset = +0.5f; y_offset = 0.5f; break;
            case SG_WRAP_CLAMP_TO_BORDER:   x_offset = -0.5f; y_offset = -0.5f; break;
            case SG_WRAP_MIRRORED_REPEAT:   x_offset = +0.5f; y_offset = -0.5f; break;
        }
        vs_params_t vs_params = {
            .offset = { x_offset, y_offset },
            .scale = { 0.4f, 0.4f }
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &vs_params, sizeof(vs_params));
        sg_draw(0, 4, 1);
    }
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .window_title = "UV Wrap Modes"
    };
}

