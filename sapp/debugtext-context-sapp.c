//------------------------------------------------------------------------------
//  debugtext-context-sapp.c
//
//  sokol_debugtext.h: Demonstrate rendering to offscreen render targets
//  with contexts.
//
//  Renders a cube with a different render target texture on each side,
//  and each render target containing debug text. Text rendering into
//  render targets uses different framebuffer attributes than the default
//  framebuffer (no depth buffer, no MSAA), so this needs to happen
//  with separate sokol-debugtext contexts.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "dbgui/dbgui.h"
#include "debugtext-context-sapp.glsl.h"

#define FONT_KC853 (0)
#define FONT_KC854 (1)
#define FONT_Z1013 (2)
#define FONT_CPC   (3)
#define FONT_C64   (4)
#define FONT_ORIC  (5)

#define NUM_FACES (6)
#define OFFSCREEN_PIXELFORMAT   (SG_PIXELFORMAT_RGBA8)
#define OFFSCREEN_SAMPLE_COUNT  (1)
#define OFFSCREEN_WIDTH         (32)
#define OFFSCREEN_HEIGHT        (32)
#define DISPLAY_SAMPLE_COUNT    (4)

static struct {
    float rx, ry;
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pipeline pip;
    sg_pass_action pass_action;     // just keep this default-initialized, which clears to gray
    struct {
        sdtx_context text_context;
        sg_image img;
        sg_pass render_pass;
        sg_pass_action pass_action;
    } passes[NUM_FACES];
} state;

typedef struct {
    float x, y, z;
    uint16_t u, v;
} vertex_t;

// face background colors
static const sg_color bg[NUM_FACES] = {
    { 0.0f, 0.0f, 0.5f, 1.0f },
    { 0.0f, 0.5f, 0.0f, 1.0f },
    { 0.5f, 0.0f, 0.0f, 1.0f },
    { 0.5f, 0.0f, 0.25f, 1.0f },
    { 0.5f, 0.25f, 0.0f, 1.0f },
    { 0.0f, 0.25f, 0.5f, 1.0f }
};

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    // setup sokol-debugtext using all builtin fonts
    sdtx_setup(&(sdtx_desc_t){
        .fonts = {
            [FONT_KC853] = sdtx_font_kc853(),
            [FONT_KC854] = sdtx_font_kc854(),
            [FONT_Z1013] = sdtx_font_z1013(),
            [FONT_CPC]   = sdtx_font_cpc(),
            [FONT_C64]   = sdtx_font_c64(),
            [FONT_ORIC]  = sdtx_font_oric()
        },
    });

    // create resources to render a textured cube (vertex buffer, index buffer
    // shader and pipeline state object)
    vertex_t vertices[] = {
        /* pos                  uvs */
        { -1.0f, -1.0f, -1.0f,      0,     0 },
        {  1.0f, -1.0f, -1.0f,  32767,     0 },
        {  1.0f,  1.0f, -1.0f,  32767, 32767 },
        { -1.0f,  1.0f, -1.0f,      0, 32767 },
        { -1.0f, -1.0f,  1.0f,  32767,     0 },
        {  1.0f, -1.0f,  1.0f,      0,     0 },
        {  1.0f,  1.0f,  1.0f,      0, 32767 },
        { -1.0f,  1.0f,  1.0f,  32767, 32767 },
        { -1.0f, -1.0f, -1.0f,      0,     0 },
        { -1.0f,  1.0f, -1.0f,  32767,     0 },
        { -1.0f,  1.0f,  1.0f,  32767, 32767 },
        { -1.0f, -1.0f,  1.0f,      0, 32767 },
        {  1.0f, -1.0f, -1.0f,  32767,     0 },
        {  1.0f,  1.0f, -1.0f,      0,     0 },
        {  1.0f,  1.0f,  1.0f,      0, 32767 },
        {  1.0f, -1.0f,  1.0f,  32767, 32767 },
        { -1.0f, -1.0f, -1.0f,      0,     0 },
        { -1.0f, -1.0f,  1.0f,  32767,     0 },
        {  1.0f, -1.0f,  1.0f,  32767, 32767 },
        {  1.0f, -1.0f, -1.0f,      0, 32767 },
        { -1.0f,  1.0f, -1.0f,  32767,     0 },
        { -1.0f,  1.0f,  1.0f,      0,     0 },
        {  1.0f,  1.0f,  1.0f,      0, 32767 },
        {  1.0f,  1.0f, -1.0f,  32767, 32767 },
    };
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });
    uint16_t indices[] = {
         0,  1,  2,   0,  2,  3,
         6,  5,  4,   7,  6,  4,
         8,  9, 10,   8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_texcoord0].format = SG_VERTEXFORMAT_SHORT2N
            }
        },
        .shader = sg_make_shader(debugtext_context_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "cube-pipeline"
    });

    // create resources for each offscreen-rendered cube face
    for (int i = 0; i < NUM_FACES; i++) {
        // each face gets its separate text context, the text canvas size
        // will remain fixed, so we can just provide the default canvas
        // size here and don't need to call sdtx_canvas() later
        state.passes[i].text_context = sdtx_make_context(&(sdtx_context_desc_t) {
            .char_buf_size = 64,
            .canvas_width = OFFSCREEN_WIDTH,
            .canvas_height = OFFSCREEN_HEIGHT / 2,
            .color_format = OFFSCREEN_PIXELFORMAT,
            .depth_format = SG_PIXELFORMAT_NONE,
            .sample_count = OFFSCREEN_SAMPLE_COUNT
        });

        // the render target texture, render pass
        state.passes[i].img = sg_make_image(&(sg_image_desc){
            .render_target = true,
            .width = OFFSCREEN_WIDTH,
            .height = OFFSCREEN_HEIGHT,
            .pixel_format = OFFSCREEN_PIXELFORMAT,
            .sample_count = OFFSCREEN_SAMPLE_COUNT,
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST
        });
        state.passes[i].render_pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0].image = state.passes[i].img
        });

        // each render target is cleared to a different background color
        state.passes[i].pass_action = (sg_pass_action){
            .colors[0] = {
                .action = SG_ACTION_CLEAR,
                .value = bg[i],
            }
        };
    }
}

// compute the model-view-proj matrix for rendering the rotating cube
vs_params_t compute_vs_params(int w, int h) {
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)w/(float)h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
    return vs_params;
}

static void frame(void) {
    const int disp_width = sapp_width();
    const int disp_height = sapp_height();
    const float t = (float)(sapp_frame_duration() * 60.0);
    const uint32_t frame_count = (uint32_t)sapp_frame_count();
    state.rx += 0.25f * t;
    state.ry += 0.5f * t;
    vs_params_t vs_params = compute_vs_params(disp_width, disp_height);

    // text in the main display
    sdtx_set_context(SDTX_DEFAULT_CONTEXT);
    sdtx_canvas(disp_width * 0.5f, disp_height * 0.5f);
    sdtx_origin(3, 3);
    sdtx_puts("Hello from main context!\n");
    sdtx_printf("Frame count: %d\n", frame_count);

    // text in each offscreen render target
    for (int i = 0; i < NUM_FACES; i++) {
        sdtx_set_context(state.passes[i].text_context);
        sdtx_origin(1.0f, 0.5f);
        sdtx_font(i);
        sdtx_printf("%02X", ((frame_count / 16) + (uint32_t)i)& 0xFF);
    }

    // rasterize text into offscreen render targets, we could also put this
    // right into the loop above, but this shows that the "text definition"
    // can be decoupled from the actual rendering
    for (int i = 0; i < NUM_FACES; i++) {
        sg_begin_pass(state.passes[i].render_pass, &state.passes[i].pass_action);
        sdtx_set_context(state.passes[i].text_context);
        sdtx_draw();
        sg_end_pass();
    }

    // finally render to the default framebuffer
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());

    // draw the cube as 6 separate draw calls (because each has its own texture)
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    for (int i = 0; i < NUM_FACES; i++) {
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.vbuf,
            .index_buffer = state.ibuf,
            .fs_images[0] = state.passes[i].img
        });
        sg_draw(i * 6, 6, 1);
    }

    // draw default-display text
    sdtx_set_context(SDTX_DEFAULT_CONTEXT);
    sdtx_draw();

    // conclude the default pass and frame
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .sample_count = DISPLAY_SAMPLE_COUNT,
        .gl_force_gles2 = true,
        .window_title = "debugtext-context-sapp",
        .icon.sokol_default = true,
    };
}
