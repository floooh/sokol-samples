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
    LOADSTATE_TOOBIG,
} loadstate_t;

static struct {
    loadstate_t load_state;
    uint32_t size;
    uint8_t buffer[MAX_FILE_SIZE];
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });
    simgui_setup(&(simgui_desc_t){ 0 });

    #if !defined(__EMSCRIPTEN__)
    sfetch_setup(&(sfetch_desc_t){
        .num_channels = 1,
        .num_lanes = 1,
    });
    #endif
}

// render the loaded file content as hex view
static void render_file_content(void) {
    const int bytes_per_line = 16;
    const int num_lines = state.size / bytes_per_line;
    
    igBeginChildStr("##scrolling", (ImVec2){0,0}, false, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav);
    ImGuiListClipper clipper = { 0 };
    ImGuiListClipper_Begin(&clipper, num_lines, igGetTextLineHeight());
    ImGuiListClipper_Step(&clipper);
    for (uint32_t line_i = clipper.DisplayStart; line_i < (uint32_t)clipper.DisplayEnd; line_i++) {
        uint32_t start_offset = line_i * bytes_per_line;
        uint32_t end_offset = start_offset + bytes_per_line;
        if (end_offset >= state.size) {
            end_offset = state.size;
        }
        igText("%04X: ", start_offset);
        for (uint32_t i = start_offset; i < end_offset; i++) {
            igSameLine(0.0f, 0.0f);
            igText("%02X ", state.buffer[i]);
        }
        igSameLine(0.0f, 0.0f); igText("    ");
        for (uint32_t i = start_offset; i < end_offset; i++) {
            igSameLine(0.0f, 0.0f);
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
    simgui_new_frame(width, height, 1.0/60.0);

    igSetNextWindowPos((ImVec2){10, 10}, ImGuiCond_Once, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){600,500}, ImGuiCond_Once);
    igBegin("Drop a file!", 0, 0);
    if (state.load_state != LOADSTATE_UNKNOWN) {
        igText("%s:", sapp_get_dropped_file_path(0));
    }
    switch (state.load_state) {
        case LOADSTATE_FAILED:
            igText("LOAD FAILED!\n", sapp_get_dropped_file_path(0));
            break;
        case LOADSTATE_TOOBIG:
            igText("%s:", sapp_get_dropped_file_path(0));
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
        state.size = response->fetched_size;
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
        state.size = response->fetched_size;
    }
    else if (response->failed) {
        switch (response->error_code) {
            case SFETCH_ERROR_BUFFER_TOO_SMALL:
                state.load_state = LOADSTATE_TOOBIG;
                break;
            default:
                state.load_state = LOADSTATE_FAILED;
                break;
        }
    }
}
#endif

static void input(const sapp_event* ev) {
    simgui_handle_event(ev);
    if (ev->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        #if __EMSCRIPTEN__
            /* on emscripten we need to use sokol_app.h to load the file data,
                we also know the file size upfront and can check against
                the maximum allowed file size
            */
            if (sapp_html5_get_dropped_file_size(0) > MAX_FILE_SIZE) {
                state.load_state = LOADSTATE_TOOBIG;
            }
            else {
                sapp_html5_fetch_dropped_file(0, &(sapp_html5_fetch_request){
                    .callback = emsc_load_callback,
                    .buffer_ptr = state.buffer,
                    .buffer_size = sizeof(state.buffer),
                });
            }
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
        .html5_max_dropped_file_size = MAX_FILE_SIZE
    };
}
