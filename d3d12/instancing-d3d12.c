//------------------------------------------------------------------------------
//  instancing-d3d12.c
//  Demonstrates instanced rendering through D3D12 backend.
//------------------------------------------------------------------------------
#include "d3d12entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D12
#include "sokol_gfx.h"
#include "sokol_log.h"
#define VECMATH_GENERICS
#include "../libs/vecmath/vecmath.h"

enum {
    MAX_PARTICLES = 512 * 1024,
    NUM_PARTICLES_EMITTED_PER_FRAME = 10,
};

// vertex shader uniform block
typedef struct {
    mat44_t mvp;
} vs_params_t;

// particle positions and velocity
static int cur_num_particles = 0;
static vec3_t pos[MAX_PARTICLES];
static vec3_t vel[MAX_PARTICLES];

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup d3d12 app wrapper and sokol_gfx
    const int width = 800;
    const int height = 600;
    d3d12_init(&(d3d12_desc_t){ .width = width, .height = height, .sample_count = 4, .title = L"instancing-d3d12.c" });
    sg_setup(&(sg_desc){
        .environment = d3d12_environment(),
        .logger.func = slog_func,
    });

    // vertex buffer for static geometry (goes into vb slot 0)
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
    sg_buffer vbuf_geom = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    // index buffer for static geometry
    const uint16_t indices[] = {
        0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
        5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
    };
    sg_buffer ibuf_geom = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices)
    });

    // dynamic per-instance data, goes into vb slot 1
    sg_buffer vbuf_inst = sg_make_buffer(&(sg_buffer_desc){
        .usage.stream_update = true,
        .size = MAX_PARTICLES * sizeof(vec3_t),
    });

    // create shader
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source =
            "cbuffer params: register(b0) {\n"
            "  float4x4 mvp;\n"
            "};\n"
            "struct vs_in {\n"
            "  float3 pos: POSITION;\n"
            "  float4 color: COLOR0;\n"
            "  float3 inst_pos: INSTPOS;\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 color: COLOR0;\n"
            "  float4 pos: SV_Position;\n"
            "};\n"
            "vs_out main(vs_in inp) {\n"
            "  vs_out outp;\n"
            "  outp.pos = mul(mvp, float4(inp.pos + inp.inst_pos, 1.0));\n"
            "  outp.color = inp.color;\n"
            "  return outp;\n"
            "};\n",
        .fragment_func.source =
            "float4 main(float4 color: COLOR0): SV_Target0 {\n"
            "  return color;\n"
            "};\n",
        .attrs = {
            [0].hlsl_sem_name = "POSITION",
            [1].hlsl_sem_name = "COLOR",
            [2].hlsl_sem_name = "INSTPOS"
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .hlsl_register_b_n = 0,
        }
    });

    // pipeline object, note the vertex layout description
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers = {
                [0] = { .stride = 28 },
                [1] = { .stride = 12, .step_func = SG_VERTEXSTEP_PER_INSTANCE }
            },
            .attrs = {
                [0] = { .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=0 },
                [1] = { .offset=12, .format=SG_VERTEXFORMAT_FLOAT4, .buffer_index=0 },
                [2] = { .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3, .buffer_index=1 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .cull_mode = SG_CULLMODE_BACK,
    });

    // resource bindings
    sg_bindings bind = {
        .vertex_buffers = {
            [0] = vbuf_geom,
            [1] = vbuf_inst
        },
        .index_buffer = ibuf_geom
    };

    // default pass action (clear to grey)
    sg_pass_action pass_action = { 0 };

    // draw loop
    float ry = 0.0f;
    const float frame_time = 1.0f / 60.0f;
    while (d3d12_process_events()) {
        // emit new particles
        for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
            if (cur_num_particles < MAX_PARTICLES) {
                pos[cur_num_particles] = vec3(0.0, 0.0, 0.0);
                vel[cur_num_particles] = vec3(
                    ((float)(rand() & 0x7FFF) / (float)0x7FFF) - 0.5f,
                    ((float)(rand() & 0x7FFF) / (float)0x7FFF) * 0.5f + 2.0f,
                    ((float)(rand() & 0x7FFF) / (float)0x7FFF) - 0.5f);
                cur_num_particles++;
            } else {
                break;
            }
        }

        // update particle positions
        for (int i = 0; i < cur_num_particles; i++) {
            vel[i].y -= 1.0f * frame_time;
            pos[i].x += vel[i].x * frame_time;
            pos[i].y += vel[i].y * frame_time;
            pos[i].z += vel[i].z * frame_time;
            if (pos[i].y < -2.0f) {
                pos[i].y = -1.8f;
                vel[i].y = -vel[i].y;
                vel[i].x *= 0.8f; vel[i].y *= 0.8f; vel[i].z *= 0.8f;
            }
        }

        // update dynamic instance data buffer
        sg_update_buffer(vbuf_inst, &(sg_range){
            .ptr = pos,
            .size = (size_t)cur_num_particles * sizeof(vec3_t)
        });

        // model-view-projection matrix
        ry += 1.0f;
        const mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), (float)d3d12_width()/(float)d3d12_height(), 0.01f, 50.0f);
        const mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 8.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat44_t model = mat44_rotation_y(vm_radians(ry));
        const vs_params_t vs_params = { .mvp = vm_mul(model, vm_mul(view, proj)) };

        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d12_swapchain() });
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bind);
        sg_apply_uniforms(0, &SG_RANGE(vs_params));
        sg_draw(0, 24, cur_num_particles);
        sg_end_pass();
        sg_commit();
        d3d12_present();
    }
    sg_shutdown();
    d3d12_shutdown();
}
