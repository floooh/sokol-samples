//------------------------------------------------------------------------------
//  imgui-d3d11.cc
//  Dear ImGui integration sample with D3D11 backend.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "imgui.h"

static const int Width = 1024;
static const int Height = 768;
static const int MaxVertices = (1<<16);
static const int MaxIndices = MaxVertices * 3;

static uint64_t last_time = 0;
static bool show_test_window = true;
static bool show_another_window = false;

static sg_pipeline pip;
static sg_bindings bind;
static sg_pass_action pass_action;

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

static void draw_imgui(ImDrawData*);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper, sokol_gfx, sokol_time
    d3d11_init(Width, Height, 1, L"Sokol Dear ImGui D3D11");
    sg_desc desc = { };
    desc.context = d3d11_get_context();
    sg_setup(&desc);
    stm_setup();

    // input forwarding
    d3d11_mouse_pos([] (float x, float y)   { ImGui::GetIO().MousePos = ImVec2(x, y); });
    d3d11_mouse_btn_down([] (int btn)       { ImGui::GetIO().MouseDown[btn] = true; });
    d3d11_mouse_btn_up([] (int btn)         { ImGui::GetIO().MouseDown[btn] = false; });
    d3d11_mouse_wheel([](float v)           { ImGui::GetIO().MouseWheel = v; });
    d3d11_char([] (wchar_t c)               { ImGui::GetIO().AddInputCharacter(c); });
    d3d11_key_down([] (int key)             { if (key < 512) ImGui::GetIO().KeysDown[key] = true; });
    d3d11_key_up([] (int key)               { if (key < 512) ImGui::GetIO().KeysDown[key] = false; });

    // setup Dear Imgui
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    // dynamic vertex- and index-buffers for imgui-generated geometry
    sg_buffer_desc vbuf_desc = { };
    vbuf_desc.usage = SG_USAGE_STREAM;
    vbuf_desc.size = MaxVertices * sizeof(ImDrawVert);
    bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    sg_buffer_desc ibuf_desc = { };
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.usage = SG_USAGE_STREAM;
    ibuf_desc.size = MaxIndices * sizeof(ImDrawIdx);
    bind.index_buffer = sg_make_buffer(&ibuf_desc);

    // font texture for imgui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = { };
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.content.subimage[0][0].ptr = font_pixels;
    img_desc.content.subimage[0][0].size = font_width * font_height * 4;
    bind.fs_images[0] = sg_make_image(&img_desc);

    // shader object for imgui rendering
    sg_shader_desc shd_desc = { };
    auto& ub = shd_desc.vs.uniform_blocks[0];
    ub.size = sizeof(vs_params_t);
    shd_desc.attrs[0].sem_name = "POSITION";
    shd_desc.attrs[1].sem_name = "TEXCOORD";
    shd_desc.attrs[2].sem_name = "COLOR";
    shd_desc.vs.source =
        "cbuffer params {\n"
        "  float2 disp_size;\n"
        "};\n"
        "struct vs_in {\n"
        "  float2 pos: POSITION;\n"
        "  float2 uv: TEXCOORD0;\n"
        "  float4 color: COLOR0;\n"
        "};\n"
        "struct vs_out {\n"
        "  float2 uv: TEXCOORD0;\n"
        "  float4 color: COLOR0;\n"
        "  float4 pos: SV_Position;\n"
        "};\n"
        "vs_out main(vs_in inp) {\n"
        "  vs_out outp;\n"
        "  outp.pos = float4(((inp.pos/disp_size)-0.5)*float2(2.0,-2.0), 0.5, 1.0);\n"
        "  outp.uv = inp.uv;\n"
        "  outp.color = inp.color;\n"
        "  return outp;\n"
        "}\n";
    shd_desc.fs.images[0].type = SG_IMAGETYPE_2D;
    shd_desc.fs.source =
        "Texture2D<float4> tex: register(t0);\n"
        "sampler smp: register(s0);\n"
        "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
        "  return tex.Sample(smp, uv) * color;\n"
        "}\n";
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = { };
    pip_desc.layout.buffers[0].stride = sizeof(ImDrawVert);
    auto& attrs = pip_desc.layout.attrs;
    attrs[0].offset=offsetof(ImDrawVert, pos); attrs[0].format=SG_VERTEXFORMAT_FLOAT2;
    attrs[1].offset=offsetof(ImDrawVert, uv); attrs[1].format=SG_VERTEXFORMAT_FLOAT2;
    attrs[2].offset=offsetof(ImDrawVert, col); attrs[2].format=SG_VERTEXFORMAT_UBYTE4N;
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.blend.enabled = true;
    pip_desc.blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.blend.color_write_mask = SG_COLORMASK_RGB;
    pip = sg_make_pipeline(&pip_desc);

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 0.0f;
    pass_action.colors[0].val[1] = 0.5f;
    pass_action.colors[0].val[2] = 0.7f;
    pass_action.colors[0].val[3] = 1.0f;

    // draw loop
    while (d3d11_process_events()) {
        const int cur_width = d3d11_width();
        const int cur_height = d3d11_height();

        // this is standard ImGui demo code
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(float(cur_width), float(cur_height));
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
        sg_begin_default_pass(&pass_action, cur_width, cur_height);
        ImGui::Render();
        draw_imgui(ImGui::GetDrawData());
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    ImGui::DestroyContext();
    sg_shutdown();
    d3d11_shutdown();
}

// render ImGui draw lists through sokol-gfx
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
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    for (int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];

        // append vertices and indices to buffers, record start offsets in bindings struct
        const int vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
        const int idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
        const int vb_offset = sg_append_buffer(bind.vertex_buffers[0], &cl->VtxBuffer.front(), vtx_size);
        const int ib_offset = sg_append_buffer(bind.index_buffer, &cl->IdxBuffer.front(), idx_size);
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
