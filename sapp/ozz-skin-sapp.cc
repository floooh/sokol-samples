//------------------------------------------------------------------------------
//  ozz-skin-sapp.c
//
//  Ozz-animation with GPU skinning.
//  FIXME: explain how it works
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

#include "ozz-skin-sapp.glsl.h"

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

#define MAX_PALETTE_JOINTS (256)
#define MAX_INSTANCES (64)

// wrapper struct for managed ozz-animation C++ objects, must be deleted
// before shutdown, otherwise ozz-animation will report a memory leak
typedef struct {
    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animation;
    ozz::animation::SamplingCache cache;
    ozz::vector<uint16_t> joint_remaps;
    ozz::vector<ozz::math::Float4x4> mesh_inverse_bindposes;
    ozz::vector<ozz::math::SoaTransform> local_matrices;
    ozz::vector<ozz::math::Float4x4> model_matrices;
} ozz_t;

// a skinned-mesh vertex, we don't need the texcoords and tangent
// in our example renderer so we just drop them. Normals, joint indices
// and joint weights are packed into BYTE4N, UBYTE4 and UBYTE4N
typedef struct {
    float position[3];
    uint32_t normal;
    uint32_t joint_indices;
    uint32_t joint_weights;
} vertex_t;

// per-instance data for hardware-instanced rendering includes the
// transposed 4x3 model-to-world matrix, and information where the
// joint palette is found in the joint texture
typedef struct {
    float model[3][4];
    float skin_info[4];
} instance_t;

static struct {
    std::unique_ptr<ozz_t> ozz;
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_image joint_texture;
    sg_buffer instance_buffer;
    sg_bindings bind;
    vs_params_t vs_params;
    int num_triangle_indices;
    int num_skin_joints;        // number of joints actually used by skinned mesh
    int joint_texture_width;    // in number of pixels
    int joint_texture_height;   // in number of pixels
    int joint_texture_pitch;    // in number of floats
    camera_t camera;
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
    struct {
        bool joint_texture_shown;
        int joint_texture_scale;
    } ui;
} state;

// IO buffers (we know the max file sizes upfront)
static uint8_t skel_io_buffer[32 * 1024];
static uint8_t anim_io_buffer[96 * 1024];
static uint8_t mesh_io_buffer[3 * 1024 * 1024];

// instance data upload buffer
instance_t instance_upload_buffer[MAX_INSTANCES];

// joint-matrix upload buffer, each joint consists of transposed 4x3 matrix
float joint_upload_buffer[MAX_INSTANCES][MAX_PALETTE_JOINTS][3][4];

static void draw_ui(void);
static void skel_data_loaded(const sfetch_response_t* respone);
static void anim_data_loaded(const sfetch_response_t* respone);
static void mesh_data_loaded(const sfetch_response_t* respone);

static void init(void) {
    state.ozz = std::make_unique<ozz_t>();
    state.time.factor = 1.0f;
    state.ui.joint_texture_scale = 4;

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

    // initialize camera controller
    camera_desc_t camdesc = { };
    camdesc.min_dist = 1.0f;
    camdesc.max_dist = 10.0f;
    camdesc.center.Y = 1.1f;
    camdesc.distance = 3.0f;
    camdesc.latitude = 10.0f;
    camdesc.longitude = 20.0f;
    cam_init(&state.camera, &camdesc);

    // vertex-skinning shader and pipeline object for 3d rendering
    sg_pipeline_desc pip_desc = { };
    pip_desc.shader = sg_make_shader(skinned_shader_desc(sg_query_backend()));
    pip_desc.layout.buffers[0].stride = sizeof(vertex_t);
    pip_desc.layout.buffers[1].stride = sizeof(instance_t);
    pip_desc.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
    pip_desc.layout.attrs[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_vs_normal].format = SG_VERTEXFORMAT_BYTE4N;
    pip_desc.layout.attrs[ATTR_vs_jindices].format = SG_VERTEXFORMAT_UBYTE4;
    pip_desc.layout.attrs[ATTR_vs_jweights].format = SG_VERTEXFORMAT_UBYTE4N;
    pip_desc.layout.attrs[ATTR_vs_inst_xxxx].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_vs_inst_xxxx].buffer_index = 1;
    pip_desc.layout.attrs[ATTR_vs_inst_yyyy].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_vs_inst_yyyy].buffer_index = 1;
    pip_desc.layout.attrs[ATTR_vs_inst_zzzz].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_vs_inst_zzzz].buffer_index = 1;
    pip_desc.layout.attrs[ATTR_vs_inst_skin_info].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_vs_inst_skin_info].buffer_index = 1;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    // ozz mesh data appears to have counter-clock-wise face winding
    pip_desc.face_winding = SG_FACEWINDING_CCW;
    pip_desc.cull_mode = SG_CULLMODE_BACK;
    pip_desc.depth.write_enabled = true;
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    state.pip = sg_make_pipeline(&pip_desc);

    // create a dynamic joint-palette texture and intermediate joint-upload-buffer
    state.joint_texture_width = MAX_PALETTE_JOINTS * 3;
    state.joint_texture_height = MAX_INSTANCES;
    state.joint_texture_pitch = state.joint_texture_width * 4;
    sg_image_desc img_desc = { };
    img_desc.width = state.joint_texture_width;
    img_desc.height = state.joint_texture_height;
    img_desc.num_mipmaps = 1;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA32F;
    img_desc.usage = SG_USAGE_STREAM;
    img_desc.min_filter = SG_FILTER_NEAREST;
    img_desc.mag_filter = SG_FILTER_NEAREST;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    state.joint_texture = sg_make_image(&img_desc);
    state.bind.vs_images[SLOT_joint_tex] = state.joint_texture;

    // create a dynamic instance-data buffer
    sg_buffer_desc buf_desc = { };
    buf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    buf_desc.size = MAX_INSTANCES * sizeof(instance_t);
    buf_desc.usage = SG_USAGE_STREAM;
    state.instance_buffer = sg_make_buffer(&buf_desc);
    state.bind.vertex_buffers[1] = state.instance_buffer;

    // start loading data
    {
        sfetch_request_t req = { };
        req.path = "ozz_skin_skeleton.ozz";
        req.callback = skel_data_loaded;
        req.buffer_ptr = skel_io_buffer;
        req.buffer_size = sizeof(skel_io_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = { };
        req.path = "ozz_skin_animation.ozz";
        req.callback = anim_data_loaded;
        req.buffer_ptr = anim_io_buffer;
        req.buffer_size = sizeof(anim_io_buffer);
        sfetch_send(&req);
    }
    {
        sfetch_request_t req = { };
        req.path = "ozz_skin_mesh.ozz";
        req.callback = mesh_data_loaded;
        req.buffer_ptr = mesh_io_buffer;
        req.buffer_size = sizeof(mesh_io_buffer);
        sfetch_send(&req);
    }
}

static void eval_animation(void) {
    // convert current time to animation ration (0.0 .. 1.0)
    const float anim_duration = state.ozz->animation.duration();
    if (!state.time.anim_ratio_ui_override) {
        state.time.anim_ratio = fmodf(state.time.absolute / anim_duration, 1.0f);
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

// update the instance-data buffer with the model-to-world matrices and
// joint-palette-texture infos for each character instance
static void update_instance_buffer(int num_instances) {
    // FIXME: fill model-to-world matrices
    const float half_pixel_x = 0.5f / (float)state.joint_texture_width;
    const float half_pixel_y = 0.5f / (float)state.joint_texture_height;
    for (int i = 0; i < num_instances; i++) {
        instance_t* inst = &instance_upload_buffer[i];
        inst->skin_info[0] = half_pixel_x;
        inst->skin_info[1] = half_pixel_y + (i / (float)state.joint_texture_height);
        inst->skin_info[2] = (float)state.joint_texture_width;
        inst->skin_info[3] = (float)state.joint_texture_height;
    }
    sg_range data = { instance_upload_buffer, num_instances * sizeof(instance_t) };
    sg_update_buffer(state.instance_buffer, &data);
}

// compute skinning matrices, and upload into joint texture
static void update_joint_texture(void) {
    const int instance = 0;
    for (int i = 0; i < state.num_skin_joints; i++) {
        ozz::math::Float4x4 skin_matrix = state.ozz->model_matrices[state.ozz->joint_remaps[i]] * state.ozz->mesh_inverse_bindposes[i];
        const ozz::math::SimdFloat4& c0 = skin_matrix.cols[0];
        const ozz::math::SimdFloat4& c1 = skin_matrix.cols[1];
        const ozz::math::SimdFloat4& c2 = skin_matrix.cols[2];
        const ozz::math::SimdFloat4& c3 = skin_matrix.cols[3];

        float* ptr = &joint_upload_buffer[instance][i][0][0];
        *ptr++ = ozz::math::GetX(c0); *ptr++ = ozz::math::GetX(c1); *ptr++ = ozz::math::GetX(c2); *ptr++ = ozz::math::GetX(c3);
        *ptr++ = ozz::math::GetY(c0); *ptr++ = ozz::math::GetY(c1); *ptr++ = ozz::math::GetY(c2); *ptr++ = ozz::math::GetY(c3);
        *ptr++ = ozz::math::GetZ(c0); *ptr++ = ozz::math::GetZ(c1); *ptr++ = ozz::math::GetZ(c2); *ptr++ = ozz::math::GetZ(c3);
    }

    sg_image_data img_data = { };
    // FIXME: upload partial texture? (needs sokol-gfx fixes)
    img_data.subimage[0][0] = SG_RANGE(joint_upload_buffer);
    sg_update_image(state.joint_texture, img_data);
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
    if (state.loaded.animation && state.loaded.skeleton && state.loaded.mesh) {
        if (!state.time.paused) {
            state.time.absolute += state.time.frame * state.time.factor;
        }
        eval_animation();
        update_joint_texture();
        update_instance_buffer(1);

        state.vs_params.view_proj = state.camera.view_proj;
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(state.vs_params));
        sg_draw(0, state.num_triangle_indices, 1);
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
    simgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();

    // free C++ objects early, otherwise ozz-animation complains about memory leaks
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
            ImGui::Separator();
            if (ImGui::Button("Toggle Joint Texture")) {
                state.ui.joint_texture_shown = !state.ui.joint_texture_shown;
            }
        }
    }
    if (state.ui.joint_texture_shown) {
        ImGui::SetNextWindowPos({ 20, 300 }, ImGuiCond_Once);
        ImGui::SetNextWindowSize({ 600, 300 }, ImGuiCond_Once);
        if (ImGui::Begin("Joint Texture", &state.ui.joint_texture_shown)) {
            ImGui::InputInt("##scale", &state.ui.joint_texture_scale);
            ImGui::SameLine();
            if (ImGui::Button("1x")) { state.ui.joint_texture_scale = 1; }
            ImGui::SameLine();
            if (ImGui::Button("2x")) { state.ui.joint_texture_scale = 2; }
            ImGui::SameLine();
            if (ImGui::Button("4x")) { state.ui.joint_texture_scale = 4; }
            ImGui::BeginChild("##frame", {0,0}, true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::Image((ImTextureID)(uintptr_t)state.joint_texture.id,
                { (float)(state.joint_texture_width * state.ui.joint_texture_scale), (float)(state.joint_texture_height * state.ui.joint_texture_scale) },
                { 0.0f, 0.0f },
                { 1.0f, 1.0f });
            ImGui::EndChild();
        }
        ImGui::End();
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
            state.ozz->local_matrices.resize(num_soa_joints);
            state.ozz->model_matrices.resize(num_joints);
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
        state.ozz->joint_remaps = meshes[0].joint_remaps;
        state.ozz->mesh_inverse_bindposes = meshes[0].inverse_bind_poses;
        state.num_skin_joints = meshes[0].num_joints();
        state.num_triangle_indices = (int)meshes[0].triangle_index_count();

        // convert mesh data into packed vertices
        size_t num_vertices  (meshes[0].parts[0].positions.size() / 3);
        assert(meshes[0].parts[0].normals.size() == (num_vertices * 3));
        assert(meshes[0].parts[0].joint_indices.size() == (num_vertices * 4));
        assert(meshes[0].parts[0].joint_weights.size() == (num_vertices * 3));
        const float* positions = &meshes[0].parts[0].positions[0];
        const float* normals = &meshes[0].parts[0].normals[0];
        const uint16_t* joint_indices = &meshes[0].parts[0].joint_indices[0];
        const float* joint_weights = &meshes[0].parts[0].joint_weights[0];
        vertex_t* vertices = (vertex_t*) calloc(num_vertices, sizeof(vertex_t));
        for (int i = 0; i < (int)num_vertices; i++) {
            vertex_t* v = &vertices[i];
            v->position[0] = positions[i * 3 + 0];
            v->position[1] = positions[i * 3 + 1];
            v->position[2] = positions[i * 3 + 2];
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

        // create vertex- and index-buffer
        sg_buffer_desc vbuf_desc = { };
        vbuf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
        vbuf_desc.data.ptr = vertices;
        vbuf_desc.data.size = num_vertices * sizeof(vertex_t);
        state.bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
        free(vertices); vertices = nullptr;

        sg_buffer_desc ibuf_desc = { };
        ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
        ibuf_desc.data.ptr = &meshes[0].triangle_indices[0];
        ibuf_desc.data.size = state.num_triangle_indices * sizeof(uint16_t);
        state.bind.index_buffer = sg_make_buffer(&ibuf_desc);
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
