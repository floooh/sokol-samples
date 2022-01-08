//------------------------------------------------------------------------------
//  uniformtypes-sapp.c
//  Test sokol-gfx uniform types and uniform block memory layout.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_color.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "uniformtypes-sapp.glsl.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    vs_params_t vs_params;
} state;

#define NUM_COLORS (10)
static const sg_color pal[NUM_COLORS] = {
    SG_RED, SG_GREEN, SG_BLUE, SG_YELLOW, SG_TURQUOISE,
    SG_VIOLET, SG_SILVER, SG_SALMON, SG_PERU, SG_MAGENTA,
};

static const char* names[NUM_COLORS] = {
    "RED", "GREEN", "BLUE", "YELLOW", "TURQOISE",
    "VIOLET", "SILVER", "SALMON", "PERU", "MAGENTA"
};

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    sdtx_setup(&(sdtx_desc_t){
        .context_pool_size = 1,
        .fonts[0] = sdtx_font_oric()
    });
    __dbgui_setup(sapp_sample_count());
    
    // setup vertex shader uniform block
    state.vs_params.scale[0] = 1.0f;
    state.vs_params.scale[1] = 1.0f;
    state.vs_params.i1 = 0;
    state.vs_params.i2[0] = 1;
    state.vs_params.i2[1] = 2;
    state.vs_params.i3[0] = 3;
    state.vs_params.i3[1] = 4;
    state.vs_params.i3[2] = 5;
    state.vs_params.i4[0] = 6;
    state.vs_params.i4[1] = 7;
    state.vs_params.i4[2] = 8;
    state.vs_params.i4[3] = 9;
    for (int i = 0; i < NUM_COLORS; i++) {
        state.vs_params.pal[i][0] = pal[i].r;
        state.vs_params.pal[i][1] = pal[i].g;
        state.vs_params.pal[i][2] = pal[i].b;
        state.vs_params.pal[i][3] = 1.0;
    }
    
    // a quad vertex buffer, index buffer and pipeline object
    const float vertices[] = {
         0.0f,  0.0f,
        +1.0f,  0.0f,
        +1.0f, +1.0f,
         0.0f, +1.0f,
    };
    const uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(uniformtypes_shader_desc(sg_query_backend())),
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .index_type = SG_INDEXTYPE_UINT16,
    });
      
    // default pass action to clear background to black
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
}

static void frame(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float cw = w * 0.5f;
    const float ch = h * 0.5f;
    const float glyph_w = 8.0f / cw;
    const float glyph_h = 8.0f / ch;
    
    sdtx_canvas(w * 0.5f, h * 0.5f);
    sdtx_origin(3, 3);
    sdtx_color3f(1.0f, 1.0f, 1.0f);
    sdtx_puts("Color names must match\nquad color on same line:\n\n\n");
    for (int i = 0; i < NUM_COLORS; i++) {
        sdtx_color3f(pal[i].r, pal[i].g, pal[i].b);
        sdtx_puts(names[i]);
        sdtx_crlf(); sdtx_crlf();
    }

    sg_begin_default_passf(&state.pass_action, w, h);
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    float x0 = -1.0f + (28.0f * glyph_w);
    float y0 = 1.0f - (16.0f * glyph_h);
    state.vs_params.scale[0] = 5.0f * glyph_w;
    state.vs_params.scale[1] = 2.0f * glyph_h;
    for (int i = 0; i < NUM_COLORS; i++) {
        state.vs_params.sel = i;
        state.vs_params.offset[0] = x0;
        state.vs_params.offset[1] = y0;
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(state.vs_params));
        sg_draw(0, 6, 1);
        y0 -= 4.0f * glyph_h;
    }
    sdtx_draw();
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
        .window_title = "Uniform Types",
        .icon.sokol_default = true,
    };
}
