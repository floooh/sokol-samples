//------------------------------------------------------------------------------
//  vertextexture-sapp.c
//
//  Render to render target texture, and then use this texture to displace
//  vertices in a vertex shader.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "vertextexture-sapp.glsl.h"
#include <stdlib.h>

// plane number of tiles along edge (don't change this since the value is hardcoded in shader)
#define NUM_TILES_ALONG_EDGE (255)

static struct {
    double time;
    float ry;
    struct {
        sg_image img;
        sg_pipeline pip;
        sg_pass pass;
        plasma_params_t plasma_params;
    } offscreen;
    struct {
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
        sg_bindings bind;
    } display;
} state;

static vs_params_t compute_vsparams(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // render target texture for GPU-rendered plasma
    state.offscreen.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 256,
        .height = 256,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "plasma-texture",
    });

    // render-pass attachments and pass action for the offscreen render pass
    state.offscreen.pass = (sg_pass){
        .attachments = sg_make_attachments(&(sg_attachments_desc){
            .colors[0].image = state.offscreen.img,
            .label = "plasma-attachments",
        }),
        .action = {
            .colors[0] = { .load_action = SG_LOADACTION_DONTCARE },
        },
    };

    // pipeline object for offscreen rendering, vertices will be synthesized
    // in the vertex shader, so don't need a vertex buffer or vertex layout
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(plasma_shader_desc(sg_query_backend())),
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .sample_count = 1,
        .label = "plasma-pipeline",
    });

    // an index buffer with triangle indices for a 256x256 plane
    {
        const size_t tiles = NUM_TILES_ALONG_EDGE;
        const size_t ibuf_size = tiles * tiles * 6 * sizeof(uint16_t);
        uint16_t* indices = malloc(ibuf_size);
        uint16_t* ptr = indices;
        for (size_t y = 0; y < tiles; y++) {
            for (size_t x = 0; x < tiles; x++) {
                uint16_t i0 = y * (tiles + 1) + x;
                uint16_t i1 = i0 + 1;
                uint16_t i2 = i0 + tiles + 1;
                uint16_t i3 = i2 + 1;
                *ptr++ = i0; *ptr++ = i1; *ptr++ = i3;
                *ptr++ = i0; *ptr++ = i3; *ptr++ = i2;
            }
        }
        state.display.ibuf = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .data = { .ptr = indices, .size = ibuf_size },
            .label = "plane-indices",
        });
        free(indices);
    }

    // a pipeline object for rendering the vertex-displaced plane,
    // again, vertices will be synthesized in the shader so no vertex buffer
    // or vertex layout needed
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .label = "render-pipeline",
    });

    // display pass action (clear to black)
    state.display.pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1 } },
    };

    // a sampler for accessing the render target as texture
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .label = "plasma-sampler",
    });

    // bindings for the display-pass
    state.display.bind = (sg_bindings) {
        .index_buffer = state.display.ibuf,
        .images[IMG_tex] = state.offscreen.img,
        .samplers[SMP_smp] = smp,
    };
}

static void frame(void) {
    state.offscreen.plasma_params.time += sapp_frame_duration();

    // offscreen pass to render plasma, this renders an offscreen-triangle
    // with vertices synthesized in the vertex shader
    sg_begin_pass(&state.offscreen.pass);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_uniforms(UB_plasma_params, &SG_RANGE(state.offscreen.plasma_params));
    sg_draw(0, 3, 1);
    sg_end_pass();

    // display pass to render vertex-displaced plane
    const int num_elements = NUM_TILES_ALONG_EDGE * NUM_TILES_ALONG_EDGE * 6;
    const vs_params_t vs_params = compute_vsparams();
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&state.display.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, num_elements, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

// compute the model-view-projection matrix used in the display pass
static vs_params_t compute_vsparams(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float t = (float)(sapp_frame_duration() * 60.0);
    hmm_mat4 proj = HMM_Perspective(60.0f, w/h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.0f, 3.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.ry += 0.5f * t;
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = rym;
    return (vs_params_t){
        .mvp = HMM_MultiplyMat4(view_proj, model),
    };
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
        .sample_count = 4,
        .window_title = "vertextexture-sapp.c",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
