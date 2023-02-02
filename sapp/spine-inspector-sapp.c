//------------------------------------------------------------------------------
//  spine-inspector-sapp.c
//  Spine sample with inspector UI.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "util/fileutil.h"
#define SOKOL_SPINE_IMPL
#include "spine/spine.h"
#include "sokol_spine.h"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#define SOKOL_IMGUI_IMPL
#define SOKOL_GFX_IMGUI_IMPL
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "sokol_imgui.h"
#include "sokol_gfx_imgui.h"
#include "stb/stb_image.h"
#include "dbgui/dbgui.h"

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
        sspine_range atlas_data;
        sspine_range skel_data;
        bool skel_data_is_binary;
    } load_status;
    struct {
        sg_imgui_t sgimgui;
        bool draw_bones_enabled;
        bool atlas_open;
        bool bones_open;
        bool slots_open;
        bool anims_open;
        bool events_open;
        bool skins_open;
        bool iktargets_open;
        struct {
            sspine_bone bone;
            sspine_slot slot;
            sspine_anim anim;
            sspine_event event;
            sspine_skin skin;
            sspine_iktarget iktarget;
        } selected;
        double cur_time;
        struct {
            double time;
            sspine_event event;
        } last_triggered_event;
    } ui;
    struct {
        uint8_t atlas[16 * 1024];
        uint8_t skeleton[512 * 1024];
        uint8_t image[512 * 1024];
    } buffers;
} state;

// describe Spine scenes available for loading
#define MAX_SPINE_SCENES (5)
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
    const char* skin;
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
    {
        .ui_name = "Mix & Match",
        .atlas_file = "mix-and-match-pma.atlas",
        .skel_file_binary = "mix-and-match-pro.skel",
        .skin = "full-skins/girl",
        .prescale = 0.5f,
        .anim_queue[0] = { .name = "walk", .looping = true },
    },
};

// helper functions
static bool load_spine_scene(int scene_index);
static void create_spine_objects(void);
static void atlas_data_loaded(const sfetch_response_t* response);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void image_data_loaded(const sfetch_response_t* response);
static void draw_bones(void);
static void ui_setup(void);
static void ui_shutdown(void);
static void ui_draw(void);

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    // setup sokol-gl
    sgl_setup(&(sgl_desc_t){ .logger.func = slog_func });
    // setup sokol-fetch for loading up to 2 files in parallel
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 3,
        .num_channels = 2,
        .num_lanes = 1,
        .logger.func = slog_func,
    });
    // setup sokol-spine with default attributes
    sspine_setup(&(sspine_desc){ .logger.func = slog_func });
    // setup UI libs
    ui_setup();
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

    // link IK target to mouse (no-op if there's no selected iktarget)
    sspine_set_iktarget_world_pos(state.instance, state.ui.selected.iktarget, state.iktarget_pos);

    // update instance animation and bone state
    sspine_update_instance(state.instance, (float)delta_time);

    // draw instance to layer 0
    sspine_draw_instance_in_layer(state.instance, 0);

    // keep track of triggered events
    const int num_triggered_events = sspine_num_triggered_events(state.instance);
    for (int i = 0; i < num_triggered_events; i++) {
        state.ui.last_triggered_event.time = state.ui.cur_time;
        state.ui.last_triggered_event.event = sspine_get_triggered_event_info(state.instance, i).event;
    }

    // draw spine bones via sokol-gl
    if (state.ui.draw_bones_enabled) {
        draw_bones();
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
    sgl_draw();
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
    ui_shutdown();
    sspine_shutdown();
    sfetch_shutdown();
    sgl_shutdown();
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
    state.load_status.atlas_data = (sspine_range){0};
    state.load_status.skel_data = (sspine_range){0};
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
        .buffer = SFETCH_RANGE(state.buffers.atlas),
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
        // in case the skeleton file is JSON text data, make sure we have room for a terminating zero
        .buffer = { .ptr = state.buffers.skeleton, .size = sizeof(state.buffers.skeleton) - 1 },
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
        state.load_status.atlas_data = (sspine_range){ response->data.ptr, response->data.size };
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (state.load_status.pending_count == 0) {
            create_spine_objects();
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
        state.load_status.skel_data = (sspine_range){ response->data.ptr, response->data.size };
        // in case the loaded data file is JSON text, make sure it's zero terminated
        assert(response->data.size < sizeof(state.buffers.skeleton));
        state.buffers.skeleton[response->data.size] = 0;
        // if both the atlas and skeleton file had been loaded, create
        // the atlas and skeleton spine objects
        if (state.load_status.pending_count == 0) {
            create_spine_objects();
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
    }
}

// called when both the Spine atlas and skeleton file has finished loading
static void create_spine_objects(void) {
    const int scene_index = state.load_status.scene_index;

    // create atlas from file data
    state.atlas = sspine_make_atlas(&(sspine_atlas_desc){
        .data = state.load_status.atlas_data,
        .override = spine_scenes[scene_index].atlas_overrides,
    });
    assert(sspine_atlas_valid(state.atlas));

    // create a skeleton object, the skeleton data might be either JSON or binary
    const char* skel_json_data = 0;
    sspine_range skel_binary_data = {0};
    if (state.load_status.skel_data_is_binary) {
        skel_binary_data = state.load_status.skel_data;
    }
    else {
        skel_json_data = (const char*)state.load_status.skel_data.ptr;
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

    // set initial skin if requested
    if (spine_scenes[scene_index].skin) {
        sspine_set_skin(state.instance, sspine_skin_by_name(state.skeleton, spine_scenes[scene_index].skin));
    }

    // populate animation queue
    assert((scene_index >= 0) && (scene_index < MAX_SPINE_SCENES));
    for (int anim_index = 0; anim_index < MAX_QUEUE_ANIMS; anim_index++) {
        const anim_t* queue_anim = &spine_scenes[scene_index].anim_queue[anim_index];
        if (queue_anim->name) {
            sspine_anim anim = sspine_anim_by_name(state.skeleton, queue_anim->name);
            if (anim_index == 0) {
                sspine_set_animation(state.instance, anim, 0, queue_anim->looping);
            }
            else {
                sspine_add_animation(state.instance, anim, 0, queue_anim->looping, queue_anim->delay);
            }
        }
    }

    // asynchronously load atlas images
    const int num_images = sspine_num_images(state.atlas);
    for (int img_index = 0; img_index < num_images; img_index++) {
        const sspine_image img = sspine_image_by_index(state.atlas, img_index);
        const sspine_image_info img_info = sspine_get_image_info(img);
        assert(img_info.valid);
        char path_buf[512];
        sfetch_send(&(sfetch_request_t){
            .channel = 0,
            .path = fileutil_get_path(img_info.filename.cstr, path_buf, sizeof(path_buf)),
            .buffer = SFETCH_RANGE(state.buffers.image),
            .callback = image_data_loaded,
            .user_data = SFETCH_RANGE(img),
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
    const sspine_image img = *(sspine_image*)response->user_data;
    const sspine_image_info img_info = sspine_get_image_info(img);
    assert(img_info.valid);
    if (response->fetched) {
        // decode image via stb_image.h
        const int desired_channels = 4;
        int img_width, img_height, num_channels;
        stbi_uc* pixels = stbi_load_from_memory(
            response->data.ptr,
            (int)response->data.size,
            &img_width, &img_height,
            &num_channels, desired_channels);
        if (pixels) {
            // sokol-spine has already allocated a sokol-gfx image handle for use,
            // now "populate" the handle with an actual image
            sg_init_image(img_info.sgimage, &(sg_image_desc){
                .width = img_width,
                .height = img_height,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = img_info.min_filter,
                .mag_filter = img_info.mag_filter,
                .label = img_info.filename.cstr,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(img_width * img_height * 4)
                }
            });
            stbi_image_free(pixels);
        }
        else {
            state.load_status.failed = false;
            sg_fail_image(img_info.sgimage);
        }
    }
    else if (response->failed) {
        state.load_status.failed = true;
        sg_fail_image(img_info.sgimage);
    }
}

//=== UI STUFF =================================================================
static void ui_setup(void) {
    simgui_setup(&(simgui_desc_t){0});
    sg_imgui_init(&state.ui.sgimgui, &(sg_imgui_desc_t){0});
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
            igMenuItem_BoolPtr("Draw Bones", 0, &state.ui.draw_bones_enabled, true);
            igMenuItem_BoolPtr("Atlas...", 0, &state.ui.atlas_open, true);
            igMenuItem_BoolPtr("Bones...", 0, &state.ui.bones_open, true);
            igMenuItem_BoolPtr("Slots...", 0, &state.ui.slots_open, true);
            igMenuItem_BoolPtr("Anims...", 0, &state.ui.anims_open, true);
            igMenuItem_BoolPtr("Events...", 0, &state.ui.events_open, true);
            igMenuItem_BoolPtr("Skins...", 0, &state.ui.skins_open, true);
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

    ImVec2 pos = IMVEC2(30, 30);
    if (state.ui.atlas_open) {
        igSetNextWindowSize(IMVEC2(300, 330), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("Spine Atlas", &state.ui.atlas_open, 0)) {
            if (!sspine_atlas_valid(state.atlas)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_atlas_pages = sspine_num_atlas_pages(state.atlas);
                igText("Num Pages: %d", num_atlas_pages);
                for (int i = 0; i < num_atlas_pages; i++) {
                    sspine_atlas_page_info info = sspine_get_atlas_page_info(sspine_atlas_page_by_index(state.atlas, i));
                    assert(info.valid);
                    igSeparator();
                    igText("Filename: %s", info.image.filename.cstr);
                    igText("Width: %d", info.image.width);
                    igText("Height: %d", info.image.height);
                    igText("Premul Alpha: %s", (info.image.premul_alpha == 0) ? "NO" : "YES");
                    igText("Original Spine params:");
                    igText("  Min Filter: %s", ui_sgfilter_name(info.image.min_filter));
                    igText("  Mag Filter: %s", ui_sgfilter_name(info.image.mag_filter));
                    igText("  Wrap U: %s", ui_sgwrap_name(info.image.wrap_u));
                    igText("  Wrap V: %s", ui_sgwrap_name(info.image.wrap_v));
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
    pos.x += 20; pos.y += 20;
    if (state.ui.bones_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("Bones", &state.ui.bones_open, 0)) {
            if (!sspine_instance_valid(state.instance)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_bones = sspine_num_bones(state.skeleton);
                igText("Num Bones: %d", num_bones);
                igBeginChild_Str("bones_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_bones; i++) {
                    const sspine_bone bone = sspine_bone_by_index(state.skeleton, i);
                    const sspine_bone_info info = sspine_get_bone_info(bone);
                    assert(info.valid);
                    igPushID_Int(bone.index);
                    if (igSelectable_Bool(info.name.cstr, sspine_bone_equal(state.ui.selected.bone, bone), 0, IMVEC2(0,0))) {
                        state.ui.selected.bone = bone;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_bone_valid(state.ui.selected.bone)) {
                    const sspine_bone_info info = sspine_get_bone_info(state.ui.selected.bone);
                    assert(info.valid);
                    igBeginChild_Str("bone_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Parent Bone: %s", sspine_bone_valid(info.parent_bone) ? sspine_get_bone_info(info.parent_bone).name.cstr : "---");
                    igText("Name: %s", info.name.cstr);
                    igText("Length: %.3f", info.length);
                    igText("Pose Transform:");
                    igText("  Position: %.3f,%.3f", info.pose.position.x, info.pose.position.y);
                    igText("  Rotation: %.3f", info.pose.rotation);
                    igText("  Scale: %.3f,%.3f", info.pose.scale.x, info.pose.scale.y);
                    igText("  Shear: %.3f,%.3f", info.pose.shear.x, info.pose.shear.y);
                    igText("Color: %.2f,%.2f,%.2f,%.2f", info.color.r, info.color.b, info.color.g, info.color.a);
                    igText("Current Transform:");
                    const sspine_bone_transform cur_tform = sspine_get_bone_transform(state.instance, state.ui.selected.bone);
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
    pos.x += 20; pos.y += 20;
    if (state.ui.slots_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("Slots", &state.ui.slots_open, 0)) {
            if (!sspine_instance_valid(state.instance)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_slots = sspine_num_slots(state.skeleton);
                igText("Num Slots: %d", num_slots);
                igBeginChild_Str("slot_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_slots; i++) {
                    const sspine_slot slot = sspine_slot_by_index(state.skeleton, i);
                    const sspine_slot_info info = sspine_get_slot_info(slot);
                    assert(info.valid);
                    igPushID_Int(slot.index);
                    if (igSelectable_Bool(info.name.cstr, sspine_slot_equal(state.ui.selected.slot, slot), 0, IMVEC2(0,0))) {
                        state.ui.selected.slot = slot;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_slot_valid(state.ui.selected.slot)) {
                    const sspine_slot_info slot_info = sspine_get_slot_info(state.ui.selected.slot);
                    assert(slot_info.valid);
                    const sspine_bone_info bone_info = sspine_get_bone_info(slot_info.bone);
                    assert(bone_info.valid);
                    igBeginChild_Str("slot_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", slot_info.index);
                    igText("Name: %s", slot_info.name.cstr);
                    igText("Attachment: %s", slot_info.attachment_name.valid ? slot_info.attachment_name.cstr : "-");
                    igText("Bone Name: %s", bone_info.name.cstr);
                    igText("Color: %.2f,%.2f,%.2f,%.2f", slot_info.color.r, slot_info.color.b, slot_info.color.g, slot_info.color.a);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    pos.x += 20; pos.y += 20;
    if (state.ui.anims_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("Anims", &state.ui.anims_open, 0)) {
            if (!sspine_instance_valid(state.instance)) {
                igText("No Spine data loaded.");
            }
            else {
                const int num_anims = sspine_num_anims(state.skeleton);
                igText("Num Anims: %d", num_anims);
                igBeginChild_Str("anim_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_anims; i++) {
                    const sspine_anim anim = sspine_anim_by_index(state.skeleton, i);
                    const sspine_anim_info info = sspine_get_anim_info(anim);
                    assert(info.valid);
                    igPushID_Int(anim.index);
                    if (igSelectable_Bool(info.name.cstr, sspine_anim_equal(state.ui.selected.anim, anim), 0, IMVEC2(0,0))) {
                        state.ui.selected.anim = anim;
                        sspine_set_animation(state.instance, anim, 0, true);
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_anim_valid(state.ui.selected.anim)) {
                    const sspine_anim_info info = sspine_get_anim_info(state.ui.selected.anim);
                    assert(info.valid);
                    igBeginChild_Str("anim_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name.cstr);
                    igText("Duration: %.3f", info.duration);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    pos.x += 20; pos.y += 20;
    if (state.ui.events_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("Events", &state.ui.events_open, 0)) {
            if (!sspine_skeleton_valid(state.skeleton)) {
                igText("No Spine data loaded");
            }
            else {
                const int num_events = sspine_num_events(state.skeleton);
                igText("Num Events: %d", num_events);
                igBeginChild_Str("event_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_events; i++) {
                    const sspine_event event = sspine_event_by_index(state.skeleton, i);
                    const sspine_event_info info = sspine_get_event_info(event);
                    assert(info.valid);
                    igPushID_Int(event.index);
                    if (igSelectable_Bool(info.name.cstr, sspine_event_equal(state.ui.selected.event, event), 0, IMVEC2(0,0))) {
                        state.ui.selected.event = event;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_event_valid(state.ui.selected.event)) {
                    const sspine_event_info info = sspine_get_event_info(state.ui.selected.event);
                    assert(info.valid);
                    igBeginChild_Str("event_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name.cstr);
                    igText("Int Value: %d\n", info.int_value);
                    igText("Float Value: %.3f\n", info.float_value);
                    igText("String Value: %s", info.string_value.valid ? info.string_value.cstr : "NONE");
                    igText("Audio Path: %s", info.audio_path.valid ? info.audio_path.cstr : "NONE");
                    igText("Volume: %.3f", info.volume);
                    igText("Balance: %.3f", info.balance);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    pos.x += 20; pos.y += 20;
    if (state.ui.skins_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("Skins", &state.ui.skins_open, 0)) {
            if (!sspine_skeleton_valid(state.skeleton)) {
                igText("No Spine data loaded");
            }
            else {
                const int num_skins = sspine_num_skins(state.skeleton);
                igText("Num Skins: %d", num_skins);
                igBeginChild_Str("skin_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_skins; i++) {
                    const sspine_skin skin = sspine_skin_by_index(state.skeleton, i);
                    const sspine_skin_info info = sspine_get_skin_info(skin);
                    assert(info.valid);
                    igPushID_Int(skin.index);
                    if (igSelectable_Bool(info.name.cstr, sspine_skin_equal(state.ui.selected.skin, skin), 0, IMVEC2(0,0))) {
                        state.ui.selected.skin = skin;
                        sspine_set_skin(state.instance, skin);
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_skin_valid(state.ui.selected.skin)) {
                    const sspine_skin_info info = sspine_get_skin_info(state.ui.selected.skin);
                    assert(info.valid);
                    igBeginChild_Str("skin_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name.cstr);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    pos.x += 20; pos.y += 20;
    if (state.ui.iktargets_open) {
        igSetNextWindowSize(IMVEC2(300, 300), ImGuiCond_Once);
        igSetNextWindowPos(pos, ImGuiCond_Once, IMVEC2(0,0));
        if (igBegin("IK Targets", &state.ui.iktargets_open, 0)) {
            if (!sspine_skeleton_valid(state.skeleton)) {
                igText("No Spine data loaded");
            }
            else {
                const int num_iktargets = sspine_num_iktargets(state.skeleton);
                igText("Num IK Targets: %d", num_iktargets);
                igBeginChild_Str("iktarget_list", IMVEC2(128, 0), true, 0);
                for (int i = 0; i < num_iktargets; i++) {
                    const sspine_iktarget iktarget = sspine_iktarget_by_index(state.skeleton, i);
                    const sspine_iktarget_info info = sspine_get_iktarget_info(iktarget);
                    assert(info.valid);
                    igPushID_Int(iktarget.index);
                    if (igSelectable_Bool(info.name.cstr, sspine_iktarget_equal(state.ui.selected.iktarget, iktarget), 0, IMVEC2(0,0))) {
                        state.ui.selected.iktarget = iktarget;
                    }
                    igPopID();
                }
                igEndChild();
                igSameLine(0, -1);
                if (sspine_iktarget_valid(state.ui.selected.iktarget)) {
                    const sspine_iktarget_info info = sspine_get_iktarget_info(state.ui.selected.iktarget);
                    assert(info.valid);
                    igBeginChild_Str("iktarget_info", IMVEC2(0,0), false, 0);
                    igText("Index: %d", info.index);
                    igText("Name: %s", info.name.cstr);
                    igText("Target Bone: %s", sspine_get_bone_info(info.target_bone).name.cstr);
                    igEndChild();
                }
            }
        }
        igEnd();
    }
    // display triggered events
    const double triggered_event_fade_time = 1.0;
    if (sspine_event_valid(state.ui.last_triggered_event.event) && (state.ui.last_triggered_event.time + triggered_event_fade_time) > state.ui.cur_time) {
        const sspine_event event = state.ui.last_triggered_event.event;
        const sspine_event_info event_info = sspine_get_event_info(event);
        const double event_time = state.ui.last_triggered_event.time;
        if (event_info.valid) {
            const float alpha = (float) (1.0 - ((state.ui.cur_time - event_time) / triggered_event_fade_time));
            igSetNextWindowBgAlpha(alpha);
            igSetNextWindowPos(IMVEC2(sapp_widthf() * 0.5f, sapp_heightf() - 50.0f), ImGuiCond_Always, IMVEC2(0.5f,0.5f));
            igPushStyleColor_U32(ImGuiCol_WindowBg, 0xFF0000FF);
            if (igBegin("Triggered Events", 0, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav)) {
                igText("%s: %.3f (age: %.3f)", event_info.name.cstr, event_time, state.ui.cur_time - event_time);
            }
            igEnd();
            igPopStyleColor(1);
        }
    }
    sg_imgui_draw(&state.ui.sgimgui);
}

static void draw_bones(void) {
    if (!sspine_instance_valid(state.instance)) {
        return;
    }
    const sspine_mat4 proj = sspine_layer_transform_to_mat4(&state.layer_transform);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_load_matrix(proj.m);
    sgl_c3f(0.0f, 1.0f, 0.0f);
    sgl_begin_lines();
    const int num_bones = sspine_num_bones(state.skeleton);
    for (int i = 0; i < num_bones; i++) {
        const sspine_bone bone = sspine_bone_by_index(state.skeleton, i);
        const sspine_bone parent_bone = sspine_get_bone_info(bone).parent_bone;
        if (sspine_bone_valid(parent_bone)) {
            const sspine_vec2 p0 = sspine_get_bone_world_position(state.instance, parent_bone);
            const sspine_vec2 p1 = sspine_get_bone_world_position(state.instance, bone);
            sgl_v2f(p0.x, p0.y);
            sgl_v2f(p1.x, p1.y);
        }
    }
    sgl_end();
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
        .sample_count = 4,
        .window_title = "spine-inspector-sapp.c",
        .icon.sokol_default = true,
    };
}
