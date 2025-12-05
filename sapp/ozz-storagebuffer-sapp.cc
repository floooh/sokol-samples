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

#define VECMATH_GENERICS
#include "vecmath/vecmath.h"
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
#include "framework/mesh.h"

#include <memory>   // std::unique_ptr, std::make_unique
#include <cmath>    // fmodf
#include <assert.h>

// the upper limit for joint palette size is 256 (because the mesh joint indices
// are stored in packed byte-size vertex formats), but the example mesh only needs less than 64
#define MAX_JOINTS (64)

// the max number of character instances we're going to render
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
    ozz::animation::SamplingJob::Context context;
} ozz_t;

static struct {
    std::unique_ptr<ozz_t> ozz;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_buffer instance_buf;
    sg_buffer joint_buf;
    sg_buffer vertex_buf;
    sg_bindings bind;
    int num_instances;          // current number of character instances
    int num_triangle_indices;
    int num_skeleton_joints;    // number of joints in the skeleton
    int num_skin_joints;        // number of joints actually used by skinned mesh
    camera_t camera;
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
} state;

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_io_buffer[32 * 1024];
static uint8_t anim_io_buffer[96 * 1024];
static uint8_t mesh_io_buffer[3 * 1024 * 1024];

// per-instance-data
static sb_instance_t instance_data[MAX_INSTANCES];

// joint-matrix data for all character instances, each joint consists of transposed 4x3 matrix
static sb_joint_t joint_upload_buffer[MAX_INSTANCES][MAX_JOINTS];

static void init_instances(void);
static void update_joints(void);
static void draw_ui(void);
static bool draw_ok(void);
static void skel_data_loaded(const sfetch_response_t* respone);
static void anim_data_loaded(const sfetch_response_t* respone);
static void mesh_data_loaded(const sfetch_response_t* respone);

static void init(void) {
    state.ozz = std::make_unique<ozz_t>();
    state.num_instances = 1;
    state.time.factor = 1.0f;

    // setup sokol-gfx
    sg_desc sgdesc = {};
    sgdesc.environment = sglue_environment();
    sgdesc.logger.func = slog_func;
    sg_setup(&sgdesc);

    // setup sokol-time
    stm_setup();

    // setup sokol-imgui
    sgimgui_desc_t sgimgui_desc = {};
    sgimgui_setup(&sgimgui_desc);
    simgui_desc_t imdesc = {};
    imdesc.logger.func = slog_func;
    simgui_setup(&imdesc);

    // initialize pass action for default pass
    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.0f, 0.1f, 0.2f, 1.0f };

    // initialize camera controller
    camera_desc_t camdesc = {};
    camdesc.min_dist = 2.0f;
    camdesc.max_dist = 40.0f;
    camdesc.center.y = 1.1f;
    camdesc.distance = 3.0f;
    camdesc.latitude = 20.0f;
    camdesc.longitude = 20.0f;
    cam_init(&state.camera, &camdesc);

    // setup sokol-fetch
    sfetch_desc_t sfdesc = {};
    sfdesc.max_requests = 3;
    sfdesc.num_channels = 1;
    sfdesc.num_lanes = 3;
    sfdesc.logger.func = slog_func;
    sfetch_setup(&sfdesc);

    // if storage-buffers are not supported, bail out here
    if (!sg_query_features().compute) {
        return;
    }

    // shader and pipeline object for vertex-skinned rendering, note that
    // there's no vertex layout since all data needed for rendering
    // is pulled from storage buffers
    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = sg_make_shader(skinned_shader_desc(sg_query_backend()));
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.face_winding = SG_FACEWINDING_CCW;
    pip_desc.depth.write_enabled = true;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.label = "pipeline";
    state.pip = sg_make_pipeline(&pip_desc);

    // create a storage buffer and view which holds the per-instance model-matrices,
    // the model matrices never change, so we can put them into an immutable buffer
    {
        init_instances();
        sg_buffer_desc buf_desc = {};
        buf_desc.usage.storage_buffer = true;
        buf_desc.data = SG_RANGE(instance_data);
        buf_desc.label = "instances";
        state.instance_buf = sg_make_buffer(&buf_desc);
        sg_view_desc view_desc = {};
        view_desc.storage_buffer.buffer = state.instance_buf;
        view_desc.label = "instances-view";
        state.bind.views[VIEW_instances] = sg_make_view(&view_desc);
    }

    // create another dynamic storage buffer and view which receives the animated joint matrices
    {
        sg_buffer_desc buf_desc = {};
        buf_desc.usage.storage_buffer = true;
        buf_desc.usage.stream_update = true;
        buf_desc.size = MAX_INSTANCES * MAX_JOINTS * sizeof(sb_joint_t);
        buf_desc.label = "joints";
        state.joint_buf = sg_make_buffer(&buf_desc);
        sg_view_desc view_desc = {};
        view_desc.storage_buffer.buffer = state.joint_buf;
        view_desc.label = "joints-view";
        state.bind.views[VIEW_joints] = sg_make_view(&view_desc);
    }

    // NOTE: the storage buffers for vertices and indices are created in the async fetch callbacks

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
        sg_apply_uniforms(UB_vs_params, SG_RANGE_REF(vs_params));
        sg_draw(0, state.num_triangle_indices, state.num_instances);
    }
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
    sgimgui_shutdown();
    simgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
    state.ozz = nullptr;
}

static bool draw_ok(void) {
    return sg_query_features().compute
        && state.loaded.animation
        && state.loaded.mesh
        && state.loaded.skeleton;
}

// compute skinning matrices and upload into joint storage buffer
static void update_joints(void) {
    uint64_t start_time = stm_now();
    const float anim_duration = state.ozz->animation.duration();
    for (int instance = 0; instance < state.num_instances; instance++) {

        // each character instance evaluates its own animation
        const float anim_ratio = fmodf(((float)state.time.abs_time_sec + (instance*0.1f)) / anim_duration, 1.0f);

        // sample animation
        // NOTE: using one cache per instance versus one cache per animation makes a small difference, but not much
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = &state.ozz->animation;
        sampling_job.context = &state.ozz->context;
        sampling_job.ratio = anim_ratio;
        sampling_job.output = make_span(state.ozz->local_matrices);
        sampling_job.Run();

        // convert joint matrices from local to model space
        ozz::animation::LocalToModelJob ltm_job;
        ltm_job.skeleton = &state.ozz->skeleton;
        ltm_job.input = make_span(state.ozz->local_matrices);
        ltm_job.output = make_span(state.ozz->model_matrices);
        ltm_job.Run();

        // compute transposed skinning matrices, and copy the xxxx,yyyy,zzzz vectors into the intermediate joint buffer
        for (int i = 0; i < state.num_skin_joints; i++) {
            const ozz::math::Float4x4 skin_matrix = state.ozz->model_matrices[state.ozz->joint_remaps[i]] * state.ozz->mesh_inverse_bindposes[i];
            const ozz::math::Float4x4 transposed = ozz::math::Transpose(skin_matrix);
            memcpy(&joint_upload_buffer[instance][i], &transposed.cols[0], 3 * sizeof(vec4_t));
        }
    }
    state.time.anim_eval_time = stm_since(start_time);

    // update the sokol-gfx joint storage buffer
    sg_update_buffer(state.joint_buf, SG_RANGE(joint_upload_buffer));
}

// arrange the character instances into a quad
static void init_instances(void) {
    // initialize the character instance model-to-world matrices
    for (int i=0, x=0, y=0, dx=0, dy=0; i < MAX_INSTANCES; i++, x+=dx, y+=dy) {
        instance_data[i].model = mat44_translation((float)x * 1.5f, 0.0f, (float)y * 1.5f);
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

// sokol-fetch io callback
static void skel_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz::io::MemoryStream stream;
        stream.Write(response->data.ptr, response->data.size);
        stream.Seek(0, ozz::io::Stream::kSet);
        ozz::io::IArchive archive(&stream);
        if (archive.TestTag<ozz::animation::Skeleton>()) {
            archive >> state.ozz->skeleton;
            state.loaded.skeleton = true;
            const int num_soa_joints = state.ozz->skeleton.num_soa_joints();
            const int num_joints = state.ozz->skeleton.num_joints();
            state.ozz->local_matrices.resize(num_soa_joints);
            state.ozz->model_matrices.resize(num_joints);
            state.num_skeleton_joints = num_joints;
            state.ozz->context.Resize(num_joints);
        }
        else {
            state.loaded.failed = true;
        }
    }
    else if (response->failed) {
        state.loaded.failed = true;
    }
}

static void anim_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz::io::MemoryStream stream;
        stream.Write(response->data.ptr, response->data.size);
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

static uint32_t pack_u32(uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
    return (uint32_t)(((uint32_t)w<<24)|((uint32_t)z<<16)|((uint32_t)y<<8)|x);
}

static uint32_t pack_f4_byte4n(float x, float y, float z, float w) {
    int8_t x8 = (int8_t) (x * 127.0f);
    int8_t y8 = (int8_t) (y * 127.0f);
    int8_t z8 = (int8_t) (z * 127.0f);
    int8_t w8 = (int8_t) (w * 127.0f);
    return pack_u32((uint8_t)x8, (uint8_t)y8, (uint8_t)z8, (uint8_t)w8);
}

static uint32_t pack_f4_ubyte4n(float x, float y, float z, float w) {
    uint8_t x8 = (uint8_t) (x * 255.0f);
    uint8_t y8 = (uint8_t) (y * 255.0f);
    uint8_t z8 = (uint8_t) (z * 255.0f);
    uint8_t w8 = (uint8_t) (w * 255.0f);
    return pack_u32(x8, y8, z8, w8);
}

static void mesh_data_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        ozz::io::MemoryStream stream;
        stream.Write(response->data.ptr, response->data.size);
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
        state.num_skin_joints = meshes[0].num_joints();
        state.num_triangle_indices = (int)meshes[0].triangle_index_count();
        state.ozz->joint_remaps = std::move(meshes[0].joint_remaps);
        state.ozz->mesh_inverse_bindposes = std::move(meshes[0].inverse_bind_poses);

        // convert mesh data into packed vertices
        size_t num_vertices = (meshes[0].parts[0].positions.size() / 3);
        assert(meshes[0].parts[0].normals.size() == (num_vertices * 3));
        assert(meshes[0].parts[0].joint_indices.size() == (num_vertices * 4));
        assert(meshes[0].parts[0].joint_weights.size() == (num_vertices * 3));
        const float* positions = &meshes[0].parts[0].positions[0];
        const float* normals = &meshes[0].parts[0].normals[0];
        const uint16_t* joint_indices = &meshes[0].parts[0].joint_indices[0];
        const float* joint_weights = &meshes[0].parts[0].joint_weights[0];
        sb_vertex_t* vertices = (sb_vertex_t*) calloc(num_vertices, sizeof(sb_vertex_t));
        for (int i = 0; i < (int)num_vertices; i++) {
            sb_vertex_t* v = &vertices[i];
            v->pos.x = positions[i * 3 + 0];
            v->pos.y = positions[i * 3 + 1];
            v->pos.z = positions[i * 3 + 2];
            const float nx = normals[i * 3 + 0];
            const float ny = normals[i * 3 + 1];
            const float nz = normals[i * 3 + 2];
            v->normal = pack_f4_byte4n(nx, ny, nz, 0.0f);
            const uint8_t ji0 = (uint8_t) joint_indices[i * 4 + 0];
            const uint8_t ji1 = (uint8_t) joint_indices[i * 4 + 1];
            const uint8_t ji2 = (uint8_t) joint_indices[i * 4 + 2];
            const uint8_t ji3 = (uint8_t) joint_indices[i * 4 + 3];
            v->joint_indices = pack_u32(ji0, ji1, ji2, ji3);
            const float jw0 = joint_weights[i * 3 + 0];
            const float jw1 = joint_weights[i * 3 + 1];
            const float jw2 = joint_weights[i * 3 + 2];
            const float jw3 = 1.0f - (jw0 + jw1 + jw2);
            v->joint_weights = pack_f4_ubyte4n(jw0, jw1, jw2, jw3);
        }

        // create a storage buffer and view with the vertex data, and an index buffer
        sg_buffer_desc vbuf_desc = {};
        vbuf_desc.usage.storage_buffer = true;
        vbuf_desc.data.ptr = vertices;
        vbuf_desc.data.size = num_vertices * sizeof(sb_vertex_t);
        vbuf_desc.label = "vertices";
        state.vertex_buf = sg_make_buffer(&vbuf_desc);
        sg_view_desc view_desc = {};
        view_desc.storage_buffer.buffer = state.vertex_buf;
        view_desc.label = "vertices-view";
        state.bind.views[VIEW_vertices] = sg_make_view(&view_desc);
        free(vertices); vertices = nullptr;

        sg_buffer_desc ibuf_desc = { };
        ibuf_desc.usage.index_buffer = true;
        ibuf_desc.data.ptr = &meshes[0].triangle_indices[0];
        ibuf_desc.data.size = state.num_triangle_indices * sizeof(uint16_t);
        ibuf_desc.label = "indices";
        state.bind.index_buffer = sg_make_buffer(&ibuf_desc);
    }
    else if (response->failed) {
        state.loaded.failed = true;
    }
}

static void draw_ui(void) {
    if (ImGui::BeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        ImGui::EndMainMenuBar();
    }
    sgimgui_draw();
    ImGui::SetNextWindowPos({ 20, 20 }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ 220, 150 }, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!sg_query_features().compute) {
            ImGui::Text("Storage buffers not supported");
        } else if (state.loaded.failed) {
            ImGui::Text("Failed loading character data!");
        } else {
            if (ImGui::SliderInt("Num Instances", &state.num_instances, 1, MAX_INSTANCES)) {
                float dist_step = (state.camera.max_dist - state.camera.min_dist) / MAX_INSTANCES;
                state.camera.distance = state.camera.min_dist + dist_step * state.num_instances;
            }
            ImGui::Text("Frame Time: %.3fms\n", state.time.frame_time_ms);
            ImGui::Text("Anim Eval Time: %.3fms\n", stm_ms(state.time.anim_eval_time));
            ImGui::Text("Num Triangles: %d\n", (state.num_triangle_indices/3) * state.num_instances);
            ImGui::Text("Num Animated Joints: %d\n", state.num_skeleton_joints * state.num_instances);
            ImGui::Text("Num Skinning Joints: %d\n", state.num_skin_joints * state.num_instances);
            ImGui::Separator();
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
            ImGui::Separator();
        }
    }
    ImGui::End();
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
