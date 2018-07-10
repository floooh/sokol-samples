//------------------------------------------------------------------------------
//  imgui-highdpi-sapp.c
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "imgui.h"
#include "imgui_font.h"

const int MaxVertices = (1<<16);
const int MaxIndices = MaxVertices * 3;

uint64_t last_time = 0;
bool show_test_window = true;
bool show_another_window = false;

sg_draw_state draw_state = { };
sg_pass_action pass_action = { };
ImDrawVert vertices[MaxVertices];
uint16_t indices[MaxIndices];
bool btn_down[SAPP_MAX_MOUSEBUTTONS] = { };
bool btn_up[SAPP_MAX_MOUSEBUTTONS] = { };

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

void imgui_draw_cb(ImDrawData*);

extern const char* vs_src;
extern const char* fs_src;

void init() {
    // setup sokol-gfx and sokol-time
    sg_desc desc = { };
    desc.mtl_device = sapp_metal_get_device();
    desc.mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor;
    desc.mtl_drawable_cb = sapp_metal_get_drawable;
    desc.d3d11_device = sapp_d3d11_get_device();
    desc.d3d11_device_context = sapp_d3d11_get_device_context();
    desc.d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view;
    desc.d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view;
    sg_setup(&desc);
    stm_setup();

    // setup Dear Imgui 
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fontCfg;
    fontCfg.FontDataOwnedByAtlas = false;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.RasterizerMultiply = 1.5f;
    io.Fonts->AddFontFromMemoryTTF(dump_font, sizeof(dump_font), 16.0f, &fontCfg);
    io.IniFilename = nullptr;
    io.RenderDrawListsFn = imgui_draw_cb;
    io.KeyMap[ImGuiKey_Tab] = SAPP_KEYCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = SAPP_KEYCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SAPP_KEYCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SAPP_KEYCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SAPP_KEYCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SAPP_KEYCODE_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = SAPP_KEYCODE_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = SAPP_KEYCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SAPP_KEYCODE_END;
    io.KeyMap[ImGuiKey_Delete] = SAPP_KEYCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SAPP_KEYCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = SAPP_KEYCODE_SPACE;
    io.KeyMap[ImGuiKey_Enter] = SAPP_KEYCODE_ENTER;
    io.KeyMap[ImGuiKey_Escape] = SAPP_KEYCODE_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SAPP_KEYCODE_A;
    io.KeyMap[ImGuiKey_C] = SAPP_KEYCODE_C;
    io.KeyMap[ImGuiKey_V] = SAPP_KEYCODE_V;
    io.KeyMap[ImGuiKey_X] = SAPP_KEYCODE_X;
    io.KeyMap[ImGuiKey_Y] = SAPP_KEYCODE_Y;
    io.KeyMap[ImGuiKey_Z] = SAPP_KEYCODE_Z;

    // dynamic vertex- and index-buffers for imgui-generated geometry
    sg_buffer_desc vbuf_desc = { };
    vbuf_desc.usage = SG_USAGE_STREAM;
    vbuf_desc.size = sizeof(vertices);
    draw_state.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

    sg_buffer_desc ibuf_desc = { };
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.usage = SG_USAGE_STREAM;
    ibuf_desc.size = sizeof(indices);
    draw_state.index_buffer = sg_make_buffer(&ibuf_desc);

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
    img_desc.min_filter = SG_FILTER_LINEAR;
    img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.content.subimage[0][0].ptr = font_pixels;
    img_desc.content.subimage[0][0].size = font_width * font_height * 4;
    draw_state.fs_images[0] = sg_make_image(&img_desc);

    // shader object for imgui rendering
    sg_shader_desc shd_desc = { };
    auto& ub = shd_desc.vs.uniform_blocks[0];
    ub.size = sizeof(vs_params_t);
    ub.uniforms[0].name = "disp_size";
    ub.uniforms[0].type = SG_UNIFORMTYPE_FLOAT2;
    shd_desc.fs.images[0].name = "tex";
    shd_desc.fs.images[0].type = SG_IMAGETYPE_2D;
    shd_desc.vs.source = vs_src;
    shd_desc.fs.source = fs_src;
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = { };
    pip_desc.layout.buffers[0].stride = sizeof(ImDrawVert);
    auto& attrs = pip_desc.layout.attrs;
    attrs[0].name="position"; attrs[0].sem_name="POSITION"; attrs[0].offset=offsetof(ImDrawVert, pos); attrs[0].format=SG_VERTEXFORMAT_FLOAT2;
    attrs[1].name="texcoord0"; attrs[1].sem_name="TEXCOORD"; attrs[1].offset=offsetof(ImDrawVert, uv); attrs[1].format=SG_VERTEXFORMAT_FLOAT2;
    attrs[2].name="color0"; attrs[2].sem_name="COLOR"; attrs[2].offset=offsetof(ImDrawVert, col); attrs[2].format=SG_VERTEXFORMAT_UBYTE4N;
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.blend.enabled = true;
    pip_desc.blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.blend.color_write_mask = SG_COLORMASK_RGB;
    draw_state.pipeline = sg_make_pipeline(&pip_desc);

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 0.0f;
    pass_action.colors[0].val[1] = 0.5f;
    pass_action.colors[0].val[2] = 0.7f;
    pass_action.colors[0].val[3] = 1.0f;
}

void frame() {
    const int cur_width = sapp_width();
    const int cur_height = sapp_height();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(cur_width), float(cur_height));
    io.DeltaTime = (float) stm_sec(stm_laptime(&last_time));
    for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
        if (btn_down[i]) {
            btn_down[i] = false;
            io.MouseDown[i] = true;
        }
        else if (btn_up[i]) {
            btn_up[i] = false;
            io.MouseDown[i] = false;
        }
    }
    if (io.WantTextInput && !sapp_keyboard_shown()) {
        sapp_show_keyboard(true);
    }
    if (!io.WantTextInput && sapp_keyboard_shown()) {
        sapp_show_keyboard(false);
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
    sg_begin_default_pass(&pass_action, cur_width, cur_height);
    ImGui::Render();
    sg_end_pass();
    sg_commit();
}

void cleanup() {
    sg_shutdown();
}

void input(const sapp_event* event) {
    auto& io = ImGui::GetIO();
    io.KeyAlt = (event->modifiers & SAPP_MODIFIER_ALT) != 0;
    io.KeyCtrl = (event->modifiers & SAPP_MODIFIER_CTRL) != 0;
    io.KeyShift = (event->modifiers & SAPP_MODIFIER_SHIFT) != 0;
    io.KeySuper = (event->modifiers & SAPP_MODIFIER_SUPER) != 0;
    const float dpi_scale = sapp_dpi_scale();
    switch (event->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            io.MousePos.x = event->mouse_x / dpi_scale;
            io.MousePos.y = event->mouse_y / dpi_scale;
            btn_down[event->mouse_button] = true;
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            io.MousePos.x = event->mouse_x / dpi_scale;
            io.MousePos.y = event->mouse_y / dpi_scale;
            btn_up[event->mouse_button] = true;
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            io.MousePos.x = event->mouse_x / dpi_scale;
            io.MousePos.y = event->mouse_y / dpi_scale;
            break;
        case SAPP_EVENTTYPE_MOUSE_ENTER:
        case SAPP_EVENTTYPE_MOUSE_LEAVE:
            for (int i = 0; i < 3; i++) {
                btn_down[i] = false;
                btn_up[i] = false;
                io.MouseDown[i] = false;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            io.MouseWheelH = event->scroll_x;
            io.MouseWheel = event->scroll_y;
            break;
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
            btn_down[0] = true;
            io.MousePos.x = event->touches[0].pos_x / dpi_scale;
            io.MousePos.y = event->touches[0].pos_y / dpi_scale;
            break;
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
            io.MousePos.x = event->touches[0].pos_x / dpi_scale;
            io.MousePos.y = event->touches[0].pos_y / dpi_scale;
            break;
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
            btn_up[0] = true;
            io.MousePos.x = event->touches[0].pos_x / dpi_scale;
            io.MousePos.y = event->touches[0].pos_y / dpi_scale;
            break;
        case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
            btn_up[0] = btn_down[0] = false;
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            io.KeysDown[event->key_code] = true;
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            io.KeysDown[event->key_code] = false;
            break;
        case SAPP_EVENTTYPE_CHAR:
            io.AddInputCharacter((ImWchar)event->char_code);
            break;
        default:
            break;
    }
}

// imgui draw callback
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
    const float dpi_scale = sapp_dpi_scale();
    vs_params_t vs_params;
    vs_params.disp_size.x = ImGui::GetIO().DisplaySize.x / dpi_scale;
    vs_params.disp_size.y = ImGui::GetIO().DisplaySize.y / dpi_scale;
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
                const int scissor_x = (int) (pcmd.ClipRect.x * dpi_scale);
                const int scissor_y = (int) (pcmd.ClipRect.y * dpi_scale);
                const int scissor_w = (int) ((pcmd.ClipRect.z - pcmd.ClipRect.x) * dpi_scale);
                const int scissor_h = (int) ((pcmd.ClipRect.w - pcmd.ClipRect.y) * dpi_scale);
                sg_apply_scissor_rect(scissor_x, scissor_y, scissor_w, scissor_h, true);
                sg_draw(base_element, pcmd.ElemCount, 1);
            }
            base_element += pcmd.ElemCount;
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1024;
    desc.height = 768;
    desc.fullscreen = true;
    desc.high_dpi = true;
    desc.ios_keyboard_resizes_canvas = false;
    desc.window_title = "Dear ImGui HighDPI (sokol-app)";
    return desc;
}

#if defined(SOKOL_GLCORE33)
const char* vs_src =
    "#version 330\n"
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
    "}\n";
const char* fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_GLES2)
const char* vs_src =
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
    "}\n";
const char* fs_src =
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "varying vec2 uv;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_GLES3)
const char* vs_src =
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
    "}\n";
const char* fs_src =
    "#version 300 es\n"
    "precision mediump float;"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_METAL)
const char* vs_src =
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
    "}\n";
const char* fs_src = 
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float2 uv;\n"
    "  float4 color;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return tex.sample(smp, in.uv) * in.color;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
const char* vs_src =
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
const char* fs_src =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv) * color;\n"
    "}\n";
#endif
