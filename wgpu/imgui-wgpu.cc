//------------------------------------------------------------------------------
//  imgui-wgpu.c
//
//  Test ImGui rendering, no input handling!
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "imgui.h"

#if !defined(__EMSCRIPTEN__)
#include "GLFW/glfw3.h"
#endif

static const size_t MaxVertices = 64 * 1024;
static const size_t MaxIndices = MaxVertices * 3;
static const int Width = 1024;
static const int Height = 768;

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    uint64_t last_time = 0;
    bool show_test_window = true;
    bool show_another_window = false;
} state;

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

static void draw_imgui(ImDrawData*);

static void init(void) {
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_desc desc = { };
    desc.environment = wgpu_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);
    stm_setup();

    // input forwarding
    wgpu_mouse_pos([] (float x, float y) { ImGui::GetIO().MousePos = ImVec2(x, y); });
    wgpu_mouse_btn_down([] (int btn)     { ImGui::GetIO().MouseDown[btn] = true; });
    wgpu_mouse_btn_up([] (int btn)       { ImGui::GetIO().MouseDown[btn] = false; });
    wgpu_mouse_wheel([](float v)         { ImGui::GetIO().MouseWheel = v; });
    wgpu_char([] (uint32_t c)            { ImGui::GetIO().AddInputCharacter(c); });
    wgpu_key_down([] (int key)           { if (key < 512) ImGui::GetIO().KeysDown[key] = true; });
    wgpu_key_up([] (int key)             { if (key < 512) ImGui::GetIO().KeysDown[key] = false; });

    // setup Dear Imgui
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    io.KeyMap[ImGuiKey_Tab] = WGPU_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = WGPU_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = WGPU_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = WGPU_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = WGPU_KEY_DOWN;
    io.KeyMap[ImGuiKey_Home] = WGPU_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = WGPU_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = WGPU_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = WGPU_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = WGPU_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = WGPU_KEY_ESCAPE;
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
    state.bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    sg_buffer_desc ibuf_desc = { };
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.usage = SG_USAGE_STREAM;
    ibuf_desc.size = MaxIndices * sizeof(ImDrawIdx);
    state.bind.index_buffer = sg_make_buffer(&ibuf_desc);

    // font texture and sampler for imgui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

    sg_image_desc img_desc = { };
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.data.subimage[0][0] = sg_range{ font_pixels, size_t(font_width * font_height * 4) };
    state.bind.fs.images[0] = sg_make_image(&img_desc);

    sg_sampler_desc smp_desc = { };
    smp_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    state.bind.fs.samplers[0] = sg_make_sampler(&smp_desc);

    // shader object for imgui rendering
    sg_shader_desc shd_desc = { };
    shd_desc.vs.uniform_blocks[0].size = sizeof(vs_params_t);
    shd_desc.vs.source =
        "struct vs_params {\n"
        "  disp_size: vec2f,\n"
        "}\n"
        "@group(0) @binding(0) var<uniform> in: vs_params;\n"
        "struct vs_out {\n"
        "  @builtin(position) pos: vec4f,\n"
        "  @location(0) uv: vec2f,\n"
        "  @location(1) color: vec4f,\n"
        "}\n"
        "@vertex fn main(@location(0) pos: vec2f, @location(1) uv: vec2f, @location(2) color: vec4f) -> vs_out {\n"
        "  var out: vs_out;\n"
        "  out.pos = vec4f(((pos/in.disp_size) - 0.5) * vec2f(2,-2), 0.5, 1.0);\n"
        "  out.uv = uv;\n"
        "  out.color = color;\n"
        "  return out;\n"
        "}\n";
    shd_desc.fs.images[0].used = true;
    shd_desc.fs.samplers[0].used = true;
    shd_desc.fs.image_sampler_pairs[0].used = true;
    shd_desc.fs.image_sampler_pairs[0].image_slot = 0;
    shd_desc.fs.image_sampler_pairs[0].sampler_slot = 0;
    shd_desc.fs.source =
        "@group(1) @binding(48) var tex: texture_2d<f32>;\n"
        "@group(1) @binding(64) var smp: sampler;\n"
        "@fragment fn main(@location(0) uv: vec2f, @location(1) color: vec4f) -> @location(0) vec4f {\n"
        "  return textureSample(tex, smp, uv) * color;\n"
        "}\n";
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
    state.pip = sg_make_pipeline(&pip_desc);

    // initial clear color
    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.0f, 0.5f, 0.7f, 1.0f };
}

static void frame(void) {
    const int cur_width = wgpu_width();
    const int cur_height = wgpu_height();
    const double cur_delta_time = stm_sec(stm_laptime(&state.last_time));

    // this is standard ImGui demo code
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(cur_width), float(cur_height));
    io.DeltaTime = (float) cur_delta_time;
    ImGui::NewFrame();

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &state.pass_action.colors[0].clear_value.r);
    if (ImGui::Button("Test Window")) state.show_test_window ^= 1;
    if (ImGui::Button("Another Window")) state.show_another_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (state.show_another_window) {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiCond_FirstUseEver);
        ImGui::Begin("Another Window", &state.show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowDemoWindow()
    if (state.show_test_window) {
        ImGui::SetNextWindowPos(ImVec2(460, 20), ImGuiCond_FirstUseEver);
        ImGui::ShowDemoWindow();
    }

    // the sokol_gfx draw pass
    sg_pass pass = {};
    pass.action = state.pass_action;
    pass.swapchain = wgpu_swapchain();
    sg_begin_pass(&pass);
    ImGui::Render();
    draw_imgui(ImGui::GetDrawData());
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
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
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE(vs_params));
    for (int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];

        // append vertices and indices to buffers, record start offsets in bindings struct
        const uint32_t vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
        const uint32_t idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
        const uint32_t vb_offset = sg_append_buffer(state.bind.vertex_buffers[0], { &cl->VtxBuffer.front(), vtx_size });
        const uint32_t ib_offset = sg_append_buffer(state.bind.index_buffer, { &cl->IdxBuffer.front(), idx_size });
        /* don't render anything if the buffer is in overflow state (this is also
            checked internally in sokol_gfx, draw calls that attempt to draw from
            overflowed buffers will be silently dropped)
        */
        if (sg_query_buffer_overflow(state.bind.vertex_buffers[0]) ||
            sg_query_buffer_overflow(state.bind.index_buffer))
        {
            continue;
        }

        state.bind.vertex_buffer_offsets[0] = vb_offset;
        state.bind.index_buffer_offset = ib_offset;
        sg_apply_bindings(&state.bind);

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

int main() {
    wgpu_desc_t desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.shutdown_cb = shutdown;
    desc.width = Width;
    desc.height = Height;
    desc.title = "imgui-wgpu";
    wgpu_start(&desc);
    return 0;
}
