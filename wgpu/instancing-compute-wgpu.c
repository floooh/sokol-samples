//------------------------------------------------------------------------------
//  instancing-compute-wgpu.c
//------------------------------------------------------------------------------
#include "wgpu_entry.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_time.h"

#define MAX_PARTICLES (512 * 1024)
#define NUM_PARTICLES_EMITTED_PER_FRAME (10)

static struct {
    uint64_t last_time;
    int num_particles;
    float ry;
    struct {
        sg_buffer buf;
        sg_pipeline pip;
    } compute;
    struct {
        sg_buffer vbuf;
        sg_buffer ibuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
    } display;
} state = {
    .display = {
        .pass_action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0, 0, 0, 1} },
        }
    }
};

typedef struct {
    float dt;
    uint32_t num_particles;
} cs_params_t;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

typedef struct {
    float pos[4];
    float vel[4];
} particle_t;

static inline uint32_t xorshift32(void) {
    static uint32_t x = 0x12345678;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    return x;
}

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = wgpu_environment(),
        .logger.func = slog_func,
    });
    stm_setup();

    // a storage buffer which holds the particle positions and velocities,
    // updated by a compute shader
    {
        particle_t* p = calloc(MAX_PARTICLES, sizeof(particle_t));
        for (size_t i = 0; i < MAX_PARTICLES; i++) {
            p[i].vel[0] = ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f;
            p[i].vel[1] = ((float)(xorshift32() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f;
            p[i].vel[2] = ((float)(xorshift32() & 0x7FFF) / 0x7FFF) - 0.5f;
        }
        state.compute.buf = sg_make_buffer(&(sg_buffer_desc){
            .usage.storage_buffer = true,
            .data = {
                .ptr = p,
                .size = MAX_PARTICLES * sizeof(particle_t),
            },
            .label = "particle-buffer",
        });
        free(p);
    }

    // create compute shader
    sg_shader compute_shd = sg_make_shader(&(sg_shader_desc){
        .compute_func.source =
            "struct params_t {\n"
            "  dt: f32,\n"
            "  num_particles: u32,\n"
            "}\n"
            "struct particle_t {\n"
            "  pos: vec4f,\n"
            "  vel: vec4f,\n"
            "}\n"
            "struct particles_t {\n"
            "  prt: array<particle_t>,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> in: params_t;\n"
            "@group(1) @binding(0) var<storage, read_write> pbuf: particles_t;\n"
            "@compute @workgroup_size(64)\n"
            "fn main(@builtin(global_invocation_id) gid : vec3u) {\n"
            "  let idx = gid.x;\n"
            "  if (idx >= in.num_particles) {\n"
            "    return;\n"
            "  }\n"
            "  var pos = pbuf.prt[idx].pos;\n"
            "  var vel = pbuf.prt[idx].vel;\n"
            "  vel.y -= 1.0 * in.dt;\n"
            "  pos += vel * in.dt;\n"
            "  if (pos.y < -2.0) {\n"
            "    pos.y = -1.8;\n"
            "    vel *= vec4f(0.8, -0.8, 0.8, 0);\n"
            "  }\n"
            "  pbuf.prt[idx].pos = pos;\n"
            "  pbuf.prt[idx].vel = vel;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_COMPUTE,
            .size = sizeof(cs_params_t),
            .wgsl_group0_binding_n = 0,
        },
        .storage_buffers[0] = {
            .stage = SG_SHADERSTAGE_COMPUTE,
            .readonly = false,
            .wgsl_group1_binding_n = 0,
        },
        .label = "compute-shader",
    });

    // create a compute pipeline object
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = compute_shd,
        .label = "compute-pipeline",
    });

    // vertex buffer for static geometry, goes into vertex-buffer-slot 0
    const float r = 0.05f;
    const float vertices[] = {
        // positions            colors
        0.0f,   -r, 0.0f,       1.0f, 0.0f, 0.0f, 1.0f,
           r, 0.0f, r,          0.0f, 1.0f, 0.0f, 1.0f,
           r, 0.0f, -r,         0.0f, 0.0f, 1.0f, 1.0f,
          -r, 0.0f, -r,         1.0f, 1.0f, 0.0f, 1.0f,
          -r, 0.0f, r,          0.0f, 1.0f, 1.0f, 1.0f,
        0.0f,    r, 0.0f,       1.0f, 0.0f, 1.0f, 1.0f
    };
    state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "geometry-vbuf",
    });

    // index buffer for static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    state.display.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices),
        .label = "geometry-ibuf",
    });

    // shader for rendering with instance positions pulled from the storage buffer
    sg_shader display_shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "struct vs_params {\n"
            "  mvp: mat4x4f,\n"
            "}\n"
            "struct particle_t {\n"
            "  pos: vec4f,\n"
            "  vel: vec4f,\n"
            "}\n"
            "struct particles_t {\n"
            "  prt: array<particle_t>,\n"
            "}\n"
            "@group(0) @binding(0) var<uniform> in: vs_params;\n"
            "@group(1) @binding(0) var<storage, read> pbuf: particles_t;\n"
            "struct vs_out {\n"
            "  @builtin(position) pos: vec4f,\n"
            "  @location(0) color: vec4f,\n"
            "}\n"
            "@vertex fn main(@location(0) pos: vec3f, @location(1) color: vec4f, @builtin(instance_index) iidx: u32) -> vs_out {\n"
            "  var out: vs_out;\n"
            "  let inst_pos = pbuf.prt[iidx].pos.xyz;\n"
            "  out.pos = in.mvp * vec4f(pos + inst_pos, 1.0);\n"
            "  out.color = color;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "@fragment fn main(@location(0) color: vec4f) -> @location(0) vec4f {\n"
            "  return color;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .wgsl_group0_binding_n = 0,
        },
        .storage_buffers[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .readonly = true,
            .wgsl_group1_binding_n = 0,
        },
        .label = "render-shader",
    });

    // a pipeline object for rendering, this uses geometry from
    // traditional vertex buffer, but pull per-instance positions
    // from the compute-update storage buffer
    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },  // position
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 },  // color
            }
        },
        .shader = display_shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_BACK,
        .label = "render-pipeline",
    });
}

static void frame(void) {
    state.num_particles += NUM_PARTICLES_EMITTED_PER_FRAME;
    if (state.num_particles > MAX_PARTICLES) {
        state.num_particles = MAX_PARTICLES;
    }

    // compute pass to update particle positions
    const cs_params_t cs_params = {
        .dt = (float)stm_sec(stm_laptime(&state.last_time)),
        .num_particles = (uint32_t)state.num_particles,
    };
    sg_begin_pass(&(sg_pass){ .compute = true, .label = "compute-pass" });
    sg_apply_pipeline(state.compute.pip);
    sg_apply_bindings(&(sg_bindings){
        .storage_buffers[0] = state.compute.buf,
    });
    sg_apply_uniforms(0, &SG_RANGE(cs_params));
    sg_dispatch((state.num_particles + 63)/ 64, 1, 1);
    sg_end_pass();

    // render pass to render instanced geometry, with the instance-positions
    // pulled from the compute-updated storage buffer
    state.ry += 1;
    const hmm_mat4 proj = HMM_Perspective(60.0f, (float)wgpu_width()/(float)wgpu_height(), 0.01f, 50.0f);
    const hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 12.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    const hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    const vs_params_t vs_params = {
        .mvp = HMM_MultiplyMat4(view_proj, HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f)))
    };
    sg_begin_pass(&(sg_pass){
        .action = state.display.pass_action,
        .swapchain = wgpu_swapchain(),
        .label = "render-pass",
    });
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbuf,
        .index_buffer = state.display.ibuf,
        .storage_buffers[0] = state.compute.buf,
    });
    sg_apply_uniforms(0, &SG_RANGE(vs_params));
    sg_draw(0, 24, state.num_particles);
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    wgpu_start(&(wgpu_desc_t){
        .init_cb = init,
        .frame_cb = frame,
        .shutdown_cb = shutdown,
        .width = 640,
        .height = 480,
        .sample_count = 1,
        .title = "instancing-compute-wgpu",
    });
}
