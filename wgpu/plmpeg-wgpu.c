//------------------------------------------------------------------------------
//  plmpeg-wgpu.c
//
//  See sapp/plmpeg-sapp.c for details.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_audio.h"
#include "sokol_fetch.h"
#include "plmpeg-wgpu.glsl.h"
#define PL_MPEG_IMPLEMENTATION
#include "../libs/pl_mpeg/pl_mpeg.h"
#include <assert.h>
#include "wgpu_entry.h"

#define SAMPLE_COUNT (4)

static const char* filename = "bjork-all-is-full-of-love.mpg";

// statically allocated streaming buffers
#define BUFFER_SIZE (512*1024)
#define CHUNK_SIZE (128*1024)
#define NUM_BUFFERS (4)
static uint8_t buf[NUM_BUFFERS][BUFFER_SIZE];

// a simple ring buffer for the circular buffer queue
#define RING_NUM_SLOTS (NUM_BUFFERS+1)
typedef struct {
    uint32_t head;
    uint32_t tail;
    int buf[RING_NUM_SLOTS];
} ring_t;
static bool ring_empty(const ring_t* rb);
static bool ring_full(const ring_t* rb);
static uint32_t ring_count(const ring_t* rb);
static void ring_enqueue(ring_t* rb, int val);
static int ring_dequeue(ring_t* rb);

// a vertex with position, normal and texcoords
typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
} vertex_t;

// application state
static struct {
    plm_t* plm;
    plm_buffer_t* plm_buffer;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    struct {
        int width;
        int height;
        uint64_t last_upd_frame;
    } image_attrs[3];
    ring_t free_buffers;
    ring_t full_buffers;
    int cur_download_buffer;
    int cur_read_buffer;
    uint32_t cur_read_pos;
    float ry;
    uint64_t cur_frame;
} state;

// sokol-fetch callback
static void fetch_callback(const sfetch_response_t* response);
// plmpeg's data loading callback
static void plmpeg_load_callback(plm_buffer_t* buf, void* user);
// plmpeg's callback when a video frame is ready
static void video_cb(plm_t *mpeg, plm_frame_t *frame, void *user);
// plmpeg's callback when audio data is ready
static void audio_cb(plm_t *mpeg, plm_samples_t *samples, void *user);

// the sokol-app init-callback
static void init(void) {

    // setup circular queues of "free" and "full" buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        ring_enqueue(&state.free_buffers, i);
    }
    state.cur_download_buffer = ring_dequeue(&state.free_buffers);
    state.cur_read_buffer = -1;

    // setup sokol-fetch and start fetching the file, once the first two buffers
    // have been filled with data, setup pl_mpeg (this happens down in the frame callback)
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 1,
        .num_channels = 1,
        .num_lanes = 1
    });
    sfetch_send(&(sfetch_request_t){
        .path = filename,
        .callback = fetch_callback,
        .buffer_ptr = buf[state.cur_download_buffer],
        .buffer_size = BUFFER_SIZE,
        .chunk_size = CHUNK_SIZE
    });

    // initialize sokol-gfx
    sg_setup(&(sg_desc){
        .context = wgpu_get_context()
    });

    // vertex-, index-buffer, shader and pipeline
    const vertex_t vertices[] = {
        /* pos         normal    uvs */
        { -1, -1, -1,  0, 0,-1,  1, 1 },
        {  1, -1, -1,  0, 0,-1,  0, 1 },
        {  1,  1, -1,  0, 0,-1,  0, 0 },
        { -1,  1, -1,  0, 0,-1,  1, 0 },

        { -1, -1,  1,  0, 0, 1,  0, 1 },
        {  1, -1,  1,  0, 0, 1,  1, 1 },
        {  1,  1,  1,  0, 0, 1,  1, 0 },
        { -1,  1,  1,  0, 0, 1,  0, 0 },

        { -1, -1, -1, -1, 0, 0,  0, 1 },
        { -1,  1, -1, -1, 0, 0,  0, 0 },
        { -1,  1,  1, -1, 0, 0,  1, 0 },
        { -1, -1,  1, -1, 0, 0,  1, 1 },

        {  1, -1, -1,  1, 0, 0,  1, 1 },
        {  1,  1, -1,  1, 0, 0,  1, 0 },
        {  1,  1,  1,  1, 0, 0,  0, 0 },
        {  1, -1,  1,  1, 0, 0,  0, 1 },
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    const uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
    };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices)
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs = {
            [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_vs_normal].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2
        },
        .shader = sg_make_shader(plmpeg_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_NONE,
    });

    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.569f, 0.918f, 1.0f } }
    };

    // NOTE: texture creation is deferred until first frame is decoded
}

// the sokol-app frame callback (video decoding and rendering)
static void frame(void) {
    state.cur_frame++;

    // pump the sokol-fetch message queues
    sfetch_dowork();

    // stop decoding if there's not at least one buffer of downloaded
    // data ready, to allow slow downloads to catch up
    if (state.plm) {
        if (!ring_empty(&state.full_buffers)) {
            plm_decode(state.plm, 1.0/60.0);
        }
    }
    // initialize plmpeg once two buffers are filled with data
    else if (ring_count(&state.full_buffers) == 2) {
        state.plm_buffer = plm_buffer_create_with_capacity(BUFFER_SIZE);
        plm_buffer_set_load_callback(state.plm_buffer, plmpeg_load_callback, 0);
        state.plm = plm_create_with_buffer(state.plm_buffer, true);
        assert(state.plm);
        plm_set_video_decode_callback(state.plm, video_cb, 0);
        plm_set_audio_decode_callback(state.plm, audio_cb, 0);
        plm_set_loop(state.plm, true);
        plm_set_audio_enabled(state.plm, true, 0);
        plm_set_audio_lead_time(state.plm, 0.25);
        if (plm_get_num_audio_streams(state.plm) > 0) {
            saudio_setup(&(saudio_desc){
                .sample_rate = plm_get_samplerate(state.plm),
                .buffer_frames = 4096,
                .num_packets = 256,
                .num_channels = 2,
            });
        }
    }

    // compute model-view-projection matrix for vertex shader
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    state.ry += -0.1f;
    hmm_mat4 model = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    // start rendering, but not before the first video frame has been decoded into textures
    sg_begin_default_pass(&state.pass_action, wgpu_width(), wgpu_height());
    if (state.bind.fs_images[0].id != SG_INVALID_ID) {
        sg_apply_pipeline(state.pip);
        sg_apply_bindings(&state.bind);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
        sg_draw(0, 24, 1);
    }
    sg_end_pass();
    sg_commit();
}

// the sokol-sapp cleanup callback
static void shutdown(void) {
    if (state.plm_buffer) {
        plm_buffer_destroy(state.plm_buffer);
    }
    sg_shutdown();
}

// (re-)create a video plane texture on demand, and update it with decoded video-plane data
static void validate_texture(int slot, plm_plane_t* plane) {

    if ((state.image_attrs[slot].width != (int)plane->width) ||
        (state.image_attrs[slot].height != (int)plane->height))
    {
        state.image_attrs[slot].width = plane->width;
        state.image_attrs[slot].height = plane->height;

        // NOTE: it's ok to call sg_destroy_image() with SG_INVALID_ID
        sg_destroy_image(state.bind.fs_images[slot]);
        state.bind.fs_images[slot] = sg_make_image(&(sg_image_desc){
            .width = plane->width,
            .height = plane->height,
            .pixel_format = SG_PIXELFORMAT_R8,
            .usage = SG_USAGE_STREAM,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        });
    }

    // copy decoded plane pixels into texture, need to prevent that
    // sg_update_image() is called more than once per frame
    if (state.image_attrs[slot].last_upd_frame != state.cur_frame) {
        state.image_attrs[slot].last_upd_frame = state.cur_frame;
        sg_update_image(state.bind.fs_images[slot], &(sg_image_data){
            .subimage[0][0] = {
                .ptr = plane->data,
                .size = plane->width * plane->height * sizeof(uint8_t)
            }
        });
    }
}

// the pl_mpeg video callback, copies decoded video data into textures
static void video_cb(plm_t* mpeg, plm_frame_t* frame, void* user) {
    validate_texture(SLOT_tex_y, &frame->y);
    validate_texture(SLOT_tex_cb, &frame->cb);
    validate_texture(SLOT_tex_cr, &frame->cr);
}

// the pl_mpeg audio callback, forwards decoded audio samples to sokol-audio
static void audio_cb(plm_t* mpeg, plm_samples_t* samples, void* user) {
    saudio_push(samples->interleaved, samples->count);
}

// the sokol-fetch response callback
static void fetch_callback(const sfetch_response_t* response) {
    // current download buffer has been filled with data...
    if (response->fetched) {
        // put the download buffer into the "full_buffers" queue
        ring_enqueue(&state.full_buffers, state.cur_download_buffer);
        if (ring_full(&state.full_buffers) || ring_empty(&state.free_buffers)) {
            // all buffers in use, need to wait for the video decoding to catch up
            sfetch_pause(response->handle);
        }
        else {
            // ...otherwise start streaming into the next free buffer
            state.cur_download_buffer = ring_dequeue(&state.free_buffers);
            sfetch_unbind_buffer(response->handle);
            sfetch_bind_buffer(response->handle, buf[state.cur_download_buffer], BUFFER_SIZE);
        }
    }
    else if (response->paused) {
        // this handles a paused download, and continues it once the video
        // decoding has caught up
        if (!ring_empty(&state.free_buffers)) {
            state.cur_download_buffer = ring_dequeue(&state.free_buffers);
            sfetch_unbind_buffer(response->handle);
            sfetch_bind_buffer(response->handle, buf[state.cur_download_buffer], BUFFER_SIZE);
            sfetch_continue(response->handle);
        }
    }
}

// the plmpeg load callback, this is called when plmpeg needs new data,
// this takes buffers loaded with video data from the "full-queue"
// as needed
static void plmpeg_load_callback(plm_buffer_t* self, void* user) {
    if (state.cur_read_buffer == -1) {
        state.cur_read_buffer = ring_dequeue(&state.full_buffers);
        state.cur_read_pos = 0;
    }
    plm_buffer_discard_read_bytes(self);
    uint32_t bytes_wanted = (uint32_t) (self->capacity - self->length);
    uint32_t bytes_available = BUFFER_SIZE - state.cur_read_pos;
    uint32_t bytes_to_copy = (bytes_wanted > bytes_available) ? bytes_available : bytes_wanted;
    uint8_t* dst = self->bytes + self->length;
    const uint8_t* src = &buf[state.cur_read_buffer][state.cur_read_pos];
    memcpy(dst, src, bytes_to_copy);
    self->length += bytes_to_copy;
    state.cur_read_pos += bytes_to_copy;
    if (state.cur_read_pos == BUFFER_SIZE) {
        ring_enqueue(&state.free_buffers, state.cur_read_buffer);
        state.cur_read_buffer = -1;
    }
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 960,
        .height = 540,
        .sample_count = SAMPLE_COUNT,
        .title = "plmpeg-wgpu"
    });
    return 0;
}

//=== a simple ring buffer implementation ====================================*/
static uint32_t ring_wrap(const ring_t* rb, uint32_t i) {
    return i % RING_NUM_SLOTS;
}

static bool ring_full(const ring_t* rb) {
    return ring_wrap(rb, rb->head + 1) == rb->tail;
}

static bool ring_empty(const ring_t* rb) {
    return rb->head == rb->tail;
}

static uint32_t ring_count(const ring_t* rb) {
    uint32_t count;
    if (rb->head >= rb->tail) {
        count = rb->head - rb->tail;
    }
    else {
        count = (rb->head + RING_NUM_SLOTS) - rb->tail;
    }
    return count;
}

static void ring_enqueue(ring_t* rb, int val) {
    assert(!ring_full(rb));
    rb->buf[rb->head] = val;
    rb->head = ring_wrap(rb, rb->head + 1);
}

static int ring_dequeue(ring_t* rb) {
    assert(!ring_empty(rb));
    uint32_t slot_id = rb->buf[rb->tail];
    rb->tail = ring_wrap(rb, rb->tail + 1);
    return slot_id;
}
