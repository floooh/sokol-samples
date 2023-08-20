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
    sg_range vertices;
    sg_range indices;
    uint64_t last_time = 0;
    bool show_test_window = true;
    bool show_another_window = false;
} state;

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

static void draw_imgui(ImDrawData*);

static size_t roundup4(size_t val) {
    return (val + 3) & ~3;
}

static void init(void) {
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_desc desc = { };
    desc.context = wgpu_get_context();
    desc.logger.func = slog_func;
    sg_setup(&desc);
    stm_setup();

    // allocate a big scratch buffer for indices and vertices
    state.vertices.size = MaxVertices * sizeof(ImDrawVert);
    state.vertices.ptr = malloc(state.vertices.size);
    state.indices.size = roundup4(MaxIndices * sizeof(ImDrawIdx));
    state.indices.ptr = malloc(state.indices.size);

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
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
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
        "@group(2) @binding(0) var tex: texture_2d<f32>;\n"
        "@group(2) @binding(1) var smp: sampler;\n"
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
    sg_begin_default_pass(&state.pass_action, cur_width, cur_height);
    ImGui::Render();
    draw_imgui(ImGui::GetDrawData());
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    free((void*)state.vertices.ptr); state.vertices.ptr = nullptr;
    free((void*)state.indices.ptr); state.indices.ptr = nullptr;
    sg_shutdown();
}

// render ImGui draw lists through sokol-gfx
void draw_imgui(ImDrawData* draw_data) {
    assert(draw_data);
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    // copy vertices and indices into common memory chunk
    ImDrawVert* vtx_base = (ImDrawVert*) state.vertices.ptr;
    ImDrawIdx* idx_base = (ImDrawIdx*) state.indices.ptr;
    size_t vtx_num = 0;
    size_t idx_num = 0;
    size_t num_valid_cmdlists = 0;
    for (size_t cl_index = 0; cl_index < (size_t)draw_data->CmdListsCount; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];
        // copy vertices and indices into an intermediate buffer, so that only
        // one update happens per frame, and also to round the index buffer
        // update size to 4
        if ((vtx_num + cl->VtxBuffer.Size) > MaxVertices) {
            break;
        }
        if ((idx_num + cl->IdxBuffer.Size) > MaxIndices) {
            break;
        }
        num_valid_cmdlists += 1;
        memcpy(vtx_base + vtx_num, cl->VtxBuffer.Data, cl->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_base + idx_num, cl->IdxBuffer.Data, cl->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_num += cl->VtxBuffer.Size;
        idx_num += cl->IdxBuffer.Size;
    }
    const size_t vtx_size = vtx_num * sizeof(ImDrawVert);
    const size_t idx_size = roundup4(idx_num * sizeof(ImDrawIdx));
    sg_update_buffer(state.bind.vertex_buffers[0], { vtx_base, vtx_size });
    sg_update_buffer(state.bind.index_buffer, { idx_base, idx_size });

    // render the command list
    vs_params_t vs_params;
    vs_params.disp_size.x = ImGui::GetIO().DisplaySize.x;
    vs_params.disp_size.y = ImGui::GetIO().DisplaySize.y;
    sg_apply_pipeline(state.pip);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE(vs_params));
    state.bind.vertex_buffer_offsets[0] = 0;
    state.bind.index_buffer_offset = 0;
    for (size_t cl_index = 0; cl_index < num_valid_cmdlists; cl_index++) {
        const ImDrawList* cl = draw_data->CmdLists[cl_index];

        sg_apply_bindings(&state.bind);
        state.bind.vertex_buffer_offsets[0] += cl->VtxBuffer.Size * sizeof(ImDrawVert);
        state.bind.index_buffer_offset += cl->IdxBuffer.Size * sizeof(ImDrawIdx);

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
