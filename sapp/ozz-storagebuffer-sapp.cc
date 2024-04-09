//------------------------------------------------------------------------------
//  ozz-storagebuffer-sapp.c
//
//  ozz-animation sample which pulls vertices, model matrices
//  and joint matrices from storage buffers.
//
//  This is a modified clone of the ozz-skin-sapp sample.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "sokol_glue.h"

#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol_gfx_imgui.h"

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "util/camera.h"
#include "util/fileutil.h"

#include "ozz-storagebuffer-sapp.glsl.h"

// ozz-animation headers
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/util/mesh.h"

#include <memory>   // std::unique_ptr, std::make_unique
#include <cmath>    // fmodf
#include <assert.h>

// the upper limit for joint palette size is 256 (because the mesh joint indices
// are stored in packed byte-size vertex formats), but the example mesh only needs less than 64
#define MAX_JOINTS (64)

// this defines the size of the instance-buffer and height of the joint-texture
#define MAX_INSTANCES (512)

// wrapper struct for managed ozz-animation C++ objects, must be deleted
// before shutdown, otherwise ozz-animation will report a memory leak
typedef struct {
    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animation;
    ozz::vector<uint16_t> joint_remaps;
    ozz::vector<ozz::math::Float4x4> mesh_inverse_bindposes;
    ozz::vector<ozz::math::SoaTransform> local_matrices;
    ozz::vector<ozz::math::Float4x4> model_matrices;
    ozz::animation::SamplingCache cache;
} ozz_t;

static struct {
    std::unique_ptr<ozz_t> ozz;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    int num_instances;          // current number of character instances
    int num_triangle_indices;
    int num_skeleton_joints;    // number of joints in the skeleton
    int num_skin_joints;        // number of joints actually used by skinned mesh
    camera_t camera;
    bool draw_enabled;
    struct {
        bool skeleton;
        bool animation;
        bool mesh;
        bool failed;
    } loaded;
    struct {
        double frame_time_ms;
        double frame_time_sec;
        double abs_time_sec;
        uint64_t anim_eval_time;
        float factor;
        bool paused;
    } time;
    struct {
        sgimgui_t sgimgui;
    } ui;
} state;

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_io_buffer[32 * 1024];
static uint8_t anim_io_buffer[96 * 1024];
static uint8_t mesh_io_buffer[3 * 1024 * 1024];

// instance data buffer;
static sb_instance_t instance_data[MAX_INSTANCES];

// joint-matrix upload buffer, each joint consists of transposed 4x3 matrix
static sb_joint_t joint_upload_buffer[MAX_INSTANCES][MAX_PALETTE_JOINTS];

static void init_instances(void);
static void update_instances(void);
static void update_joints(void);
static void draw_ui(void);
static bool draw_ok(void);
static void skel_data_loaded(const sfetch_response_t* respone);
static void anim_data_loaded(const sfetch_response_t* respone);
static void mesh_data_loaded(const sfetch_response_t* respone);

static void init(void) {
    state.ozz = std::make_unique<ozz_t>();
    state.num_instances = 1;
    state.draw_enabled = true;
    state.time.factor = 1.0f;

    // setup sokol-gfx
    sg_desc sgdesc = {};
    sgdesc.environment = sglue_environment();
    sgdesc.logger.func = slog_func;
    sg_setup(&sgdesc);

    // setup sokol-time
    stm_setup();

    // setup sokol-imgui
    simgui_desc_t imdesc = {};
    imdesc.logger.func = slog_func;
    simgui_setup(&imdesc);
    sgimgui_desc_t sgimgui_desc = {};
    sgimgui_init(&state.ui.sgimgui, &sgimgui_desc);

    // initialize pass action for default pass
    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.0f, 0.1f, 0.2f, 1.0f };

    // initialize camera controller
    camera_desc_t camdesc = {};
    camdesc.min_dist = 2.0f;
    camdesc.max_dist = 40.0f;
    camdesc.center.Y = 1.1f;
    camdesc.distance = 3.0f;
    camdesc.latitude = 20.0f;
    camdesc.longitude = 20.0f;
    cam_init(&state.camera, &camdesc);

    // if storage-buffers are not supported, bail out here
    if (!sg_query_features().storage_buffer) {
        return;
    }

    // setup sokol-fetch
    sfetch_desc_t sfdesc = {};
    sfdesc.max_requests = 3;
    sfdesc.num_channels = 1;
    sfdesc.num_lanes = 3;
    sfdesc.logger.func = slog_func;
    sfetch_setup(&sfdesc);

    // initial instance model matrices
    init_instances();

    // shader and pipeline object for vertex-skinned rendering, note that
    // there's no vertex layout since all data needed for rendering
    // is pulled from storage buffers
    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = sg_make_shader(skinned_shader_desc(sg_query_backend()));
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.face_winding = SG_FACEWINDING_CCW;
    pip_desc.depth.write_enabled = true;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    state.pip = sg_make_pipeline(&pip_desc);

    // create a dynamic storage buffer which holds the per-instance model-matrices
    {
        sg_buffer_desc buf_desc = {};
        buf_desc.type = SG_BUFFERTYPE_STORAGEBUFFER;
        buf_desc.usage = SG_USAGE_STREAM;
        buf_desc.size = MAX_INSTANCES * sizeof(sb_instance_t);
        state.bind.vs.storage_buffers[SLOT_instances] = sg_make_buffer(&buf_desc);
    }

    // create another dynamic storage buffer which holds the joint matrices
    {
        sg_buffer_desc buf_desc = {};
        buf_desc.type = SG_BUFFERTYPE_STORAGEBUFFER;
        buf_desc.usage = SG_USAGE_STREAM;
        buf_desc.size = MAX_INSTANCES * MAX_JOINTS * sizeof(sb_joint_t);
        state.bind.vs.storage_buffers[SLOT_joints] = sg_make_buffer(&joint_buf_desc);
    }

    // NOTE: the buffers for vertices and indices are created in the async fetch callbacks

    // start loading data
    char path_buf[512];
    {
        sfetch_request_t req = {};
        req.path = fileutil_get_path("ozz_skin_skeleton.ozz", path_buf, sizeof(path_buf));
        req.callback = skel_data_loaded;
        req.buffer = SFETCH_RANGE(skel_io_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = {};
        req.path = fileutil_get_path("ozz_skin_animation.ozz", path_buf, sizeof(path_buf));
        req.callback = anim_data_loaded;
        req.buffer = SFETCH_RANGE(anim_io_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = {};
        req.path = fileutil_get_path("ozz_skin_mesh.ozz", path_buf, sizeof(path_buf));
        req.callback = mesh_data_loaded;
        req.buffer = SFETCH_RANGE(mesh_io_buffer);
        sfetch_send(&req);
    }
}

static void frame(void) {
    sfetch_dowork();

    const int fb_width = sapp_width();
    const int fb_height = sapp_height();
    state.time.frame_time_sec = sapp_frame_duration();
    state.time.frame_time_ms = sapp_frame_duration() * 1000.0;
    if (!state.time.paused) {
        state.time.abs_time_sec += state.time.frame_time_sec * state.time.factor;
    }
    cam_update(&state.camera, fb_width, fb_height);
    simgui_new_frame({ fb_width, fb_height, state.time.frame_time_sec, sapp_dpi_scale() });
    draw_ui();

    // update instance and joint storage buffers
    if (draw_ok()) {
        update_instances();
        update_joints();
    }

    // finally render everything in a single draw call
    sg_pass pass = {};
    pass.action = state.pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    if (draw_ok()) {
        vs_params_t vs_params = {};
        vs_params.view_proj = state.camera.view_proj;
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
        sg_draw(0, state.num_triangle_indices, state.num_instances);
    }
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* ev) {
    if (simgui_handle_event(ev)) {
        return;
    }
    cam_handle_event(&state.camera, ev);
}

static void cleanup(void) {
    sgimgui_discard(&state.ui.sgimgui);
    simgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
    state.ozz = nullptr;
}

static bool draw_ok(void) {
    return sg_features().storage_buffer
        && state.loaded.animation
        && state.loaded.mesh
        && state.loaded.skeleton;
}

// arrange the character instances into a quad
static void init_instances(void) {
    // initialize the character instance model-to-world matrices
    for (int i=0, x=0, y=0, dx=0, dy=0; i < MAX_INSTANCES; i++, x+=dx, y+=dy) {
        sb_instance_t* inst = &instance_data[i];

        // a 3x4 transposed model-to-world matrix (only the x/z position is set)
        inst->xxxx.X=1.0f; inst->xxxx.Y=0.0f; inst->xxxx.Z=0.0f; inst->xxxx.W=(float)x * 1.5f;
        inst->yyyy.X=0.0f; inst->yyyy.Y=1.0f; inst->yyyy.Z=0.0f; inst->yyyy.W=0.0f;
        inst->zzzz.X=0.0f; inst->zzzz.Y=0.0f; inst->zzzz.Z=1.0f; inst->zzzz.W=(float)y * 1.5f;

        // at a corner?
        if (abs(x) == abs(y)) {
            if (x >= 0) {
                // top-right corner: start a new ring
                if (y >= 0) { x+=1; y+=1; dx=0; dy=-1; }
                // bottom-right corner
                else { dx=-1; dy=0; }
            }
            else {
                // top-left corner
                if (y >= 0) { dx=+1; dy=0; }
                // bottom-left corner
                else { dx=0; dy=+1; }
            }
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 800;
    desc.height = 600;
    desc.sample_count = 4;
    desc.window_title = "ozz-storagebuffer-sapp.cc",
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}