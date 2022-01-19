//------------------------------------------------------------------------------
//  ozz-anim-sapp.cc
//
//  https://guillaumeblanc.github.io/ozz-animation/
//
//  Port of the ozz-animation "Animation Playback" sample. Use sokol-gl
//  for debug-rendering the animated character skeleton (no skinning).
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"

#define SOKOL_GL_IMPL
#include "sokol_gl.h"

#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "util/camera.h"

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

#include <memory>   // std::unique_ptr, std::make_unique
#include <cmath>    // fmodf

// wrapper struct for managed ozz-animation C++ objects, must be deleted
// before shutdown, otherwise ozz-animation will report a memory leak
typedef struct {
    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animation;
    ozz::animation::SamplingCache cache;
    ozz::vector<ozz::math::SoaTransform> local_matrices;
    ozz::vector<ozz::math::Float4x4> model_matrices;
} ozz_t;

static struct {
    std::unique_ptr<ozz_t> ozz;
    sg_pass_action pass_action;
    camera_t camera;
    struct {
        bool skeleton;
        bool animation;
        bool failed;
    } loaded;
    struct {
        double frame;
        double absolute;
        float factor;
        float anim_ratio;
        bool anim_ratio_ui_override;
        bool paused;
    } time;
} state;

// io buffers for skeleton and animation data files, we know the max file size upfront
static uint8_t skel_data_buffer[4 * 1024];
static uint8_t anim_data_buffer[32 * 1024];

static void eval_animation(void);
static void draw_skeleton(void);
static void draw_ui(void);
static void skeleton_data_loaded(const sfetch_response_t* response);
static void animation_data_loaded(const sfetch_response_t* response);

static void init(void) {
    state.ozz = std::make_unique<ozz_t>();
    state.time.factor = 1.0f;

    // setup sokol-gfx
    sg_desc sgdesc = { };
    sgdesc.context = sapp_sgcontext();
    sg_setup(&sgdesc);

    // setup sokol-fetch
    sfetch_desc_t sfdesc = { };
    sfdesc.max_requests = 2;
    sfdesc.num_channels = 1;
    sfdesc.num_lanes = 2;
    sfetch_setup(&sfdesc);

    // setup sokol-gl
    sgl_desc_t sgldesc = { };
    sgldesc.sample_count = sapp_sample_count();
    sgl_setup(&sgldesc);

    // setup sokol-imgui
    simgui_desc_t imdesc = { };
    simgui_setup(&imdesc);

    // initialize pass action for default-pass
    state.pass_action.colors[0].action = SG_ACTION_CLEAR;
    state.pass_action.colors[0].value = { 0.0f, 0.1f, 0.2f, 1.0f };

    // initialize camera helper
    camera_desc_t camdesc = { };
    camdesc.min_dist = 1.0f;
    camdesc.max_dist = 10.0f;
    camdesc.center.Y = 1.0f;
    camdesc.distance = 3.0f;
    camdesc.latitude = 10.0f;
    camdesc.longitude = 20.0f;
    cam_init(&state.camera, &camdesc);

    // start loading the skeleton and animation files
    {
        sfetch_request_t req = { };
        req.path = "ozz_anim_skeleton.ozz";
        req.callback = skeleton_data_loaded;
        req.buffer_ptr = skel_data_buffer;
        req.buffer_size = sizeof(skel_data_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = { };
        req.path = "ozz_anim_animation.ozz";
        req.callback = animation_data_loaded;
        req.buffer_ptr = anim_data_buffer;
        req.buffer_size = sizeof(anim_data_buffer);
        sfetch_send(&req);
    }
}

static void frame(void) {
    sfetch_dowork();

    const int fb_width = sapp_width();
    const int fb_height = sapp_height();
    state.time.frame = sapp_frame_duration();
    cam_update(&state.camera, fb_width, fb_height);

    simgui_new_frame({ fb_width, fb_height, state.time.frame, sapp_dpi_scale() });
    draw_ui();

    if (state.loaded.animation && state.loaded.skeleton) {
        if (!state.time.paused) {
            state.time.absolute += state.time.frame * state.time.factor;
        }
        eval_animation();
        draw_skeleton();
    }

    sg_begin_default_pass(&state.pass_action, fb_width, fb_height);
    sgl_draw();
    simgui_render();
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
    simgui_shutdown();
    sgl_shutdown();
    sfetch_shutdown();
    sg_shutdown();

    // free C++ objects early, otherwise ozz-animation complains about memory leaks
    state.ozz = nullptr;
}

static void eval_animation(void) {

    // convert current time to animation ration (0.0 .. 1.0)
    const float anim_duration = state.ozz->animation.duration();
    if (!state.time.anim_ratio_ui_override) {
        state.time.anim_ratio = fmodf((float)state.time.absolute / anim_duration, 1.0f);
    }

    // sample animation
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = &state.ozz->animation;
    sampling_job.cache = &state.ozz->cache;
    sampling_job.ratio = state.time.anim_ratio;
    sampling_job.output = make_span(state.ozz->local_matrices);
    sampling_job.Run();

    // convert joint matrices from local to model space
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &state.ozz->skeleton;
    ltm_job.input = make_span(state.ozz->local_matrices);
    ltm_job.output = make_span(state.ozz->model_matrices);
    ltm_job.Run();
}

static void draw_vec(const ozz::math::SimdFloat4& vec) {
    sgl_v3f(ozz::math::GetX(vec), ozz::math::GetY(vec), ozz::math::GetZ(vec));
}

static void draw_line(const ozz::math::SimdFloat4& v0, const ozz::math::SimdFloat4& v1) {
    draw_vec(v0);
    draw_vec(v1);
}

// this draws a wireframe 3d rhombus between the current and parent joints
static void draw_joint(int joint_index, int parent_joint_index) {
    if (parent_joint_index < 0) {
        return;
    }

    using namespace ozz::math;

    const Float4x4& m0 = state.ozz->model_matrices[joint_index];
    const Float4x4& m1 = state.ozz->model_matrices[parent_joint_index];

    const SimdFloat4 p0 = m0.cols[3];
    const SimdFloat4 p1 = m1.cols[3];
    const SimdFloat4 ny = m1.cols[1];
    const SimdFloat4 nz = m1.cols[2];

    const SimdFloat4 len = SplatX(Length3(p1 - p0)) * simd_float4::Load1(0.1f);

    const SimdFloat4 pmid = p0 + (p1 - p0) * simd_float4::Load1(0.66f);
    const SimdFloat4 p2 = pmid + ny * len;
    const SimdFloat4 p3 = pmid + nz * len;
    const SimdFloat4 p4 = pmid - ny * len;
    const SimdFloat4 p5 = pmid - nz * len;

    sgl_c3f(1.0f, 1.0f, 0.0f);
    draw_line(p0, p2); draw_line(p0, p3); draw_line(p0, p4); draw_line(p0, p5);
    draw_line(p1, p2); draw_line(p1, p3); draw_line(p1, p4); draw_line(p1, p5);
    draw_line(p2, p3); draw_line(p3, p4); draw_line(p4, p5); draw_line(p5, p2);
}

static void draw_skeleton(void) {
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_load_matrix((const float*)&state.camera.proj);
    sgl_matrix_mode_modelview();
    sgl_load_matrix((const float*)&state.camera.view);

    const int num_joints = state.ozz->skeleton.num_joints();
    ozz::span<const int16_t> joint_parents = state.ozz->skeleton.joint_parents();
    sgl_begin_lines();
    for (int joint_index = 0; joint_index < num_joints; joint_index++) {
        draw_joint(joint_index, joint_parents[joint_index]);
    }
    sgl_end();
}

static void draw_ui(void) {
    ImGui::SetNextWindowPos({ 20, 20 }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ 220, 150 }, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.loaded.failed) {
            ImGui::Text("Failed loading character data!");
        }
        else {
            ImGui::Text("Camera Controls:");
            ImGui::Text("  LMB + Mouse Move: Look");
            ImGui::Text("  Mouse Wheel: Zoom");
            ImGui::SliderFloat("Distance", &state.camera.distance, state.camera.min_dist, state.camera.max_dist, "%.1f", 1.0f);
            ImGui::SliderFloat("Latitude", &state.camera.latitude, state.camera.min_lat, state.camera.max_lat, "%.1f", 1.0f);
            ImGui::SliderFloat("Longitude", &state.camera.longitude, 0.0f, 360.0f, "%.1f", 1.0f);
            ImGui::Separator();
            ImGui::Text("Time Controls:");
            ImGui::Checkbox("Paused", &state.time.paused);
            ImGui::SliderFloat("Factor", &state.time.factor, 0.0f, 10.0f, "%.1f", 1.0f);
            if (ImGui::SliderFloat("Ratio", &state.time.anim_ratio, 0.0f, 1.0f)) {
                state.time.anim_ratio_ui_override = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                state.time.anim_ratio_ui_override = false;
            }
        }
    }
    ImGui::End();
}

static void skeleton_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        // NOTE: if we derived our own ozz::io::Stream class we could
        // avoid the extra allocation and memory copy that happens
        // with the standard MemoryStream class
        ozz::io::MemoryStream stream;
        stream.Write(response->buffer_ptr, response->fetched_size);
        stream.Seek(0, ozz::io::Stream::kSet);
        ozz::io::IArchive archive(&stream);
        if (archive.TestTag<ozz::animation::Skeleton>()) {
            archive >> state.ozz->skeleton;
            state.loaded.skeleton = true;
            const int num_soa_joints = state.ozz->skeleton.num_soa_joints();
            const int num_joints = state.ozz->skeleton.num_joints();
            state.ozz->local_matrices.resize(num_soa_joints);
            state.ozz->model_matrices.resize(num_joints);
            state.ozz->cache.Resize(num_joints);
        }
        else {
            state.loaded.failed = true;
        }
    }
    else if (response->failed) {
        state.loaded.failed = true;
    }
}

static void animation_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz::io::MemoryStream stream;
        stream.Write(response->buffer_ptr, response->fetched_size);
        stream.Seek(0, ozz::io::Stream::kSet);
        ozz::io::IArchive archive(&stream);
        if (archive.TestTag<ozz::animation::Animation>()) {
            archive >> state.ozz->animation;
            state.loaded.animation = true;
        }
        else {
            state.loaded.failed = true;
        }
    }
    else if (response->failed) {
        state.loaded.failed = true;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 800;
    desc.height = 600;
    desc.sample_count = 4;
    desc.window_title = "ozz-anim-sapp.cc";
    desc.icon.sokol_default = true;

    return desc;
}
