//------------------------------------------------------------------------------
//  spine-inspector-sapp.c
//  Spine sample with inspector UI.
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

#define MAX_TRIGGERED_EVENTS (16)

static struct {
    sspine_atlas atlas;
    sspine_skeleton skeleton;
    sspine_instance instance;
    sg_pass_action pass_action;
    sspine_layer_transform layer_transform;
    sspine_vec2 iktarget_pos;
    struct {
        int scene_index;
        int pending_count;
        bool failed;
        size_t atlas_data_size;
        size_t skel_data_size;
        bool skel_data_is_binary;
    } load_status;
    struct {
        uint8_t atlas[8 * 1024];
        uint8_t skeleton[256 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
    struct {
        sg_imgui_t sgimgui;
        bool atlas_open;
        bool bones_open;
        bool slots_open;
        bool anims_open;
        bool events_open;
        bool iktargets_open;
        struct {
            int bone_index;
            int slot_index;
            int anim_index;
            int event_index;
            int iktarget_index;
        } selected;
        double cur_time;
        struct {
            double time;
            int index;
        } last_triggered_event;
    } ui;
} state;

// describe Spine scenes available for loading
#define MAX_SPINE_SCENES (4)
#define MAX_QUEUE_ANIMS (4)
typedef struct {
    const char* name;
    bool looping;
    float delay;
} anim_t;
typedef struct {
    const char* ui_name;
    const char* atlas_file;
    const char* skel_file_json;     // skeleton files are either json or binary
    const char* skel_file_binary;
    float prescale;
    sspine_atlas_overrides atlas_overrides;
    anim_t anim_queue[MAX_QUEUE_ANIMS];
} scene_t;
scene_t spine_scenes[MAX_SPINE_SCENES] = {
    {
        .ui_name = "Spine Boy",
        .atlas_file = "spineboy.atlas",
        .skel_file_json = "spineboy-pro.json",
        .prescale = 0.75f,
        .atlas_overrides = {
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
        },
        .anim_queue = {
            { .name = "portal" },
            { .name = "run", .looping = true },
        }
    },
    {
        .ui_name = "Raptor",
        .atlas_file = "raptor-pma.atlas",
        .skel_file_binary = "raptor-pro.skel",
        .prescale = 0.5f,
        .anim_queue = {
            { .name = "jump" },
            { .name = "roar" },
            { .name = "walk", .looping = true },
        }
    },
    {
        .ui_name = "Alien",
        .atlas_file = "alien-pma.atlas",
        .skel_file_binary = "alien-pro.skel",
        .prescale = 0.5f,
        .anim_queue = {
            { .name = "run", .looping = true },
            { .name = "death", .looping = false, .delay = 5.0f },
            { .name = "run", .looping = true },
            { .name = "death", .looping = true, .delay = 5.0f },
        },
    },
    {
        .ui_name = "Speedy",
        .atlas_file = "speedy-pma.atlas",
        .skel_file_binary = "speedy-ess.skel",
        .anim_queue = {
            { .name = "run", .looping = true }
        },
    },
};

// helper functions
static bool load_spine_scene(int scene_index);
static void setup_spine_objects();
static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void ui_setup(void);
static void ui_reset(void);
static void ui_shutdown(void);
static void ui_draw(void);

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    // setup UI libs
    ui_setup();
    // setup sokol-fetch for loading up to 2 files in parallel
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
    });
    // setup sokol-spine with default attributes
    sspine_setup(&(sspine_desc){0});
    // start loading Spine atlas and skeleton file asynchronously
    load_spine_scene(0);
}

static void frame(void) {
    const double delta_time = sapp_frame_duration();
    state.ui.cur_time += delta_time;
    state.layer_transform = (sspine_layer_transform) {
        .size   = { .x = sapp_widthf(), .y = sapp_heightf() },
        .origin = { .x = sapp_widthf() * 0.5f, .y = sapp_heightf() * 0.8f }
    };

    sfetch_dowork();
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .dpi_scale = sapp_dpi_scale(),
        .delta_time = delta_time,
    });

    // can call Spine functions with invalid or 'incomplete' object handles
    sspine_new_frame();

    // link IK target to mouse
    int iktarget_index = (state.ui.selected.iktarget_index == -1) ? 0 : state.ui.selected.iktarget_index;
    if (sspine_iktarget_index_valid(state.skeleton, iktarget_index)) {
        sspine_set_iktarget_world_pos(state.instance, iktarget_index, state.iktarget_pos);
    }

    // update instance animation and bone state
    sspine_update_instance(state.instance, delta_time);

    // draw instance to layer 0
    sspine_draw_instance_in_layer(state.instance, 0);

    // keep track of triggered events
    const int num_triggered_events = sspine_num_triggered_events(state.instance);
    for (int i = 0; i < num_triggered_events; i++) {
        state.ui.last_triggered_event.time = state.ui.cur_time;
        state.ui.last_triggered_event.index = sspine_get_triggered_event_info(state.instance, i).event_index;
    }

    ui_draw();

    // when loading has failed, change the clear color to red
    if (state.load_status.failed) {
        state.pass_action = (sg_pass_action){
            .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.0f, 0.0f, 1.0f} }
        };
    }
    else {
        state.pass_action = (sg_pass_action){
            .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.5f, 0.7f, 1.0f} }
        };
    }

    // the actual sokol-gfx render pass
    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    // NOTE: using the display width/height here means the Spine rendering
    // is mapped to pixels and doesn't scale with window size
    sspine_draw_layer(0, &state.layer_transform);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (simgui_handle_event(ev)) {
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        state.iktarget_pos.x = ev->mouse_x - state.layer_transform.origin.x;
        state.iktarget_pos.y = ev->mouse_y - state.layer_transform.origin.y;
    }
}

static void cleanup(void) {
    sspine_shutdown();
    sfetch_shutdown();
    ui_shutdown();
    sg_shutdown();
}

// start loading a spine scene asynchronously
static bool load_spine_scene(int scene_index) {
    assert((scene_index >= 0) && (scene_index < MAX_SPINE_SCENES));
    assert(spine_scenes[scene_index].atlas_file);
    assert(spine_scenes[scene_index].skel_file_json || spine_scenes[scene_index].skel_file_binary);

    // don't disturb any in-progress scene loading
    if (state.load_status.pending_count > 0) {
        return false;
    }
    state.load_status.scene_index = scene_index;
    state.load_status.pending_count = 0;
    state.load_status.failed = false;
    state.load_status.atlas_data_size = 0;
    state.load_status.skel_data_size = 0;
    state.load_status.skel_data_is_binary = false;

    // discard previous spine scene (functions can be called with invalid handles)
    sspine_destroy_instance(state.instance);
    sspine_destroy_skeleton(state.skeleton);
    sspine_destroy_atlas(state.atlas);

    // start loading the atlas file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path(spine_scenes[scene_index].atlas_file, path_buf, sizeof(path_buf)),
        .channel = 0,
        .buffer_ptr = state.buffers.atlas,
        .buffer_size = sizeof(state.buffers.atlas),
        .callback = atlas_data_loaded,
    });
    state.load_status.pending_count++;

    // start loading the skeleton file, this can either be provided as JSON or as binary data
    const char* skel_file = spine_scenes[scene_index].skel_file_json;
    if (!skel_file) {
        skel_file = spine_scenes[scene_index].skel_file_binary;
        state.load_status.skel_data_is_binary = true;
    }
    sfetch_send(&(sfetch_request_t){
        .path = fileutil_get_path(skel_file, path_buf, sizeof(path_buf)),
        .channel = 1,
        .buffer_ptr = state.buffers.skeleton,
        // in case the skeleton file is JSON text data, make sure we have room for a terminating zero
        .buffer_size = sizeof(state.buffers.skeleton) - 1,
        .callback = skeleton_data_loaded,
    });
    state.load_status.pending_count++;
    return true;
}

// called by sokol-fetch when atlas file has been loaded
static void atlas_data_loaded(const sfetch_response_t* response) {
    if (response->fetched || response->failed) {
        assert(state.load_status.pending_count > 0);
        state.load_status.pending_count--;
    }
    if (response->fetched) {
        state.load_status.atlas_data_size = response->fetched_size;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (state.load_status.pending_count == 0) {
            setup_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called by sokol-fetch when skeleton file has been loaded
static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched || response->failed) {
        assert(state.load_status.pending_count > 0);
        state.load_status.pending_count--;
    }
    if (response->fetched) {
        state.load_status.skel_data_size = response->fetched_size;
        // in case the loaded data file is JSON text, make sure it's zero terminated
        assert(response->fetched_size < sizeof(state.buffers.skeleton));
        state.buffers.skeleton[response->fetched_size] = 0;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (state.load_status.pending_count == 0) {
            setup_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called when both the Spine atlas and skeleton file has finished loading
static void setup_spine_objects(void) {
    const int scene_index = state.load_status.scene_index;
    ui_reset();

    // create atlas from file data
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = {
            .ptr = state.buffers.atlas,
            .size = state.load_status.atlas_data_size,
        },
        .override = spine_scenes[scene_index].atlas_overrides,
    });
    assert(sspine_atlas_valid(state.atlas));

    // create a skeleton object, the skeleton data might be either JSON or binary
    const char* skel_json_data = 0;
    sspine_range skel_binary_data = {0};
    if (state.load_status.skel_data_is_binary) {
        skel_binary_data = (sspine_range){
            .ptr = &state.buffers.skeleton,
            .size = state.load_status.skel_data_size,
        };
    }
    else {
        skel_json_data = (const char*)&state.buffers.skeleton;
    }
    state.skeleton = sspine_make_skeleton(&(sspine_skeleton_desc){
        .atlas = state.atlas,
        .prescale = spine_scenes[scene_index].prescale,
        .anim_default_mix = 0.2f,
        // one of those two will be valid:
        .json_data = skel_json_data,
        .binary_data = skel_binary_data
    });
    assert(sspine_skeleton_valid(state.skeleton));

    // create a skeleton instance
    state.instance = sspine_make_instance(&(sspine_instance_desc){
        .skeleton = state.skeleton,
    });
    assert(sspine_instance_valid(state.instance));

    // populate animation queue
    assert((scene_index >= 0) && (scene_index < MAX_SPINE_SCENES));
    for (int anim_index = 0; anim_index < MAX_QUEUE_ANIMS; anim_index++) {
        const anim_t* anim = &spine_scenes[scene_index].anim_queue[anim_index];
        if (anim->name) {
            if (anim_index == 0) {
                sspine_set_animation_by_name(state.instance, 0, anim->name, anim->looping);
            }
            else {
                sspine_add_animation_by_name(state.instance, 0, anim->name, anim->looping, anim->delay);
            }
        }
    }

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
        state.load_status.pending_count++;
    }
}

// called when atlas image data has finished loading
static void image_data_loaded(const sfetch_response_t* response) {
    if (response->fetched || response->failed) {
        assert(state.load_status.pending_count > 0);
        state.load_status.pending_count--;
    }
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

//=== UI STUFF =================================================================
static void ui_setup(void) {
    simgui_setup(&(simgui_desc_t){0});
    sg_imgui_init(&state.ui.sgimgui, &(sg_imgui_desc_t){0});
    ui_reset();
}

static void ui_reset(void) {
    state.ui.selected.bone_index = -1;
    state.ui.selected.slot_index = -1;
    state.ui.selected.anim_index = -1;
    state.ui.selected.event_index = -1;
    state.ui.selected.iktarget_index = -1;
    state.ui.last_triggered_event.index = -1;
}

static void ui_shutdown(void) {
    sg_imgui_discard(&state.ui.sgimgui);
    simgui_shutdown();
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
            if (igBeginMenu("Load", true)) {
                for (int i = 0; i < MAX_SPINE_SCENES; i++) {
                    if (spine_scenes[i].ui_name) {
                        if (igMenuItem_Bool(spine_scenes[i].ui_name, 0, (i == state.load_status.scene_index), true)) {
                            load_spine_scene(i);
                        }
                    }
                }
                igEndMenu();
            }
            igMenuItem_BoolPtr("Atlas...", 0, &state.ui.atlas_open, true);
            igMenuItem_BoolPtr("Bones...", 0, &state.ui.bones_open, true);
            igMenuItem_BoolPtr("Slots...", 0, &state.ui.slots_open, true);
            igMenuItem_BoolPtr("Anims...", 0, &state.ui.anims_open, true);
            igMenuItem_BoolPtr("Events...", 0, &state.ui.events_open, true);
            igMenuItem_BoolPtr("IK Targets...", 0, &state.ui.iktargets_open, true);
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
            if (!sspine_atlas_valid(state.atlas)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_atlas_pages = sspine_num_atlas_pages(state.atlas);
                igText("Num Pages: %d", num_atlas_pages);
                for (int i = 0; i < num_atlas_pages; i++) {
                    sspine_atlas_page_info info = sspine_get_atlas_page_info(state.atlas, i);
                    igSeparator();
                    igText("Name: %s", info.name);
                    igText("Width: %d", info.width);
                    igText("Height: %d", info.height);
                    igText("Premul Alpha: %s", (info.premul_alpha == 0) ? "NO" : "YES");
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
            if (!sspine_instance_valid(state.instance)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_bones = sspine_num_bones(state.skeleton);
                igText("Num Bones: %d", num_bones);
                igBeginChild_Str("bones_list", IMVEC2(128, 0), true, 0);
                for (int bone_index = 0; bone_index < num_bones; bone_index++) {
                    const sspine_bone_info info = sspine_get_bone_info(state.skeleton, bone_index);
                    igPushID_Int(bone_index);
                    if (igSelectable_Bool(info.name, state.ui.selected.bone_index == bone_index, 0, IMVEC2(0,0))) {
                        state.ui.selected.bone_index = bone_index;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_bone_index_valid(state.skeleton, state.ui.selected.bone_index)) {
                    const sspine_bone_info info = sspine_get_bone_info(state.skeleton, state.ui.selected.bone_index);
                    igBeginChild_Str("bone_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Parent Index: %d", info.parent_index);
                    igText("Name: %s", info.name);
                    igText("Length: %.3f", info.length);
                    igText("Pose Transform:");
                    igText("  Position: %.3f,%.3f", info.pose.position.x, info.pose.position.y);
                    igText("  Rotation: %.3f", info.pose.rotation);
                    igText("  Scale: %.3f,%.3f", info.pose.scale.x, info.pose.scale.y);
                    igText("  Shear: %.3f,%.3f", info.pose.shear.x, info.pose.shear.y);
                    igText("Color: %.2f,%.2f,%.2f,%.2f", info.color.r, info.color.b, info.color.g, info.color.a);
                    igText("Current Transform:");
                    const sspine_bone_transform cur_tform = sspine_get_bone_transform(state.instance, state.ui.selected.bone_index);
                    igText("  Position: %.3f,%.3f", cur_tform.position.x, cur_tform.position.y);
                    igText("  Rotation: %.3f", cur_tform.rotation);
                    igText("  Scale: %.3f,%.3f", cur_tform.scale.x, cur_tform.scale.y);
                    igText("  Shear: %.3f,%.3f", cur_tform.shear.x, cur_tform.shear.y);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    if (state.ui.slots_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        if (igBegin("Slots", &state.ui.slots_open, 0)) {
            if (!sspine_instance_valid(state.instance)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_slots = sspine_num_slots(state.skeleton);
                igText("Num Slots: %d", num_slots);
                igBeginChild_Str("slot_list", IMVEC2(128, 0), true, 0);
                for (int slot_index = 0; slot_index < num_slots; slot_index++) {
                    assert(sspine_slot_index_valid(state.skeleton, slot_index));
                    const sspine_slot_info info = sspine_get_slot_info(state.skeleton, slot_index);
                    igPushID_Int(slot_index);
                    if (igSelectable_Bool(info.name, state.ui.selected.slot_index == slot_index, 0, IMVEC2(0,0))) {
                        state.ui.selected.slot_index = slot_index;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_slot_index_valid(state.skeleton, state.ui.selected.slot_index)) {
                    const sspine_slot_info slot_info = sspine_get_slot_info(state.skeleton, state.ui.selected.slot_index);
                    const sspine_bone_info bone_info = sspine_get_bone_info(state.skeleton, slot_info.bone_index);
                    igBeginChild_Str("slot_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", slot_info.index);
                    igText("Name: %s", slot_info.name);
                    igText("Attachment: %s", slot_info.attachment_name ? slot_info.attachment_name : "-");
                    igText("Bone Name: %s", bone_info.name);
                    igText("Color: %.2f,%.2f,%.2f,%.2f", slot_info.color.r, slot_info.color.b, slot_info.color.g, slot_info.color.a);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    if (state.ui.anims_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        if (igBegin("Anims", &state.ui.anims_open, 0)) {
            if (!sspine_instance_valid(state.instance)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_anims = sspine_num_anims(state.skeleton);
                igText("Num Anims: %d", num_anims);
                igBeginChild_Str("anim_list", IMVEC2(128, 0), true, 0);
                for (int anim_index = 0; anim_index < num_anims; anim_index++) {
                    assert(sspine_anim_index_valid(state.skeleton, anim_index));
                    const sspine_anim_info info = sspine_get_anim_info(state.skeleton, anim_index);
                    igPushID_Int(anim_index);
                    if (igSelectable_Bool(info.name, state.ui.selected.anim_index == anim_index, 0, IMVEC2(0,0))) {
                        state.ui.selected.anim_index = anim_index;
                        sspine_set_animation(state.instance, 0, anim_index, true);
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_anim_index_valid(state.skeleton, state.ui.selected.anim_index)) {
                    const sspine_anim_info info = sspine_get_anim_info(state.skeleton, state.ui.selected.anim_index);
                    igBeginChild_Str("anim_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name);
                    igText("Duration: %.3f", info.duration);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    if (state.ui.events_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        if (igBegin("Events", &state.ui.events_open, 0)) {
            if (!sspine_skeleton_valid(state.skeleton)) {
                igText("No Spine data loaded");
            }
            else {
                const int num_events = sspine_num_events(state.skeleton);
                igText("Num Events: %d", num_events);
                igBeginChild_Str("event_list", IMVEC2(128, 0), true, 0);
                for (int event_index = 0; event_index < num_events; event_index++) {
                    assert(sspine_event_index_valid(state.skeleton, event_index));
                    const sspine_event_info info = sspine_get_event_info(state.skeleton, event_index);
                    igPushID_Int(event_index);
                    if (igSelectable_Bool(info.name, state.ui.selected.event_index == event_index, 0, IMVEC2(0,0))) {
                        state.ui.selected.event_index = event_index;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_event_index_valid(state.skeleton, state.ui.selected.event_index)) {
                    const sspine_event_info info = sspine_get_event_info(state.skeleton, state.ui.selected.event_index);
                    igBeginChild_Str("event_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name);
                    igText("Int Value: %d\n", info.int_value);
                    igText("Float Value: %.3f\n", info.float_value);
                    igText("String Value: %s", info.string_value ? info.string_value : "NONE");
                    igText("Audio Path: %s", info.audio_path ? info.audio_path : "NONE");
                    igText("Volume: %.3f", info.volume);
                    igText("Balance: %.3f", info.balance);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    if (state.ui.iktargets_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        if (igBegin("IK Targets", &state.ui.iktargets_open, 0)) {
            if (!sspine_skeleton_valid(state.skeleton)) {
                igText("No Spine data loaded");
            }
            else {
                const int num_iktargets = sspine_num_iktargets(state.skeleton);
                igText("Num IK Targets: %d", num_iktargets);
                igBeginChild_Str("iktarget_list", IMVEC2(128, 0), true, 0);
                for (int iktarget_index = 0; iktarget_index < num_iktargets; iktarget_index++) {
                    assert(sspine_iktarget_index_valid(state.skeleton, iktarget_index));
                    const sspine_iktarget_info info = sspine_get_iktarget_info(state.skeleton, iktarget_index);
                    igPushID_Int(iktarget_index);
                    if (igSelectable_Bool(info.name, state.ui.selected.iktarget_index == iktarget_index, 0, IMVEC2(0,0))) {
                        state.ui.selected.iktarget_index = iktarget_index;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_iktarget_index_valid(state.skeleton, state.ui.selected.iktarget_index)) {
                    const sspine_iktarget_info info = sspine_get_iktarget_info(state.skeleton, state.ui.selected.iktarget_index);
                    igBeginChild_Str("iktarget_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name);
                    igText("Target Bone Index: %d", info.target_bone_index);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    // display triggered events
    const double triggered_event_fade_time = 1.0;
    if ((state.ui.last_triggered_event.index != -1) && (state.ui.last_triggered_event.time + triggered_event_fade_time) > state.ui.cur_time) {
        const int event_index = state.ui.last_triggered_event.index;
        const char* event_name = sspine_get_event_info(state.skeleton, event_index).name;
        const double event_time = state.ui.last_triggered_event.time;
        if (event_name) {
            const float alpha = 1.0 - ((state.ui.cur_time - event_time) / triggered_event_fade_time);
            igSetNextWindowBgAlpha(alpha);
            igSetNextWindowPos(IMVEC2(sapp_widthf() * 0.5f, sapp_heightf() - 50.0f), ImGuiCond_Always, IMVEC2(0.5f,0.5f));
            igPushStyleColor_U32(ImGuiCol_WindowBg, 0xFF0000FF);
            if (igBegin("Triggered Events", 0, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav)) {
                igText("%s: %.3f (age: %.3f)", sspine_get_event_info(state.skeleton, event_index).name, event_time, state.ui.cur_time - event_time);
            }
            igEnd();
            igPopStyleColor(1);
        }
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
        .window_title = "spine-inspector-sapp.c",
        .icon.sokol_default = true,
    };
}
