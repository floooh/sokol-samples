//------------------------------------------------------------------------------
//  ozz-skin-sapp.c
//
//  Ozz-animation with GPU skinning.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_fetch.h"
#include "sokol_time.h"
#include "sokol_glue.h"

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
#include "ozz/util/mesh.h"

#include <memory>   // std::unique_ptr, std::make_unique
#include <cmath>    // fmodf

// wrapper struct for managed ozz-animation C++ objects, must be deleted
// before shutdown, otherwise ozz-animation will report a memory leak
typedef struct {
    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animation;
    ozz::animation::SamplingCache cache;
    ozz::vector<ozz::math::SoaTransform> locals;
    ozz::vector<ozz::math::Float4x4> models;
} ozz_t;

// FIXME: better packing
typedef struct {
    float px, py, pz;
    float nx, ny, nz;
    uint8_t joint_indices[4];
    float joint_weights[4];
} vertex_t;

static struct {
    std::unique_ptr<ozz_t> ozz;
    sg_pass_action pass_action;
    camera_t camera;
    sg_pipeline pip;
    sg_buffer vbuf;
    sg_buffer ibuf;
    struct {
        bool skeleton;
        bool animation;
        bool mesh;
        bool failed;
    } loaded;
    struct {
        double frame;
        double absolute;
        uint64_t laptime;
        float factor;
        float anim_ratio;
        bool anim_ratio_ui_override;
        bool paused;
    } time;
} state;

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_data_buffer[32 * 1024];
static uint8_t anim_data_buffer[96 * 1024];
static uint8_t mesh_data_buffer[3 * 2024 * 1024];

static void draw_ui(void);
static void skel_data_loaded(const sfetch_response_t* respone);
static void anim_data_loaded(const sfetch_response_t* respone);
static void mesh_data_loaded(const sfetch_response_t* respone);

static void init(void) {
    state.ozz = std::make_unique<ozz_t>();
    state.time.factor = 1.0f;

    // setup sokol-gfx
    sg_desc sgdesc = { };
    sgdesc.context = sapp_sgcontext();
    sg_setup(&sgdesc);

    // setup sokol-time
    stm_setup();

    // setup sokol-fetch
    sfetch_desc_t sfdesc = { };
    sfdesc.max_requests = 3;
    sfdesc.num_channels = 1;
    sfdesc.num_lanes = 3;
    sfetch_setup(&sfdesc);

    // setup sokol-imgui
    simgui_desc_t imdesc = { };
    simgui_setup(&imdesc);

    // initialize pass action for default-pass
    state.pass_action.colors[0].action = SG_ACTION_CLEAR;
    state.pass_action.colors[0].value = { 0.0f, 0.0f, 0.0f, 1.0f };

    // initialize camera helper
    camera_desc_t camdesc = { };
    camdesc.min_dist = 1.0f;
    camdesc.max_dist = 10.0f;
    camdesc.center.Y = 1.0f;
    camdesc.distance = 3.0f;
    camdesc.latitude = 10.0f;
    camdesc.longitude = 20.0f;
    cam_init(&state.camera, &camdesc);

    // start loading data
    {
        sfetch_request_t req = { };
        req.path = "ozz_skin_skeleton.ozz";
        req.callback = skel_data_loaded;
        req.buffer_ptr = skel_data_buffer;
        req.buffer_size = sizeof(skel_data_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = { };
        req.path = "ozz_skin_animation.ozz";
        req.callback = anim_data_loaded;
        req.buffer_ptr = anim_data_buffer;
        req.buffer_size = sizeof(anim_data_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = { };
        req.path = "ozz_skin_mesh.ozz";
        req.callback = mesh_data_loaded;
        req.buffer_ptr = mesh_data_buffer;
        req.buffer_size = sizeof(mesh_data_buffer);
        sfetch_send(&req);
    }
}

static void frame(void) {
    sfetch_dowork();

    const int fb_width = sapp_width();
    const int fb_height = sapp_height();
    state.time.frame = stm_sec(stm_round_to_common_refresh_rate(stm_laptime(&state.time.laptime)));
    cam_update(&state.camera, fb_width, fb_height);

    simgui_new_frame(fb_width, fb_height, state.time.frame);
    draw_ui();

    sg_begin_default_pass(&state.pass_action, fb_width, fb_height);
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
    sfetch_shutdown();
    sg_shutdown();

    // free C++ objects early, other ozz-animation complains about memory leaks
    state.ozz = nullptr;
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

// FIXME: all loading code is much less efficient than it should be!
static void skel_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz::io::MemoryStream stream;
        stream.Write(response->buffer_ptr, response->fetched_size);
        stream.Seek(0, ozz::io::Stream::kSet);
        ozz::io::IArchive archive(&stream);
        if (archive.TestTag<ozz::animation::Skeleton>()) {
            archive >> state.ozz->skeleton;
            state.loaded.skeleton = true;
            const int num_soa_joints = state.ozz->skeleton.num_soa_joints();
            const int num_joints = state.ozz->skeleton.num_joints();
            state.ozz->locals.resize(num_soa_joints);
            state.ozz->models.resize(num_joints);
            state.ozz->cache.Resize(num_joints);
        }
        else {
            state.loaded.failed = true;
        }
    }
    else {
        state.loaded.failed = true;
    }
}

static void anim_data_loaded(const sfetch_response_t* response) {
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
    else {
        state.loaded.failed = true;
    }
}

static void mesh_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz::io::MemoryStream stream;
        stream.Write(response->buffer_ptr, response->fetched_size);
        stream.Seek(0, ozz::io::Stream::kSet);

        ozz::vector<ozz::sample::Mesh> meshes;
        ozz::io::IArchive archive(&stream);
        while (archive.TestTag<ozz::sample::Mesh>()) {
            meshes.resize(meshes.size() + 1);
            archive >> meshes.back();
        }
        // assume one mesh and one submesh
        assert((meshes.size() == 1) && (meshes[0].parts.size() == 1));
        state.loaded.mesh = true;

        const size_t num_vertices = (int) (meshes[0].parts[0].positions.size() / 3);
        assert(meshes[0].parts[0].normals.size() == (num_vertices * 3));
        assert(meshes[0].parts[0].joint_indices.size() == (num_vertices * 4));
        assert(meshes[0].parts[0].joint_weights.size() == (num_vertices * 3));
        const float* positions = &meshes[0].parts[0].positions[0];
        const float* normals = &meshes[0].parts[0].normals[0];
        const uint16_t* joint_indices = &meshes[0].parts[0].joint_indices[0];
        const float* joint_weights = &meshes[0].parts[0].joint_weights[0];

        vertex_t* vertices = (vertex_t*) calloc(num_vertices, sizeof(vertex_t));
        for (size_t i = 0; i < num_vertices; i++) {
            vertex_t* v = &vertices[i];
            v->px = positions[i * 3 + 0];
            v->py = positions[i * 3 + 1];
            v->pz = positions[i * 3 + 2];
            v->nz = normals[i * 3 + 0];
            v->ny = normals[i * 3 + 1];
            v->nz = normals[i * 3 + 2];
            v->joint_indices[0] = (uint8_t) joint_indices[i * 4 + 0];
            v->joint_indices[1] = (uint8_t) joint_indices[i * 4 + 1];
            v->joint_indices[2] = (uint8_t) joint_indices[i * 4 + 2];
            v->joint_indices[3] = (uint8_t) joint_indices[i * 4 + 3];
            v->joint_weights[0] = joint_weights[i * 3 + 0];
            v->joint_weights[1] = joint_weights[i * 3 + 1];
            v->joint_weights[2] = joint_weights[i * 3 + 2];
            v->joint_weights[3] = 1.0f - (v->joint_weights[0] + v->joint_weights[1] + v->joint_weights[2]);
        }

        sg_buffer_desc vbuf_desc = { };
        vbuf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
        vbuf_desc.data.ptr = vertices;
        vbuf_desc.data.size = num_vertices * sizeof(vertices);
        state.vbuf = sg_make_buffer(&vbuf_desc);
        free(vertices); vertices = nullptr;

        sg_buffer_desc ibuf_desc = { };
        ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
        ibuf_desc.data.ptr = &meshes[0].triangle_indices[0];
        ibuf_desc.data.size = meshes[0].triangle_indices.size() * sizeof(uint16_t);
        state.ibuf = sg_make_buffer(&ibuf_desc);
    }
    else {
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
    desc.window_title = "ozz-skin-sapp.cc";
    desc.icon.sokol_default = true;

    return desc;
}
