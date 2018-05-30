//------------------------------------------------------------------------------
//  imgui-emsc
//  Demonstrates basic integration with Dear Imgui (without custom
//  texture or custom font support).
//  Since emscripten is using clang exclusively, we can use designated
//  initializers even though this is C++.
//------------------------------------------------------------------------------
#include "imgui.h"
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "emsc.h"

const int MaxVertices = (1<<16);
const int MaxIndices = MaxVertices * 3;

uint64_t last_time = 0;
bool show_test_window = true;
bool show_another_window = false;

sg_draw_state draw_state = { };
sg_pass_action pass_action = { };
bool btn_down[3] = { };
bool btn_up[3] = { };
ImDrawVert vertices[MaxVertices];
uint16_t indices[MaxIndices];

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

void draw();
void imgui_draw_cb(ImDrawData*);

int main() {
    /* setup WebGL context */
    emsc_init("#canvas", EMSC_NONE);

    /* setup sokol_gfx and sokol_time */
    stm_setup();
    sg_desc desc = { };
    sg_setup(&desc);
    assert(sg_isvalid());
    
    // setup the ImGui environment
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.RenderDrawListsFn = imgui_draw_cb;
    io.Fonts->AddFontDefault();
    // emscripten has no clearly defined key code constants
    io.KeyMap[ImGuiKey_Tab] = 9;
    io.KeyMap[ImGuiKey_LeftArrow] = 37;
    io.KeyMap[ImGuiKey_RightArrow] = 39;
    io.KeyMap[ImGuiKey_UpArrow] = 38;
    io.KeyMap[ImGuiKey_DownArrow] = 40;
    io.KeyMap[ImGuiKey_Home] = 36;
    io.KeyMap[ImGuiKey_End] = 35;
    io.KeyMap[ImGuiKey_Delete] = 46;
    io.KeyMap[ImGuiKey_Backspace] = 8;
    io.KeyMap[ImGuiKey_Enter] = 13;
    io.KeyMap[ImGuiKey_Escape] = 27;
    io.KeyMap[ImGuiKey_A] = 65;
    io.KeyMap[ImGuiKey_C] = 67;
    io.KeyMap[ImGuiKey_V] = 86;
    io.KeyMap[ImGuiKey_X] = 88;
    io.KeyMap[ImGuiKey_Y] = 89;
    io.KeyMap[ImGuiKey_Z] = 90;

    // emscripten to ImGui input forwarding
    emscripten_set_keydown_callback(0, nullptr, true, 
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            if (e->keyCode < 512) {
                ImGui::GetIO().KeysDown[e->keyCode] = true;
            }
            // only forward alpha-numeric keys to browser
            return e->keyCode < 32;
        });
    emscripten_set_keyup_callback(0, nullptr, true, 
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            if (e->keyCode < 512) {
                ImGui::GetIO().KeysDown[e->keyCode] = false;
            }
            // only forward alpha-numeric keys to browser
            return e->keyCode < 32;
        });
    emscripten_set_keypress_callback(0, nullptr, true,
        [](int, const EmscriptenKeyboardEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().AddInputCharacter((ImWchar)e->charCode);
            return true;
        });
    emscripten_set_mousedown_callback("#canvas", nullptr, true, 
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            switch (e->button) {
                case 0: btn_down[0] = true; break;
                case 2: btn_down[1] = true; break;
            }
            return true;
        });
    emscripten_set_mouseup_callback("#canvas", nullptr, true, 
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            switch (e->button) {
                case 0: btn_up[0] = true; break;
                case 2: btn_up[1] = true; break;
            }
            return true;
        });
    emscripten_set_mouseenter_callback("#canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            auto& io = ImGui::GetIO();
            for (int i = 0; i < 3; i++) {
                btn_down[i] = btn_up[i] = false;
                io.MouseDown[i] = false;
            }
            return true;
        });
    emscripten_set_mouseleave_callback("#canvas", nullptr, true,
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            auto& io = ImGui::GetIO();
            for (int i = 0; i < 3; i++) {
                btn_down[i] = btn_up[i] = false;
                io.MouseDown[i] = false;
            }
            return true;
        });
    emscripten_set_mousemove_callback("#canvas", nullptr, true, 
        [](int, const EmscriptenMouseEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().MousePos.x = (float) e->canvasX;
            ImGui::GetIO().MousePos.y = (float) e->canvasY;
            return true;
        });
    emscripten_set_wheel_callback("#canvas", nullptr, true, 
        [](int, const EmscriptenWheelEvent* e, void*)->EM_BOOL {
            ImGui::GetIO().MouseWheelH = -0.1f * (float)e->deltaX;
            ImGui::GetIO().MouseWheel = -0.1f * (float)e->deltaY;
            return true;
        });

    // dynamic vertex- and index-buffers for imgui-generated geometry
    sg_buffer_desc vbuf_desc = {
        .usage = SG_USAGE_STREAM,
        .size = sizeof(vertices)
    };
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(indices)
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
    draw_state.index_buffer = sg_make_buffer(&ibuf_desc);

    // font texture for imgui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = {
        .width = font_width,
        .height = font_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .content.subimage[0][0] = {
            .ptr = font_pixels,
            .size = font_width * font_height * 4
        }
    };
    draw_state.fs_images[0] = sg_make_image(&img_desc);

    // shader object for imgui rendering
    sg_shader_desc shd_desc = {
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="disp_size", .type=SG_UNIFORMTYPE_FLOAT2}
            }
        },
        .vs.source =
            "uniform vec2 disp_size;\n"
            "attribute vec2 position;\n"
            "attribute vec2 texcoord0;\n"
            "attribute vec4 color0;\n"
            "varying vec2 uv;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "    gl_Position = vec4(((position/disp_size)-0.5)*vec2(2.0,-2.0), 0.5, 1.0);\n"
            "    uv = texcoord0;\n"
            "    color = color0;\n"
            "}\n",
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        .fs.source =
            "precision mediump float;"
            "uniform sampler2D tex;\n"
            "varying vec2 uv;\n"
            "varying vec4 color;\n"
            "void main() {\n"
            "    gl_FragColor = texture2D(tex, uv) * color;\n"
            "}\n"
    };
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = {
        .layout = {
            .buffers[0].stride = sizeof(ImDrawVert),
            .attrs = {
                [0] = { .name="position", .offset=offsetof(ImDrawVert,pos), .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .name="texcoord0", .offset=offsetof(ImDrawVert,uv), .format=SG_VERTEXFORMAT_FLOAT2 },
                [2] = { .name="color0", .offset=offsetof(ImDrawVert,col), .format=SG_VERTEXFORMAT_UBYTE4N }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_write_mask = SG_COLORMASK_RGB
        }
    };
    draw_state.pipeline = sg_make_pipeline(&pip_desc);

    // initial clear color
    pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };

    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

// the main draw loop, this draw the standard ImGui demo windows
void draw() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(emsc_width()), float(emsc_height()));
    io.DeltaTime = (float) stm_sec(stm_laptime(&last_time));
    // this mouse button handling fixes the problem when down- and up-events 
    // happen in the same frame
    for (int i = 0; i < 3; i++) {
        if (io.MouseDown[i]) {
            if (btn_up[i]) {
                io.MouseDown[i] = false;
                btn_up[i] = false;
            }
        }
        else {
            if (btn_down[i]) {
                io.MouseDown[i] = true;
                btn_down[i] = false;
            }
        }
    }
    ImGui::NewFrame();

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].val[0]);
    if (ImGui::Button("Test Window")) show_test_window ^= 1;
    if (ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (show_another_window) {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
    if (show_test_window) {
        ImGui::SetNextWindowPos(ImVec2(460, 20), ImGuiSetCond_FirstUseEver);
        ImGui::ShowTestWindow();
    }

    // the sokol_gfx draw pass
    sg_begin_default_pass(&pass_action, emsc_width(), emsc_height());
    ImGui::Render();
    sg_end_pass();
    sg_commit();
}

// imgui draw callback to render imgui-generated draw commands through sokol_gfx
void imgui_draw_cb(ImDrawData* draw_data) {
    assert(draw_data);
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    // copy vertices and indices
    int num_vertices = 0;
    int num_indices = 0;
    int num_cmdlists = 0;
    for (num_cmdlists = 0; num_cmdlists < draw_data->CmdListsCount; num_cmdlists++) {
        const ImDrawList* cl = draw_data->CmdLists[num_cmdlists];
        const int cl_num_vertices = cl->VtxBuffer.size();
        const int cl_num_indices = cl->IdxBuffer.size();

        // overflow check
        if ((num_vertices + cl_num_vertices) > MaxVertices) {
            break;
        }
        if ((num_indices + cl_num_indices) > MaxIndices) {
            break;
        }

        // copy vertices
        memcpy(&vertices[num_vertices], &cl->VtxBuffer.front(), cl_num_vertices*sizeof(ImDrawVert));

        // copy indices, need to 'rebase' indices to start of global vertex buffer
        const ImDrawIdx* src_index_ptr = &cl->IdxBuffer.front();
        const uint16_t base_vertex_index = num_vertices;
        for (int i = 0; i < cl_num_indices; i++) {
            indices[num_indices++] = src_index_ptr[i] + base_vertex_index;
        }
        num_vertices += cl_num_vertices;
    }

    // update vertex and index buffers
    const int vertex_data_size = num_vertices * sizeof(ImDrawVert);
    const int index_data_size = num_indices * sizeof(uint16_t);
    sg_update_buffer(draw_state.vertex_buffers[0], vertices, vertex_data_size);
    sg_update_buffer(draw_state.index_buffer, indices, index_data_size);

    // render the command list
    vs_params_t vs_params;
    vs_params.disp_size = ImGui::GetIO().DisplaySize;
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    int base_element = 0;
    for (int cl_index=0; cl_index<num_cmdlists; cl_index++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[cl_index];
        for (const ImDrawCmd& pcmd : cmd_list->CmdBuffer) {
            if (pcmd.UserCallback) {
                pcmd.UserCallback(cmd_list, &pcmd);
            }
            else {
                const int sx = (int) pcmd.ClipRect.x;
                const int sy = (int) pcmd.ClipRect.y;
                const int sw = (int) (pcmd.ClipRect.z - pcmd.ClipRect.x);
                const int sh = (int) (pcmd.ClipRect.w - pcmd.ClipRect.y);
                sg_apply_scissor_rect(sx, sy, sw, sh, true);
                sg_draw(base_element, pcmd.ElemCount, 1);
            }
            base_element += pcmd.ElemCount;
        }
    }
}

