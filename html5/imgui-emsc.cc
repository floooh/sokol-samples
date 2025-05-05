//------------------------------------------------------------------------------
//  imgui-emsc
//  Demonstrates basic integration with Dear Imgui (without custom
//  texture or custom font support).
//  Since emscripten is using clang exclusively, we can use designated
//  initializers even though this is C++.
//------------------------------------------------------------------------------
#include "imgui.h"
#define SOKOL_IMPL
#define SOKOL_GLES3
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "emsc.h"

// these are fairly recent warnings in clang
#pragma clang diagnostic ignored "-Wc99-designator"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wreorder-init-list"

static const int MaxVertices = (1<<16);
static const int MaxIndices = MaxVertices * 3;

static uint64_t last_time = 0;
static bool show_test_window = true;
static bool show_another_window = false;

static sg_pass_action pass_action;
static sg_pipeline pip;
static sg_bindings bind;

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

static EM_BOOL draw(double time, void* userdata);
static void draw_imgui(ImDrawData*);
static ImGuiKey as_imgui_key(int keycode);

int main() {
    // setup WebGL context
    emsc_init("#canvas", EMSC_NONE);

    // setup sokol_gfx and sokol_time
    stm_setup();
    sg_setup(sg_desc{
        .environment = emsc_environment(),
        .logger = {
            .func = slog_func
        }
    });
    assert(sg_isvalid());

    // setup the ImGui environment
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();

    // emscripten to ImGui input forwarding
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddKeyEvent(as_imgui_key(e->keyCode), true);
            // only forward alpha-numeric keys to browser
            return e->keyCode < 32;
        });
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            if (e->keyCode < 512) {
                ImGui::GetIO().AddKeyEvent(as_imgui_key(e->keyCode), false);
            }
            // only forward alpha-numeric keys to browser
            return e->keyCode < 32;
        });
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddInputCharacter((ImWchar)e->charCode);
            return true;
        });
    emscripten_set_mousedown_callback("canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMouseButtonEvent(e->button, true);
            return true;
        });
    emscripten_set_mouseup_callback("canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMouseButtonEvent(e->button, false);
            return true;
        });
    emscripten_set_mousemove_callback("canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMousePosEvent((float)e->targetX, (float)e->targetY);
            return true;
        });
    emscripten_set_wheel_callback("canvas", nullptr, true,
        [](int, const EmscriptenWheelEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddMouseWheelEvent(-0.1f * (float)e->deltaX, -0.1f * (float)e->deltaY);
            return true;
        });

    // dynamic vertex- and index-buffers for imgui-generated geometry
    bind.vertex_buffers[0] = sg_make_buffer({
        .usage.stream_update = true,
        .size = MaxVertices * sizeof(ImDrawVert)
    });
    bind.index_buffer = sg_make_buffer({
        .usage = {
            .index_buffer = true,
            .stream_update = true,
        },
        .size = MaxIndices * sizeof(ImDrawIdx)
    });

    // font texture and sampler for imgui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    bind.images[0] = sg_make_image({
        .width = font_width,
        .height = font_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] = {
            .ptr = font_pixels,
            .size = size_t(font_width * font_height * 4)
        }
    });
    bind.samplers[0] = sg_make_sampler({
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    });

    // shader object for imgui rendering
    sg_shader shd = sg_make_shader({
        .vertex_func.source =
            "#version 300 es\n"
            "uniform vec2 disp_size;\n"
            "in vec2 position;\n"
            "in vec2 texcoord0;\n"
            "in vec4 color0;\n"
            "out vec2 uv;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "    gl_Position = vec4(((position/disp_size)-0.5)*vec2(2.0,-2.0), 0.5, 1.0);\n"
            "    uv = texcoord0;\n"
            "    color = color0;\n"
            "}\n",
        .fragment_func.source =
            "#version 300 es\n"
            "precision mediump float;"
            "uniform sampler2D tex;\n"
            "in vec2 uv;\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "    frag_color = texture(tex, uv) * color;\n"
            "}\n",
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "texcoord0",
            [2].glsl_name = "color0"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(vs_params_t),
            .glsl_uniforms = {
                [0] = { .glsl_name = "disp_size", .type = SG_UNIFORMTYPE_FLOAT2}
            }
        },
        .images[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .image_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .glsl_name = "tex", .image_slot = 0, .sampler_slot = 0 },
    });

    // pipeline object for imgui rendering
    pip = sg_make_pipeline({
        .layout = {
            .buffers[0].stride = sizeof(ImDrawVert),
            .attrs = {
                [0] = { .offset=offsetof(ImDrawVert,pos), .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .offset=offsetof(ImDrawVert,uv), .format=SG_VERTEXFORMAT_FLOAT2 },
                [2] = { .offset=offsetof(ImDrawVert,col), .format=SG_VERTEXFORMAT_UBYTE4N }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .colors[0] = {
            .write_mask = SG_COLORMASK_RGB,
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            }
        }
    });

    // initial clear color
    pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };

    emscripten_request_animation_frame_loop(draw, 0);
    return 0;
}

// the main draw loop, this draw the standard ImGui demo windows
static EM_BOOL draw(double time, void* userdata) {
    (void)time; (void)userdata;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(emsc_width()), float(emsc_height()));
    io.DeltaTime = (float) stm_sec(stm_laptime(&last_time));
    ImGui::NewFrame();

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].clear_value.r);
    if (ImGui::Button("Test Window")) show_test_window ^= 1;
    if (ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (show_another_window) {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiCond_FirstUseEver);
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowDemoWindow()
    if (show_test_window) {
        ImGui::SetNextWindowPos(ImVec2(460, 20), ImGuiCond_FirstUseEver);
        ImGui::ShowDemoWindow();
    }

    // the sokol_gfx draw pass
    sg_begin_pass({ .action = pass_action, .swapchain = emsc_swapchain() });
    ImGui::Render();
    draw_imgui(ImGui::GetDrawData());
    sg_end_pass();
    sg_commit();
    return EM_TRUE;
}

// imgui draw callback
void draw_imgui(ImDrawData* draw_data) {
    assert(draw_data);
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    // render the command list
    vs_params_t vs_params;
    vs_params.disp_size.x = ImGui::GetIO().DisplaySize.x;
    vs_params.disp_size.y = ImGui::GetIO().DisplaySize.y;
    sg_apply_pipeline(pip);
    sg_apply_uniforms(0, SG_RANGE(vs_params));
    for (int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];

        // append vertices and indices to buffers, record start offsets in binding struct
        const size_t vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
        const size_t idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
        const int vb_offset = sg_append_buffer(bind.vertex_buffers[0], { &cl->VtxBuffer.front(), vtx_size });
        const int ib_offset = sg_append_buffer(bind.index_buffer, { &cl->IdxBuffer.front(), idx_size });
        /* don't render anything if the buffer is in overflow state (this is also
            checked internally in sokol_gfx, draw calls that attempt from
            overflowed buffers will be silently dropped)
        */
        if (sg_query_buffer_overflow(bind.vertex_buffers[0]) ||
            sg_query_buffer_overflow(bind.index_buffer))
        {
            continue;
        }

        bind.vertex_buffer_offsets[0] = vb_offset;
        bind.index_buffer_offset = ib_offset;
        sg_apply_bindings(&bind);

        int base_element = 0;
        for (const ImDrawCmd& pcmd : cl->CmdBuffer) {
            if (pcmd.UserCallback) {
                pcmd.UserCallback(cl, &pcmd);
            }
            else {
                const int scissor_x = (int) (pcmd.ClipRect.x);
                const int scissor_y = (int) (pcmd.ClipRect.y);
                const int scissor_w = (int) (pcmd.ClipRect.z - pcmd.ClipRect.x);
                const int scissor_h = (int) (pcmd.ClipRect.w - pcmd.ClipRect.y);
                sg_apply_scissor_rect(scissor_x, scissor_y, scissor_w, scissor_h, true);
                sg_draw(base_element, pcmd.ElemCount, 1);
            }
            base_element += pcmd.ElemCount;
        }
    }
}

ImGuiKey as_imgui_key(int keycode) {
    switch (keycode) {
        case 9: return ImGuiKey_Tab;
        case 37: return ImGuiKey_LeftArrow;
        case 39: return ImGuiKey_RightArrow;
        case 38: return ImGuiKey_UpArrow;
        case 40: return ImGuiKey_DownArrow;
        case 36: return ImGuiKey_Home;
        case 35: return ImGuiKey_End;
        case 46: return ImGuiKey_Delete;
        case 8: return ImGuiKey_Backspace;
        case 13: return ImGuiKey_Enter;
        case 27: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    };
}
