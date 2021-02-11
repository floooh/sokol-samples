//------------------------------------------------------------------------------
//  imgui-highdpi-sapp.c
//
//  This demonstrates Dear ImGui rendering via sokol_gfx.h and sokol_imgui.h,
//  with HighDPI rendering and a custom embedded font.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_glue.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_font.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

static bool show_test_window = true;
static bool show_another_window = false;
static bool show_quit_dialog = false;
static bool html5_ask_leave_site = false;

static sg_pass_action pass_action;

ImFont *g_font, *g_font_monospace;
char InputBuf1[256], InputBuf2[256];

// In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
    return 0;
}

void init(void) {
    // setup sokol-gfx and sokol-time
    sg_desc desc = { };
    desc.context = sapp_sgcontext();
    sg_setup(&desc);

    // setup sokol-imgui, but provide our own font
    simgui_desc_t simgui_desc = { };
    simgui_desc.no_default_font = true;
    simgui_desc.dpi_scale = sapp_dpi_scale();
    simgui_setup(&simgui_desc);

    // configure Dear ImGui with our own embedded font
    auto& io = ImGui::GetIO();
    /*ImFontConfig fontCfg;
    fontCfg.FontDataOwnedByAtlas = false;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.RasterizerMultiply = 1.5f;
    io.Fonts->AddFontFromMemoryTTF(dump_font, sizeof(dump_font), 56.0f, &fontCfg);
*/
    ImFontConfig config_words;
    config_words.OversampleV = 2;
    config_words.OversampleH = 2; // 默认为3
    config_words.RasterizerMultiply = 1.5f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    ImWchar* glyph = NULL;
    int fontcn = 0;//atoi(cp.get("-fontcn", "-1"));
    if(fontcn == 0)
        glyph = (ImWchar* )io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    else if(fontcn == 1)
        glyph = (ImWchar* )io.Fonts->GetGlyphRangesChineseFull();
    //const char *font = "fonts/Cousine-Regular.ttf";
    const char *font = "/system/fonts/NotoSansCJK-Regular.ttc";
    g_font = io.Fonts->AddFontFromFileTTF(font, 24.0, &config_words, glyph);
    //g_font_monospace = io.Fonts->AddFontFromFileTTF("/system/fonts/DroidSansMono.ttf", 24.0+2.0, &config_words);

    io.FontGlobalScale = 1.8f;
    // create font texture for the custom font
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
    io.Fonts->TexID = (ImTextureID)(uintptr_t) sg_make_image(&img_desc).id;

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].val[0] = 0.0f;
    pass_action.colors[0].val[1] = 0.3f;
    pass_action.colors[0].val[2] = 0.3f;
    pass_action.colors[0].val[3] = 1.0f;
}

void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame(width, height, 1.0/60.0);

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    ImGui::SetNextWindowPos(ImVec2(0, 60));
    ImGui::Begin("Main");
    ImGui::SetWindowFontScale(1.5f);
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].val[0]);
    ImGui::Text("width: %d, height: %d\n", sapp_width(), sapp_height());
    if (ImGui::Button("Test Window")) show_test_window ^= 1;
    ImGui::SameLine();
    if (ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("NOTE: 移动终端不支持退出programmatic quit isn't supported on mobile");
    if (ImGui::Button("Soft Quit")) {
        sapp_request_quit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Hard Quit")) {
        sapp_quit();
    }
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
    ImGui::PushItemWidth(ImGui::GetColumnWidth() - ImGui::CalcTextSize("<<").x - ImGui::GetScrollX() - 1 * ImGui::GetStyle().ItemSpacing.x);
    if (1) {
        ImGui::InputText("in1", InputBuf1, IM_ARRAYSIZE(InputBuf1), input_text_flags, &TextEditCallbackStub, (void*)NULL);
        ImGui::Text("Input1:%s", InputBuf1);
        reclaim_focus = true;
    }
    else
        ImGui::Text("Input1:");
    ImGui::PopItemWidth();
    // Auto-focus on window apparition
    //ImGui::SetItemDefaultFocus();

    ImGui::PushItemWidth(ImGui::GetColumnWidth() - ImGui::CalcTextSize("<<").x - ImGui::GetScrollX() - 1 * ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::InputText("in2", InputBuf2, IM_ARRAYSIZE(InputBuf2), input_text_flags, &TextEditCallbackStub, (void*)NULL)) {
        ImGui::Text("Enter:%s", InputBuf2);
        reclaim_focus = true;
    }
    else
        ImGui::Text("Enter:");
    ImGui::PopItemWidth();
    //if (reclaim_focus)
    //    ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
    // Note: we are using a fixed-sized buffer for simplicity here. See ImGuiInputTextFlags_CallbackResize
    // and the code in misc/cpp/imgui_stdlib.h for how to setup InputText() for dynamically resizing strings.
    static char text[1024] =
        "/*"
        "The Pentium F00F bug, shorthand for F0 0F C7 C8,\n"
        "the hexadecimal encoding of one offending instruction,\n"
        "一段中文\n"
        " more formally, the invalid operand with locked CMPXCHG8B\n"
        " instruction bug, is a design flaw in the majority of\n"
        " Intel Pentium, Pentium MMX, and Pentium OverDrive\n"
        " processors (all in the P5 microarchitecture).\n"
        "*/\n\n"
        "label:\n"
        "\tlock cmpxchg8b eax\n";

    static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
    ImGui::InputTextMultiline("##source", text, IM_ARRAYSIZE(text), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8), flags);

    if (ImGui::Checkbox("HTML5 Ask Leave Site", &html5_ask_leave_site)) {
        sapp_html5_ask_leave_site(html5_ask_leave_site);
    }
    if (ImGui::Button(sapp_is_fullscreen() ? "Switch to windowed" : "Switch to fullscreen")) {
        sapp_toggle_fullscreen();
    }
    
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

    // 4. Prepare and conditionally open the "Really Quit?" popup
    if (ImGui::BeginPopupModal("Really Quit?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you really want to quit?\n");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            sapp_quit();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (show_quit_dialog) {
        ImGui::OpenPopup("Really Quit?");
        show_quit_dialog = false;
    }
    ImGui::End();
    // the sokol_gfx draw pass
    sg_begin_default_pass(&pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event) {
    if (event->type == SAPP_EVENTTYPE_QUIT_REQUESTED) {
        show_quit_dialog = true;
        sapp_cancel_quit();
    }
    else {
        simgui_handle_event(event);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1024;
    desc.height = 768;
    desc.fullscreen = true;
    desc.high_dpi = true;
    desc.html5_ask_leave_site = html5_ask_leave_site;
    desc.ios_keyboard_resizes_canvas = false;
    desc.gl_force_gles2 = true;
    desc.window_title = "Dear ImGui HighDPI (sokol-app)";
    return desc;
}
