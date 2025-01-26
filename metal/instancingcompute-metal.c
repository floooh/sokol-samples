//------------------------------------------------------------------------------
//  instancingcompute-metal.c
//------------------------------------------------------------------------------
#include "osxentry.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include <stdlib.h> // calloc, free

#define MAX_PARTICLES (512 * 1024)

static struct {
    struct {
        sg_buffer buffers[2];
        sg_pipeline pip;
    } compute;
    struct {
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
} params_t;

typedef struct {
    float pos[4];
    float vel[4];
} particle_t;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = osx_environment(),
        .logger.func = slog_func,
    });

    // create two ping-pong storage buffers with random initial particle velocities
    {
        particle_t* p = calloc(MAX_PARTICLES, sizeof(particle_t));
        for (size_t i = 0; i < MAX_PARTICLES; i++) {
            p[i].vel[0] = ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f;
            p[i].vel[1] = ((float)(rand() & 0x7FFF) / 0x7FFF) * 0.5f + 2.0f;
            p[i].vel[2] = ((float)(rand() & 0x7FFF) / 0x7FFF) - 0.5f;
        }
        for (size_t i = 0; i < 2; i++) {
            state.compute.buffers[i] = sg_make_buffer(&(sg_buffer_desc){
                .type = SG_BUFFERTYPE_STORAGEBUFFER,
                .data = {
                    .ptr = p,
                    .size = MAX_PARTICLES * sizeof(particle_t),
                },
            });
        }
        free(p);
    }

    // create compute shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .compute_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float dt;\n"
            "};\n"
            "struct particle_t {\n"
            "  float4 pos;\n"
            "  float4 vel;\n"
            "};\n"
            "struct ssbo_t {\n"
            "  particle_t prt[1];\n"
            "};\n"
            "kernel void _main(\n"
            "  constant params_t& params [[buffer(0)]],\n"
            "  const device ssbo_t& inp [[buffer(8)]],\n"
            "  device ssbo_t& out [[buffer(9)]],\n"
            "  uint idx [[thread_position_in_grid]])\n"
            "{\n"
            "  float4 pos = inp.prt[idx].pos;\n"
            "  float4 vel = inp.prt[idx].vel;\n"
            "  float dt = params.dt;\n"
            "  vel.y -= 1.0 * dt;\n"
            "  pos += vel * dt;\n"
            "  if (pos.y < -2.0) {\n"
            "    pos.y = -1.8f;\n"
            "    vel *= float4(0.8, -0.8, 0.8, 0);\n"
            "  }\n"
            "  out.prt[idx].pos = pos;\n"
            "  out.prt[idx].vel = vel;\n"
            "}\n",
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_COMPUTE,
            .size = sizeof(params_t),
            .msl_buffer_n = 0,
        },
        .storage_buffers = {
            [0] = {
                .stage = SG_SHADERSTAGE_COMPUTE,
                .readonly = true,
                .msl_buffer_n = 8,
            },
            [1] = {
                .stage = SG_SHADERSTAGE_COMPUTE,
                .readonly = false,
                .msl_buffer_n = 9,
            },
        },
    });

    // create a compute pipeline object
    state.compute.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .compute = true,
        .shader = shd,
    });
}

static void frame(void) {
    sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = osx_swapchain() });
    sg_end_pass();
    sg_commit();
}

static void shutdown(void) {
    sg_shutdown();
}

int main() {
    osx_start(640, 480, 1, SG_PIXELFORMAT_DEPTH, "computeboids-metal", init, frame, shutdown);
}
