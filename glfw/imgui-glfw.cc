//------------------------------------------------------------------------------
//  imgui-glfw
//  Demonstrates basic integration with Dear Imgui (without custom
//  texture or custom font support).
//------------------------------------------------------------------------------
#include "imgui.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl/flextGL.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"

const int Width = 1024;
const int Height = 768;
const int MaxVertices = (1<<16);
const int MaxIndices = MaxVertices * 3;

bool show_test_window = false;
bool show_another_window = false;
ImVec4 clear_color = ImColor(114, 144, 154);

sg_draw_state draw_state = { };
sg_pass_action pass_action = { };
ImDrawVert vertices[MaxVertices];
uint16_t indices[MaxIndices];

typedef struct {
    ImVec2 disp_size;
} vs_params_t;

void imgui_draw_cb(ImDrawData*);

int main() {

    // window and GL context via GLFW and flextGL
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(Width, Height, "Sokol+ImGui+GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit(w);
    
    // GLFW to ImGui input forwarding
    glfwSetMouseButtonCallback(w, [](GLFWwindow* w, int btn, int action, int mods) {
        if ((btn >= 0) && (btn < 3)) {
            ImGui::GetIO().MouseDown[btn] = (action == GLFW_PRESS);
        }
    });
    glfwSetCursorPosCallback(w, [](GLFWwindow* w, double pos_x, double pos_y) {
        ImGui::GetIO().MousePos.x = float(pos_x);
        ImGui::GetIO().MousePos.y = float(pos_y);
    });
    glfwSetScrollCallback(w, [](GLFWwindow* w, double pos_x, double pos_y){
        ImGui::GetIO().MouseWheel = pos_y;
    });
    glfwSetKeyCallback(w, [](GLFWwindow* w, int key, int scancode, int action, int mods){
        ImGuiIO& io = ImGui::GetIO();
        if ((key >= 0) && (key < 512)) {
            io.KeysDown[key] = (action==GLFW_PRESS)||(action==GLFW_REPEAT);
        }
        io.KeyCtrl = (0 != (mods & GLFW_MOD_CONTROL));
        io.KeyAlt = (0 != (mods & GLFW_MOD_ALT));
        io.KeyShift = (0 != (mods & GLFW_MOD_SHIFT));
    });
    glfwSetCharCallback(w, [](GLFWwindow* w, unsigned int codepoint){
        ImGui::GetIO().AddInputCharacter((ImWchar)codepoint);
    });

    // setup sokol_gfx
    sg_desc desc = { }; 
    sg_setup(&desc);
    assert(sg_isvalid());

    // setup Dear Imgui 
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.RenderDrawListsFn = imgui_draw_cb;
    io.Fonts->AddFontDefault();
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;; 
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
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

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
    const int font_img_size = font_width * font_height * sizeof(uint32_t);
    const void* img_data_ptrs[] = { font_pixels };
    const int img_data_sizes[] = { font_img_size };
    sg_image_desc img_desc = { };
    img_desc.type = SG_IMAGETYPE_2D;
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.min_filter = SG_FILTER_NEAREST;
    img_desc.mag_filter = SG_FILTER_NEAREST;
    img_desc.num_data_items = 1;
    img_desc.data_ptrs = img_data_ptrs;
    img_desc.data_sizes = img_data_sizes;
    draw_state.fs_images[0] = sg_make_image(&img_desc);

    // shader object for imgui rendering
    sg_shader_desc shd_desc = { };
    shd_desc.vs.uniform_blocks[0].size = sizeof(vs_params_t);
    auto& u = shd_desc.vs.uniform_blocks[0].uniforms;
    u[0] = sg_named_uniform("disp_size", offsetof(vs_params_t, disp_size), SG_UNIFORMTYPE_FLOAT2, 1);
    shd_desc.vs.source =
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
    shd_desc.fs.images[0] = sg_named_image("tex", SG_IMAGETYPE_2D);
    shd_desc.fs.source =
        "#version 330\n"
        "uniform sampler2D tex;\n"
        "in vec2 uv;\n"
        "in vec4 color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "    frag_color = texture(tex, uv) * color;\n"
        "}\n";
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = { };
    pip_desc.vertex_layouts[0].stride = sizeof(ImDrawVert);
    auto& layouts = pip_desc.vertex_layouts;
    layouts[0].attrs[0] = sg_named_attr("position", offsetof(ImDrawVert, pos), SG_VERTEXFORMAT_FLOAT2);
    layouts[0].attrs[1] = sg_named_attr("texcoord0", offsetof(ImDrawVert, uv), SG_VERTEXFORMAT_FLOAT2);
    layouts[0].attrs[2] = sg_named_attr("color0", offsetof(ImDrawVert, col), SG_VERTEXFORMAT_UBYTE4N);
    pip_desc.shader = shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_ALWAYS;
    pip_desc.blend.enabled = true;
    pip_desc.blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.blend.color_write_mask = SG_COLORMASK_RGB;
    pip_desc.rasterizer.scissor_test_enabled = true;
    pip_desc.rasterizer.cull_mode = SG_CULLMODE_NONE;
    draw_state.pipeline = sg_make_pipeline(&pip_desc);

    // draw loop
    while (!glfwWindowShouldClose(w)) {
        int cur_width, cur_height;
        glfwGetWindowSize(w, &cur_width, &cur_height);

        // this is standard ImGui demo code
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(float(cur_width), float(cur_height));
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();

        // 1. Show a simple window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
        static float f = 0.0f;
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);
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
            ImGui::ShowTestWindow(&show_test_window);
        }

        // the sokol_gfx draw pass
        pass_action.colors[0].action = SG_ACTION_CLEAR;
        pass_action.colors[0].val[0] = clear_color.x;
        pass_action.colors[0].val[1] = clear_color.y;
        pass_action.colors[0].val[2] = clear_color.z;
        sg_begin_default_pass(&pass_action, cur_width, cur_height);
        ImGui::Render();
        sg_end_pass();
        sg_commit();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    /* cleanup */
    ImGui::Shutdown();
    sg_shutdown();
    glfwTerminate();
    return 0;
}

/* imgui draw callback */
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
                const int sw = (int) pcmd.ClipRect.z - pcmd.ClipRect.x;
                const int sh = (int) pcmd.ClipRect.w - pcmd.ClipRect.y;
                sg_apply_scissor_rect(sx, sy, sw, sh, true);
                sg_draw(base_element, pcmd.ElemCount, 1);
            }
            base_element += pcmd.ElemCount;
        }
    }
}

