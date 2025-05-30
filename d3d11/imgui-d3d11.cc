//------------------------------------------------------------------------------
//  imgui-d3d11.cc
//  Dear ImGui integration sample with D3D11 backend.
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"
#include "sokol_log.h"
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
static ImGuiKey as_imgui_key(int keycode);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d11 app wrapper, sokol_gfx, sokol_time
    d3d11_desc_t d3d11_desc = {};
    d3d11_desc.width = Width;
    d3d11_desc.height = Height;
    d3d11_desc.title = L"imgui-d3d11.c";
    d3d11_init(&d3d11_desc);
    sg_desc desc = { };
    desc.environment = d3d11_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);
    stm_setup();

    // input forwarding
    d3d11_mouse_pos([] (float x, float y)   { ImGui::GetIO().AddMousePosEvent(x, y); });
    d3d11_mouse_btn_down([] (int btn)       { ImGui::GetIO().AddMouseButtonEvent(btn, true); });
    d3d11_mouse_btn_up([] (int btn)         { ImGui::GetIO().AddMouseButtonEvent(btn, false); });
    d3d11_mouse_wheel([](float v)           { ImGui::GetIO().AddMouseWheelEvent(0, v); });
    d3d11_char([] (wchar_t c)               { ImGui::GetIO().AddInputCharacter(c); });
    d3d11_key_down([] (int key)             { ImGui::GetIO().AddKeyEvent(as_imgui_key(key), true); });
    d3d11_key_up([] (int key)               { ImGui::GetIO().AddKeyEvent(as_imgui_key(key), false); });

    // setup Dear Imgui
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();

    // dynamic vertex- and index-buffers for imgui-generated geometry
    sg_buffer_desc vbuf_desc = { };
    vbuf_desc.usage.stream_update = true;
    vbuf_desc.size = MaxVertices * sizeof(ImDrawVert);
    bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    sg_buffer_desc ibuf_desc = { };
    ibuf_desc.usage.index_buffer = true;
    ibuf_desc.usage.stream_update = true;
    ibuf_desc.size = MaxIndices * sizeof(ImDrawIdx);
    bind.index_buffer = sg_make_buffer(&ibuf_desc);

    // font texture and sampler for imgui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = { };
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.data.subimage[0][0].ptr = font_pixels;
    img_desc.data.subimage[0][0].size = font_width * font_height * 4;
    bind.images[0] = sg_make_image(&img_desc);
    sg_sampler_desc smp_desc = { };
    smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    bind.samplers[0] = sg_make_sampler(&smp_desc);

    // shader object for imgui rendering
    sg_shader_desc shd_desc = { };
    shd_desc.vertex_func.source =
        "cbuffer params: register(b0) {\n"
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
    shd_desc.fragment_func.source =
        "Texture2D<float4> tex: register(t0);\n"
        "sampler smp: register(s0);\n"
        "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
        "  return tex.Sample(smp, uv) * color;\n"
        "}\n";
    shd_desc.attrs[0].hlsl_sem_name = "POSITION";
    shd_desc.attrs[1].hlsl_sem_name = "TEXCOORD";
    shd_desc.attrs[2].hlsl_sem_name = "COLOR";
    shd_desc.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    shd_desc.uniform_blocks[0].size = sizeof(vs_params_t);
    shd_desc.uniform_blocks[0].hlsl_register_b_n = 0;
    shd_desc.images[0].stage = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.images[0].hlsl_register_t_n = 0,
    shd_desc.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.samplers[0].hlsl_register_s_n = 0;
    shd_desc.image_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
    shd_desc.image_sampler_pairs[0].image_slot = 0;
    shd_desc.image_sampler_pairs[0].sampler_slot = 0;
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = { };
    pip_desc.layout.buffers[0].stride = sizeof(ImDrawVert);
    auto& attrs = pip_desc.layout.attrs;
    attrs[0].offset = offsetof(ImDrawVert, pos); attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    attrs[1].offset = offsetof(ImDrawVert, uv);  attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    attrs[2].offset = offsetof(ImDrawVert, col); attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.colors[0].blend.enabled = true;
    pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].write_mask = SG_COLORMASK_RGB;
    pip = sg_make_pipeline(&pip_desc);

    // initial clear color
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = { 0.0f, 0.5f, 0.7f, 1.0f };

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
        sg_pass pass = { };
        pass.action = pass_action;
        pass.swapchain = d3d11_swapchain();
        sg_begin_pass(&pass);
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
    sg_apply_uniforms(0, SG_RANGE(vs_params));
    for (int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];

        // append vertices and indices to buffers, record start offsets in bindings struct
        const uint32_t vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
        const uint32_t idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
        const uint32_t vb_offset = sg_append_buffer(bind.vertex_buffers[0], { &cl->VtxBuffer.front(), vtx_size });
        const uint32_t ib_offset = sg_append_buffer(bind.index_buffer, { &cl->IdxBuffer.front(), idx_size });
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
            } else {
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

static ImGuiKey as_imgui_key(int keycode) {
    switch (keycode) {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    }
}