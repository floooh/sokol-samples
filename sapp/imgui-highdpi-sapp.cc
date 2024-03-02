//------------------------------------------------------------------------------
//  imgui-highdpi-sapp.c
//
//  This demonstrates Dear ImGui rendering via sokol_gfx.h and sokol_imgui.h,
//  with HighDPI rendering and a custom embedded font.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "imgui.h"
#include "imgui_font.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

static bool show_test_window = true;
static bool show_another_window = false;
static bool show_quit_dialog = false;
static bool html5_ask_leave_site = false;

static sg_pass_action pass_action;

void init(void) {
    // setup sokol-gfx and sokol-time
    sg_desc desc = { };
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    // setup sokol-imgui, but provide our own font
    simgui_desc_t simgui_desc = { };
    simgui_desc.no_default_font = true;
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);

    // configure Dear ImGui with our own embedded font
    auto& io = ImGui::GetIO();
    ImFontConfig fontCfg;
    fontCfg.FontDataOwnedByAtlas = false;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 2;
    fontCfg.RasterizerMultiply = 1.5f;
    io.Fonts->AddFontFromMemoryTTF(dump_font, sizeof(dump_font), 16.0f, &fontCfg);

    // create font texture and linear-filtering sampler for the custom font
    // NOTE: linear filtering looks better on low-dpi displays, while
    // nearest-filtering looks better on high-dpi displays
    simgui_font_tex_desc_t font_texture_desc = { };
    font_texture_desc.min_filter = SG_FILTER_LINEAR;
    font_texture_desc.mag_filter = SG_FILTER_LINEAR;
    simgui_create_fonts_texture(&font_texture_desc);

    // initial clear color
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = { 0.3f, 0.7f, 0.0f, 1.0f };
}

void frame(void) {
    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });

    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    static float f = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].clear_value.r);
    ImGui::Text("width: %d, height: %d, dpi_scale: %.1f\n", sapp_width(), sapp_height(), sapp_dpi_scale());
    if (ImGui::Button("Test Window")) show_test_window ^= 1;
    if (ImGui::Button("Another Window")) show_another_window ^= 1;
    ImGui::Text("NOTE: programmatic quit isn't supported on mobile");
    if (ImGui::Button("Soft Quit")) {
        sapp_request_quit();
    }
    if (ImGui::Button("Hard Quit")) {
        sapp_quit();
    }
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

    // the sokol_gfx draw pass
    sg_pass pass = {};
    pass.action = pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
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
    desc.window_title = "Dear ImGui HighDPI (sokol-app)";
    desc.icon.sokol_default = true;
    desc.enable_clipboard = true;
    desc.logger.func = slog_func;
    return desc;
}
