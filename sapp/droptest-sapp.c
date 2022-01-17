//------------------------------------------------------------------------------
//  droptest-sapp.c
//  Test drag'n'drop file loading.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#define MAX_FILE_SIZE (1024 * 1024)

typedef enum {
    LOADSTATE_UNKNOWN = 0,
    LOADSTATE_SUCCESS,
    LOADSTATE_FAILED,
    LOADSTATE_FILE_TOO_BIG,
} loadstate_t;

static struct {
    loadstate_t load_state;
    int size;
    uint8_t buffer[MAX_FILE_SIZE];
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    simgui_setup(&(simgui_desc_t){ 0 });

    // on native platforms, use sokol_fetch.h to load the dropped-file content,
    // on web, sokol_app.h has a builtin helper function for this
    #if !defined(__EMSCRIPTEN__)
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 1,
    });
    #endif
}

// render the loaded file content as hex view
static void render_file_content(void) {
    const int bytes_per_line = 16;  // keep this 2^N
    const int num_lines = (state.size + (bytes_per_line - 1)) / bytes_per_line;

    igBeginChild_Str("##scrolling", (ImVec2){0,0}, false, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav);
    ImGuiListClipper clipper = { 0 };
    ImGuiListClipper_Begin(&clipper, num_lines, igGetTextLineHeight());
    ImGuiListClipper_Step(&clipper);
    for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) {
        int start_offset = line_i * bytes_per_line;
        int end_offset = start_offset + bytes_per_line;
        if (end_offset >= state.size) {
            end_offset = state.size;
        }
        igText("%04X: ", start_offset);
        for (int i = start_offset; i < end_offset; i++) {
            igSameLine(0.0f, 0.0f);
            igText("%02X ", state.buffer[i]);
        }
        igSameLine((6 * 7.0f) + (bytes_per_line * 3 * 7.0f) + (2 * 7.0f), 0.0f);
        for (int i = start_offset; i < end_offset; i++) {
            if (i != start_offset) {
                igSameLine(0.0f, 0.0f);
            }
            uint8_t c = state.buffer[i];
            if ((c < 32) || (c > 127)) {
                c = '.';
            }
            igText("%c", c);
        }
    }
    igText("EOF\n");
    ImGuiListClipper_End(&clipper);
    igEndChild();
}

static void frame(void) {
    #if !defined(__EMSCRIPTEN__)
    sfetch_dowork();
    #endif

    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = width,
        .height = height,
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });

    igSetNextWindowPos((ImVec2){10, 10}, ImGuiCond_Once, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){600,500}, ImGuiCond_Once);
    igBegin("Drop a file!", 0, 0);
    if (state.load_state != LOADSTATE_UNKNOWN) {
        igText("%s:", sapp_get_dropped_file_path(0));
    }
    switch (state.load_state) {
        case LOADSTATE_FAILED:
            igText("LOAD FAILED!");
            break;
        case LOADSTATE_FILE_TOO_BIG:
            igText("FILE TOO BIG!");
            break;
        case LOADSTATE_SUCCESS:
            igSeparator();
            render_file_content();
            break;
        default:
            break;
    }
    igEnd();

    sg_begin_default_pass(&(sg_pass_action){0}, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    #if !defined(__EMSCRIPTEN__)
    sfetch_shutdown();
    #endif
    simgui_shutdown();
    sg_shutdown();
}

#if defined(__EMSCRIPTEN__)
// the async-loading callback for sapp_html5_fetch_dropped_file
static void emsc_load_callback(const sapp_html5_fetch_response* response) {
    if (response->succeeded) {
        state.load_state = LOADSTATE_SUCCESS;
        state.size = (int) response->fetched_size;
    }
    else if (SAPP_HTML5_FETCH_ERROR_BUFFER_TOO_SMALL == response->error_code) {
        state.load_state = LOADSTATE_FILE_TOO_BIG;
    }
    else {
        state.load_state = LOADSTATE_FAILED;
    }
}
#else
// the async-loading callback for native platforms
static void native_load_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_state = LOADSTATE_SUCCESS;
        state.size = (int)response->fetched_size;
    }
    else if (response->error_code == SFETCH_ERROR_BUFFER_TOO_SMALL) {
        state.load_state = LOADSTATE_FILE_TOO_BIG;
    }
    else {
        state.load_state = LOADSTATE_FAILED;
    }
}
#endif

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
    if (ev->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        #if defined(__EMSCRIPTEN__)
            // on emscripten need to use the sokol-app helper function to load the file data
            sapp_html5_fetch_dropped_file(&(sapp_html5_fetch_request){
                .dropped_file_index = 0,
                .callback = emsc_load_callback,
                .buffer_ptr = state.buffer,
                .buffer_size = sizeof(state.buffer),
            });
        #else
            // native platform: use sokol-fetch to load file content
            sfetch_send(&(sfetch_request_t){
                .path = sapp_get_dropped_file_path(0),
                .callback = native_load_callback,
                .buffer_ptr = state.buffer,
                .buffer_size = sizeof(state.buffer)
            });
        #endif
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "droptest-sapp",
        .enable_dragndrop = true,
        .max_dropped_files = 1,
        .icon.sokol_default = true,
    };
}
