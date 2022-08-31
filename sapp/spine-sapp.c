//------------------------------------------------------------------------------
//  spine-sapp.c
//  Simple sokol_spine.h sample
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "util/fileutil.h"
#define SOKOL_SPINE_IMPL
#include "spine/spine.h"
#include "sokol_spine.h"
#include "stb/stb_image.h"
#include "dbgui/dbgui.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_GFX_IMGUI_IMPL
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instance;
    sg_pass_action pass_action;
    struct {
        bool failed;
        int load_count;
        size_t atlas_data_size;
    } load_status;
    struct {
        uint8_t atlas[8 * 1024];
        uint8_t skeleton[256 * 1024];
        uint8_t image[256 * 1024];
    } buffers;
    struct {
        sg_imgui_t sgimgui;
        bool atlas_open;
        bool bones_open;
        bool slots_open;
        bool events_open;
        bool anims_open;
        bool instance_open;
        int sel_bone_index;
    } ui;
} state;

static void setup_spine_objects(void);
static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void ui_draw(void);

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });

    // setup UI libs
    simgui_setup(&(simgui_desc_t){0});
    sg_imgui_init(&state.ui.sgimgui, &(sg_imgui_desc_t){0});
    state.ui.sel_bone_index = -1;

    // setup sokol-fetch for loading up to 2 files in parallel
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
    });

    // setup sokol-spine with default attributes
    sspine_setup(&(sspine_desc){0});

    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 0.7f, 1.0f} }
    };

    // start loading Spine atlas and skeleton file asynchronously
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("spineboy.atlas", path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer_ptr = state.buffers.atlas,
        .buffer_size = sizeof(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path("spineboy-pro.json", path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer_ptr = state.buffers.skeleton,
        // the skeleton file is text data, make sure we have room for a terminating zero
        .buffer_size = sizeof(state.buffers.skeleton) - 1,
        .callback = skeleton_data_loaded,
    });
}

static void frame(void) {
    const double delta_time = sapp_frame_duration();

    sfetch_dowork();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .dpi_scale = sapp_dpi_scale(),
        .delta_time = delta_time,
    });

    // can call Spine drawing functions with invalid or 'incomplete' object handles
    sspine_new_frame();
    sspine_update_instance(state.instance, delta_time);
    sspine_draw_instance_in_layer(state.instance, 0);

    ui_draw();

    // the actual sokol-gfx render pass
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    // NOTE: using the display width/height here means the Spine rendering
    // is mapped to pixels and doesn't scale with window size
    sspine_draw_layer(0, &(sspine_layer_transform){
        .size   = { .x = sapp_widthf(), .y = sapp_heightf() },
        .origin = { .x = sapp_widthf() * 0.5f, .y = sapp_heightf() * 0.8f }
    });
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (simgui_handle_event(ev)) {
        return;
    }
}

static void cleanup(void) {
    sspine_shutdown();
    sfetch_shutdown();
    sg_imgui_discard(&state.ui.sgimgui);
    simgui_shutdown();
    sg_shutdown();
}

// called by sokol-fetch when atlas file has been loaded
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.load_status.atlas_data_size = response->fetched_size;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (++state.load_status.load_count == 2) {
            setup_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        // the loaded data file is JSON text, make sure it's zero terminated
        assert(response->fetched_size < sizeof(state.buffers.skeleton));
        state.buffers.skeleton[response->fetched_size] = 0;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (++state.load_status.load_count == 2) {
            setup_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called when both the Spine atlas and skeleton file has finished loading
static void setup_spine_objects(void) {
    // create atlas from file data
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = {
            .ptr = state.buffers.atlas,
            .size = state.load_status.atlas_data_size,
        },
        .override = {
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
        }
    });
    assert(sspine_atlas_valid(state.atlas));

    // create a skeleton object
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .prescale = 0.75f,
        .anim_default_mix = 0.2f,
        // we already made sure the JSON text data is zero-terminated
        .json_data = (const char*)&state.buffers.skeleton,
    });
    assert(sspine_skeleton_valid(state.skeleton));

    // create a skeleton instance
    state.instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = state.skeleton,
    });
    assert(sspine_instance_valid(state.instance));
    sspine_set_animation_by_name(state.instance, 0, "portal", false);
    sspine_add_animation_by_name(state.instance, 0, "run", true, 0.0f);

    // asynchronously load atlas images
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image_info img_info = sspine_get_image_info(state.atlas, img_index);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .channel = 0,
            .path = fileutil_get_path(img_info.filename, path_buf, sizeof(path_buf)),
            .buffer_ptr = state.buffers.image,
            .buffer_size = sizeof(state.buffers.image),
            .callback = image_data_loaded,
            // sspine_image_info is small enough to directly fit into the user data payload
            .user_data_ptr = &img_info,
            .user_data_size = sizeof(img_info)
        });
    }
}

// called when atlas image data has finished loading
static void image_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        const sspine_image_info* img_info = (sspine_image_info*)response->user_data;
        // decode image via stb_image.h
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->buffer_ptr,
            (int)response->fetched_size,
            &img_width, &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated a sokol-gfx image handle for use,
            // now "populate" the handle with an actual image
            sg_init_image(img_info->image, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = img_info->min_filter,
                .mag_filter = img_info->mag_filter,
                .label = img_info->filename,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(img_width * img_height * 4)
                }
            });
            stbi_image_free(pixels);
        }
        else {
            state.load_status.failed = false;
            sg_fail_image(img_info->image);
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

static const char* ui_sgfilter_name(sg_filter f) {
    switch (f) {
        case _SG_FILTER_DEFAULT: return "DEFAULT";
        case SG_FILTER_NEAREST: return "NEAREST";
        case SG_FILTER_LINEAR: return "LINEAR";
        case SG_FILTER_NEAREST_MIPMAP_NEAREST: return "NEAREST_MIPMAP_NEAREST";
        case SG_FILTER_NEAREST_MIPMAP_LINEAR: return "NEAREST_MIPMAP_LINEAR";
        case SG_FILTER_LINEAR_MIPMAP_NEAREST: return "LINEAR_MIPMAP_NEAREST";
        case SG_FILTER_LINEAR_MIPMAP_LINEAR: return "LINEAR_MIPMAP_LINEAR";
        default: return "???";
    }
}

static const char* ui_sgwrap_name(sg_wrap w) {
    switch (w) {
        case _SG_WRAP_DEFAULT: return "DEFAULT";
        case SG_WRAP_REPEAT: return "REPEAT";
        case SG_WRAP_CLAMP_TO_EDGE: return "CLAMP_TO_EDGE";
        case SG_WRAP_CLAMP_TO_BORDER: return "CLAMP_TO_BORDER";
        case SG_WRAP_MIRRORED_REPEAT: return "MIRRORED_REPEAT";
        default: return "???";
    }
}

static void ui_draw(void) {
    if (igBeginMainMenuBar()) {
        if (igBeginMenu("sokol-spine", true)) {
            if (igMenuItem_Bool("Load...", 0, false, true)) {
                // FIXME: open load window
            }
            igMenuItem_BoolPtr("Atlas...", 0, &state.ui.atlas_open, true);
            igMenuItem_BoolPtr("Instance...", 0, &state.ui.instance_open, true);
            igMenuItem_BoolPtr("Bones...", 0, &state.ui.bones_open, true);
            igMenuItem_BoolPtr("Slots...", 0, &state.ui.slots_open, true);
            igMenuItem_BoolPtr("Events...", 0, &state.ui.events_open, true);
            igMenuItem_BoolPtr("Anims...", 0, &state.ui.anims_open, true);
            igEndMenu();
        }
        if (igBeginMenu("sokol-gfx", true)) {
            igMenuItem_BoolPtr("Capabilities...", 0, &state.ui.sgimgui.caps.open, true);
            igMenuItem_BoolPtr("Buffers...", 0, &state.ui.sgimgui.buffers.open, true);
            igMenuItem_BoolPtr("Images...", 0, &state.ui.sgimgui.images.open, true);
            igMenuItem_BoolPtr("Shaders...", 0, &state.ui.sgimgui.shaders.open, true);
            igMenuItem_BoolPtr("Pipelines...", 0, &state.ui.sgimgui.pipelines.open, true);
            igMenuItem_BoolPtr("Passes...", 0, &state.ui.sgimgui.passes.open, true);
            igMenuItem_BoolPtr("Calls...", 0, &state.ui.sgimgui.capture.open, true);
            igEndMenu();
        }
        if (igBeginMenu("options", true)) {
            static int theme = 0;
            if (igRadioButton_IntPtr("Dark Theme", &theme, 0)) {
                igStyleColorsDark(0);
            }
            if (igRadioButton_IntPtr("Light Theme", &theme, 1)) {
                igStyleColorsLight(0);
            }
            if (igRadioButton_IntPtr("Classic Theme", &theme, 2)) {
                igStyleColorsClassic(0);
            }
            igEndMenu();
        }
        igEndMainMenuBar();
    }
    if (state.ui.atlas_open) {
        igSetNextWindowSize(IMVEC2(300, 330), ImGuiCond_Once);
        if (igBegin("Spine Atlas", &state.ui.atlas_open, 0)) {
            if (sspine_atlas_valid(state.atlas)) {
                const int num_atlas_pages = sspine_num_atlas_pages(state.atlas);
                igText("Num Pages: %d", num_atlas_pages);
                for (int i = 0; i < num_atlas_pages; i++) {
                    sspine_atlas_page page = sspine_atlas_page_at(state.atlas, i);
                    sspine_atlas_page_info info = sspine_get_atlas_page_info(page);
                    igSeparator();
                    igText("Name: %s", info.name);
                    igText("Width: %d", info.width);
                    igText("Height: %d", info.height);
                    igText("Premul Alpha: %s", (info.premultiplied_alpha == 0) ? "NO" : "YES");
                    igText("Original Spine params:");
                    igText("  Min Filter: %s", ui_sgfilter_name(info.min_filter));
                    igText("  Mag Filter: %s", ui_sgfilter_name(info.mag_filter));
                    igText("  Wrap U: %s", ui_sgwrap_name(info.wrap_u));
                    igText("  Wrap V: %s", ui_sgwrap_name(info.wrap_v));
                    igText("Overrides:");
                    igText("  Min Filter: %s", ui_sgfilter_name(info.overrides.min_filter));
                    igText("  Mag Filter: %s", ui_sgfilter_name(info.overrides.mag_filter));
                    igText("  Wrap U: %s", ui_sgwrap_name(info.overrides.wrap_u));
                    igText("  Wrap V: %s", ui_sgwrap_name(info.overrides.wrap_v));
                    igText("  Premul Alpha Enabled: %s", info.overrides.premul_alpha_enabled ? "YES" : "NO");
                    igText("  Premul Alpha Disabled: %s", info.overrides.premul_alpha_disabled ? "YES" : "NO");
                }
            }
        }
        igEnd();
    }
    if (state.ui.bones_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        if (igBegin("Bones", &state.ui.bones_open, 0)) {
            if (sspine_instance_valid(state.instance)) {
                const int num_bones = sspine_num_bones(state.instance);
                igText("Num Bones: %d", num_bones);
                igBeginChild_Str("bones_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_bones; i++) {
                    sspine_bone bone = sspine_bone_at(state.instance, i);
                    SOKOL_ASSERT(sspine_bone_valid(bone));
                    const sspine_bone_info info = sspine_get_bone_info(bone);
                    igPushID_Int(bone.index);
                    if (igSelectable_Bool(info.name, state.ui.sel_bone_index == bone.index, 0, IMVEC2(0,0))) {
                        state.ui.sel_bone_index = bone.index;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                sspine_bone bone = sspine_bone_at(state.instance, state.ui.sel_bone_index);
                if (sspine_bone_valid(bone)) {
                    const sspine_bone_info info = sspine_get_bone_info(bone);
                    igBeginChild_Str("bone_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name);
                    igText("Length: %.3f", info.length);
                    igText("Pose Transform:");
                    igText("  Position: %.3f,%.3f", info.pose_tform.position.x, info.pose_tform.position.y);
                    igText("  Rotation: %.3f", info.pose_tform.rotation);
                    igText("  Scale: %.3f,%.3f", info.pose_tform.scale.x, info.pose_tform.scale.y);
                    igText("  Shear: %.3f,%.3f", info.pose_tform.shear.x, info.pose_tform.shear.y);
                    igText("Current Transform:");
                    igText("  Position: %.3f,%.3f", info.cur_tform.position.x, info.cur_tform.position.y);
                    igText("  Rotation: %.3f", info.cur_tform.rotation);
                    igText("  Scale: %.3f,%.3f", info.cur_tform.scale.x, info.cur_tform.scale.y);
                    igText("  Shear: %.3f,%.3f", info.cur_tform.shear.x, info.cur_tform.shear.y);
                    igText("Color: %.2f,%.2f,%.2f,%.2f", info.color.r, info.color.b, info.color.g, info.color.a);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    sg_imgui_draw(&state.ui.sgimgui);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .window_title = "spine-sapp.c",
        .icon.sokol_default = true,
    };
}
