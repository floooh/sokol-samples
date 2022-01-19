//------------------------------------------------------------------------------
//  imgui-usercallback-sapp.c
//
//  Demonstrate rendering inside an ImGui window with sokol-gfx and
//  sokol-gl using ImGui's UserCallback command.
//
//  Uses cimgui instead of the ImGui C++ API (C vs C++ API is not relevant
//  for the demonstrated feature).
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#include "imgui-usercallback-sapp.glsl.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"

// global application state
static struct {
    sg_pass_action default_pass_action;
    struct {
        float rx, ry;
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } scene1;
    struct {
        float r0, r1;
        sgl_pipeline pip;
    } scene2;
} state;

// vertices and indices for rendering a cube via sokol-gfx
static float cube_vertices[] = {
    -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
     1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
     1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
    -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

    -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
     1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
     1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
    -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

    -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
    -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

    1.0, -1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
    1.0,  1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
    1.0,  1.0,  1.0,    1.0, 0.5, 0.0, 1.0,
    1.0, -1.0,  1.0,    1.0, 0.5, 0.0, 1.0,

    -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
    -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
     1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
     1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

    -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
    -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
     1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
     1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
};

static uint16_t cube_indices[] = {
    0, 1, 2,  0, 2, 3,
    6, 5, 4,  7, 6, 4,
    8, 9, 10,  8, 10, 11,
    14, 13, 12,  15, 14, 12,
    16, 17, 18,  16, 18, 19,
    22, 21, 20,  23, 22, 20
};

void init(void) {
    // setup sokol-gfx, sokol-imgui and sokol-gl
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    simgui_setup(&(simgui_desc_t){ 0 });
    sgl_setup(&(sgl_desc_t){ 0 });

    // default pass actions
    state.default_pass_action = (sg_pass_action) {
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.0f, 0.5f, 0.7f, 1.0f }
        }
    };

    // setup the sokol-gfx resources needed for the first user draw callback
    {
        state.scene1.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(cube_vertices),
            .label = "cube-vertices"
        });

        state.scene1.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .data = SG_RANGE(cube_indices),
            .label = "cube-indices"
        });

        state.scene1.pip = sg_make_pipeline(&(sg_pipeline_desc){
            .layout = {
                .attrs = {
                    [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
                    [ATTR_vs_color0].format   = SG_VERTEXFORMAT_FLOAT4
                }
            },
            .shader = sg_make_shader(scene_shader_desc(sg_query_backend())),
            .index_type = SG_INDEXTYPE_UINT16,
            .depth = {
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = true,
            },
            .cull_mode = SG_CULLMODE_BACK,
            .label = "cube-pipeline"
        });
    }

    // setup a sokol-gl pipeline needed for the second user draw callback
    {
        state.scene2.pip = sgl_make_pipeline(&(sg_pipeline_desc){
            .depth = {
                .write_enabled = true,
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
            },
            .cull_mode = SG_CULLMODE_BACK
        });
    }
}

// an ImGui draw callback to render directly with sokol-gfx
void draw_scene_1(const ImDrawList* dl, const ImDrawCmd* cmd) {
    (void)dl;

    // first set the viewport rectangle to render in, same as
    // the ImGui draw command's clip rect
    const int cx = (int) cmd->ClipRect.x;
    const int cy = (int) cmd->ClipRect.y;
    const int cw = (int) (cmd->ClipRect.z - cmd->ClipRect.x);
    const int ch = (int) (cmd->ClipRect.w - cmd->ClipRect.y);
    sg_apply_scissor_rect(cx, cy, cw, ch, true);
    sg_apply_viewport(cx, cy, 360, 360, true);

    // a model-view-proj matrix for the vertex shader
    const float t = (float)(sapp_frame_duration() * 60.0);
    vs_params_t vs_params;
    hmm_mat4 proj = HMM_Perspective(60.0f, 1.0f, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.scene1.rx += 1.0f * t; state.scene1.ry += 2.0f * t;
    hmm_mat4 rxm = HMM_Rotate(state.scene1.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.scene1.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    /*
        NOTE: we cannot start a separate render pass here since passes cannot
        be nested, so if we'd need to clear the color- or z-buffer we'd need to
        render a quad instead

        Another option is to render into a texture render target outside the
        ImGui user callback, and render this texture as quad inside the
        callback (or as a standard Image widget). This allows to perform rendering
        inside an (offscreen) render pass, and clear the background as usual.
    */
    sg_apply_pipeline(state.scene1.pip);
    sg_apply_bindings(&state.scene1.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
}

// helper function to draw a cube via sokol-gl
static void cube_sgl(void) {
    sgl_begin_quads();
    sgl_c3f(1.0f, 0.0f, 0.0f);
        sgl_v3f_t2f(-1.0f,  1.0f, -1.0f, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f,  1.0f, -1.0f,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0f, -1.0f, -1.0f,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
    sgl_c3f(0.0f, 1.0f, 0.0f);
        sgl_v3f_t2f(-1.0, -1.0,  1.0, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0, -1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0,  1.0,  1.0,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0,  1.0,  1.0, -1.0f, -1.0f);
    sgl_c3f(0.0f, 0.0f, 1.0f);
        sgl_v3f_t2f(-1.0, -1.0,  1.0, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0,  1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0,  1.0, -1.0,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0, -1.0, -1.0, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.5f, 0.0f);
        sgl_v3f_t2f(1.0, -1.0,  1.0, -1.0f,   1.0f);
        sgl_v3f_t2f(1.0, -1.0, -1.0,  1.0f,   1.0f);
        sgl_v3f_t2f(1.0,  1.0, -1.0,  1.0f,  -1.0f);
        sgl_v3f_t2f(1.0,  1.0,  1.0, -1.0f,  -1.0f);
    sgl_c3f(0.0f, 0.5f, 1.0f);
        sgl_v3f_t2f( 1.0, -1.0, -1.0, -1.0f,  1.0f);
        sgl_v3f_t2f( 1.0, -1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f(-1.0, -1.0,  1.0,  1.0f, -1.0f);
        sgl_v3f_t2f(-1.0, -1.0, -1.0, -1.0f, -1.0f);
    sgl_c3f(1.0f, 0.0f, 0.5f);
        sgl_v3f_t2f(-1.0,  1.0, -1.0, -1.0f,  1.0f);
        sgl_v3f_t2f(-1.0,  1.0,  1.0,  1.0f,  1.0f);
        sgl_v3f_t2f( 1.0,  1.0,  1.0,  1.0f, -1.0f);
        sgl_v3f_t2f( 1.0,  1.0, -1.0, -1.0f, -1.0f);
    sgl_end();
}

// another ImGui draw callback to render via sokol-gl
void draw_scene_2(const ImDrawList* dl, const ImDrawCmd* cmd) {
    (void)dl;
    const float t = (float)(sapp_frame_duration() * 60.0);

    const int cx = (int) cmd->ClipRect.x;
    const int cy = (int) cmd->ClipRect.y;
    const int cw = (int) (cmd->ClipRect.z - cmd->ClipRect.x);
    const int ch = (int) (cmd->ClipRect.w - cmd->ClipRect.y);
    sgl_scissor_rect(cx, cy, cw, ch, true);
    sgl_viewport(cx, cy, 360, 360, true);

    state.scene2.r0 += 1.0f * t;
    state.scene2.r1 += 2.0f * t;

    sgl_defaults();
    sgl_load_pipeline(state.scene2.pip);

    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(45.0f), 1.0f, 0.1f, 100.0f);

    sgl_matrix_mode_modelview();
    sgl_translate(0.0f, 0.0f, -12.0f);
    sgl_rotate(sgl_rad(state.scene2.r0), 1.0f, 0.0f, 0.0f);
    sgl_rotate(sgl_rad(state.scene2.r1), 0.0f, 1.0f, 0.0f);
    cube_sgl();
    sgl_push_matrix();
        sgl_translate(0.0f, 0.0f, 3.0f);
        sgl_scale(0.5f, 0.5f, 0.5f);
        sgl_rotate(-2.0f * sgl_rad(state.scene2.r0), 1.0f, 0.0f, 0.0f);
        sgl_rotate(-2.0f * sgl_rad(state.scene2.r1), 0.0f, 1.0f, 0.0f);
        cube_sgl();
        sgl_push_matrix();
            sgl_translate(0.0f, 0.0f, 3.0f);
            sgl_scale(0.5f, 0.5f, 0.5f);
            sgl_rotate(-3.0f * sgl_rad(2*state.scene2.r0), 1.0f, 0.0f, 0.0f);
            sgl_rotate(3.0f * sgl_rad(2*state.scene2.r1), 0.0f, 0.0f, 1.0f);
            cube_sgl();
        sgl_pop_matrix();
    sgl_pop_matrix();

    /*
        render the sokol-gl command list, this is the only call
        which actually needs to happen here in the callback,
        current downside is that only one such call must happen
        per frame
    */
    sgl_draw();
}

void frame(void) {

    // create the ImGui UI, a single window with two child views, each
    // rendering its own custom 3D scene via a user draw callback
    const int w = sapp_width();
    const int h = sapp_height();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = w,
        .height = h,
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });

    igSetNextWindowPos((ImVec2){20, 20}, ImGuiCond_Once, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){800, 400}, ImGuiCond_Once);
    if (igBegin("Dear ImGui", 0, 0)) {
        if (igBeginChild_Str("sokol-gfx", (ImVec2){360, 360}, true, ImGuiWindowFlags_None)) {
            ImDrawList* dl = igGetWindowDrawList();
            ImDrawList_AddCallback(dl, draw_scene_1, 0);
        }
        igEndChild();
        igSameLine(0, 10);
        if (igBeginChild_Str("sokol-gl", (ImVec2){360, 360}, true, ImGuiWindowFlags_None)) {
            ImDrawList* dl = igGetWindowDrawList();
            ImDrawList_AddCallback(dl, draw_scene_2, 0);
        }
        igEndChild();
    }
    igEnd();

    // actual UI rendering, the user draw callbacks are called from inside simgui_render()
    sg_begin_default_pass(&state.default_pass_action, w, h);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sgl_shutdown();
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event * ev) {
    simgui_handle_event(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 860,
        .height = 440,
        .gl_force_gles2 = true,
        .window_title = "imgui-usercallback",
        .icon.sokol_default = true,
    };
}
