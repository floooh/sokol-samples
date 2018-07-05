//------------------------------------------------------------------------------
//  imgui-metal.c
//  Since this will only need to compile with clang we can use
//  mix designated initializers and C++ code.
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "imgui.h"

const int WIDTH = 1024;
const int HEIGHT = 768;
const int MaxVertices = (1<<16);
const int MaxIndices = MaxVertices * 3;

uint64_t last_time = 0;
bool show_test_window = true;
bool show_another_window = false;

sg_draw_state draw_state = { };
sg_pass_action pass_action = { };
ImDrawVert vertices[MaxVertices];
uint16_t indices[MaxIndices];

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

void imgui_draw_cb(ImDrawData*);

void init(const void* mtl_device) {
    // setup sokol_gfx and sokol_time
    sg_desc desc = {
        .mtl_device = mtl_device,
        .mtl_renderpass_descriptor_cb = osx_mtk_get_render_pass_descriptor,
        .mtl_drawable_cb = osx_mtk_get_drawable
    };
    sg_setup(&desc);
    stm_setup();

    // setup the imgui environment
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.RenderDrawListsFn = imgui_draw_cb;
    io.Fonts->AddFontDefault();
    io.KeyMap[ImGuiKey_Tab] = 0x30;
    io.KeyMap[ImGuiKey_LeftArrow] = 0x7B;
    io.KeyMap[ImGuiKey_RightArrow] = 0x7C;
    io.KeyMap[ImGuiKey_DownArrow] = 0x7D;
    io.KeyMap[ImGuiKey_UpArrow] = 0x7E;
    io.KeyMap[ImGuiKey_Home] = 0x73;
    io.KeyMap[ImGuiKey_End] = 0x77;
    io.KeyMap[ImGuiKey_Delete] = 0x75;
    io.KeyMap[ImGuiKey_Backspace] = 0x33;
    io.KeyMap[ImGuiKey_Enter] = 0x24;
    io.KeyMap[ImGuiKey_Escape] = 0x35;
    io.KeyMap[ImGuiKey_A] = 0x00;
    io.KeyMap[ImGuiKey_C] = 0x08;
    io.KeyMap[ImGuiKey_V] = 0x09;
    io.KeyMap[ImGuiKey_X] = 0x07;
    io.KeyMap[ImGuiKey_Y] = 0x10;
    io.KeyMap[ImGuiKey_Z] = 0x06;

    // OSX => ImGui input forwarding
    osx_mouse_pos([] (float x, float y) { ImGui::GetIO().MousePos = ImVec2(x, y); });
    osx_mouse_btn_down([] (int btn)     { ImGui::GetIO().MouseDown[btn] = true; });
    osx_mouse_btn_up([] (int btn)       { ImGui::GetIO().MouseDown[btn] = false; });
    osx_mouse_wheel([] (float v)        { ImGui::GetIO().MouseWheel = 0.25f * v; });
    osx_key_down([] (int key)           { if (key < 512) ImGui::GetIO().KeysDown[key] = true; });
    osx_key_up([] (int key)             { if (key < 512) ImGui::GetIO().KeysDown[key] = false; });
    osx_char([] (wchar_t c)             { ImGui::GetIO().AddInputCharacter(c); });

    // dynamic vertex- and index-buffers for ImGui-generated geometry
    sg_buffer_desc vbuf_desc = {
        .usage = SG_USAGE_STREAM,
        .size = sizeof(vertices)
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(indices)
    };
    draw_state.index_buffer = sg_make_buffer(&ibuf_desc);

    // font texture for ImGui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = {
        .width = font_width,
        .height = font_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .content.subimage[0][0] = {
            .ptr = font_pixels,
            .size = font_width * font_height * 4
        }
    };
    draw_state.fs_images[0] = sg_make_image(&img_desc);

    // shader object for imgui renering
    sg_shader_desc shd_desc = {
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float2 disp_size;\n"
            "};\n"
            "struct vs_in {\n"
            "  float2 pos [[attribute(0)]];\n"
            "  float2 uv [[attribute(1)]];\n"
            "  float4 color [[attribute(2)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float2 uv;\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  out.pos = float4(((in.pos / params.disp_size)-0.5)*float2(2.0,-2.0), 0.5, 1.0);\n"
            "  out.uv = in.uv;\n"
            "  out.color = in.color;\n"
            "  return out;\n"
            "}\n",
        .fs.images[0].type = SG_IMAGETYPE_2D,
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct fs_in {\n"
            "  float2 uv;\n"
            "  float4 color;\n"
            "};\n"
            "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
            "  return tex.sample(smp, in.uv) * in.color;\n"
            "}\n"
    };
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = {
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
}

void frame() {
    const int width = osx_width();
    const int height = osx_height();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);
    io.DeltaTime = (float) stm_sec(stm_laptime(&last_time));
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

    // the sokol draw pass
    sg_begin_default_pass(&pass_action, width, height);
    ImGui::Render();
    sg_end_pass();
    sg_commit();
}

void shutdown() {
    ImGui::DestroyContext();
    sg_shutdown();
}

int main() {
    osx_start(WIDTH, HEIGHT, 1, "Sokol Dear ImGui (Metal)", init, frame, shutdown);
    return 0;
}

void imgui_draw_cb(ImDrawData* draw_data) {
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    // copy vertices and indices
    int num_vertices = 0;
    int num_indices = 0;
    int num_cmdlists = 0;
    for (; num_cmdlists < draw_data->CmdListsCount; num_cmdlists++) {
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

        // copy indices, need to rebase to start of global vertex buffer
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
    for (int cl_index = 0; cl_index < num_cmdlists; cl_index++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[cl_index];
        for (const ImDrawCmd& cmd : cmd_list->CmdBuffer) {
            if (cmd.UserCallback) {
                cmd.UserCallback(cmd_list, &cmd);
            }
            else {
                const int sx = (int) cmd.ClipRect.x;
                const int sy = (int) cmd.ClipRect.y;
                const int sw = (int) (cmd.ClipRect.z - cmd.ClipRect.x);
                const int sh = (int) (cmd.ClipRect.w - cmd.ClipRect.y);
                sg_apply_scissor_rect(sx, sy, sw, sh, true);
                sg_draw(base_element, cmd.ElemCount, 1);
            }
            base_element += cmd.ElemCount;
        }
    }
}

