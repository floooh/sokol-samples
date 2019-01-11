//------------------------------------------------------------------------------
//  dyntex-sapp.c
//  Update dynamic texture with CPU-generated data each frame.
//------------------------------------------------------------------------------
#include <stdlib.h> /* rand */
#include "sokol_app.h"
#include "sokol_gfx.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"

#define MSAA_SAMPLES (4)
#define IMAGE_WIDTH (64)
#define IMAGE_HEIGHT (64)
#define LIVING (0xFFFFFFFF)
#define DEAD (0xFF000000)

static const char *vs_src, *fs_src;

static sg_pass_action pass_action;
static sg_pipeline pip;
static sg_bindings bind;
static float rx, ry;
static int update_count;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

static uint32_t pixels[IMAGE_WIDTH][IMAGE_HEIGHT];

static void game_of_life_init();
static void game_of_life_update();

void init(void) {
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });

    /* a 128x128 image with streaming update strategy */
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = IMAGE_WIDTH,
        .height = IMAGE_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    
    /* cube vertex buffer */
    float vertices[] = {
        /* pos                  color                       uvs */
        -1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     0.0f, 0.0f,
        1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     1.0f, 0.0f,
        1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.0f, 1.0f,     0.0f, 1.0f,
        
        -1.0f, -1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     0.0f, 0.0f,
        1.0f, -1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     1.0f, 0.0f,
        1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 0.0f, 1.0f,     0.0f, 1.0f,
        
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f, 1.0f, 1.0f,     0.0f, 1.0f,
        
        1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 0.0f,
        1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 0.0f,
        1.0f,  1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 1.0f,
        1.0f, -1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 1.0f,
        
        -1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
        1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
        1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,
        
        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 0.0f,
        1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 1.0f,
        1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 1.0f
    };
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    });
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });

    /* a shader to render a textured cube */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        .vs.source = vs_src,
        .fs.source = fs_src
    });

    /* a pipeline state object */
    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .name="position",   .sem_name="POSITION",   .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0",     .sem_name="COLOR",      .format=SG_VERTEXFORMAT_FLOAT4 },
                [2] = { .name="texcoord0",  .sem_name="TEXCOORD",   .format=SG_VERTEXFORMAT_FLOAT2 }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_BACK,
        .rasterizer.sample_count = MSAA_SAMPLES
    });

    /* setup the resource bindings */
    bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };

    /* initialize the game-of-life state */
    game_of_life_init();
}

void frame(void) {
    /* compute model-view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width()/(float)sapp_height(), 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 4.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    vs_params_t vs_params;
    rx += 0.1f; ry += 0.2f;
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
    
    /* update game-of-life state */
    game_of_life_update();
    
    /* update the texture */
    sg_update_image(bind.fs_images[0], &(sg_image_content){
        .subimage[0][0] = {
            .ptr = pixels,
            .size = sizeof(pixels)
        }
    });
    
    /* render the frame */
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

void game_of_life_init() {
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            if ((rand() & 255) > 230) {
                pixels[y][x] = LIVING;
            }
            else {
                pixels[y][x] = DEAD;
            }
        }
    }
}

void game_of_life_update() {
    for (int y = 0; y < IMAGE_HEIGHT; y++) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            int num_living_neighbours = 0;
            for (int ny = -1; ny < 2; ny++) {
                for (int nx = -1; nx < 2; nx++) {
                    if ((nx == 0) && (ny == 0)) {
                        continue;
                    }
                    if (pixels[(y+ny)&(IMAGE_HEIGHT-1)][(x+nx)&(IMAGE_WIDTH-1)] == LIVING) {
                        num_living_neighbours++;
                    }
                }
            }
            /* any live cell... */
            if (pixels[y][x] == LIVING) {
                if (num_living_neighbours < 2) {
                    /* ... with fewer than 2 living neighbours dies, as if caused by underpopulation */
                    pixels[y][x] = DEAD;
                }
                else if (num_living_neighbours > 3) {
                    /* ... with more than 3 living neighbours dies, as if caused by overpopulation */
                    pixels[y][x] = DEAD;
                }
            }
            else if (num_living_neighbours == 3) {
                /* any dead cell with exactly 3 living neighbours becomes a live cell, as if by reproduction */
                pixels[y][x] = LIVING;
            }
        }
    }
    if (update_count++ > 240) {
        game_of_life_init();
        update_count = 0;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .sample_count = MSAA_SAMPLES,
        .gl_force_gles2 = true,
        .window_title = "Dynamic Texture (sokol-app)",
    };
}

#if defined(SOKOL_GLCORE33)
static const char* vs_src =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec4 position;\n"
    "in vec4 color0;\n"
    "in vec2 texcoord0;\n"
    "out vec2 uv;"
    "out vec4 color;"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  uv = texcoord0;\n"
    "  color = color0;\n"
    "}\n";
static const char* fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec4 color;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = texture(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
static const char* vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "attribute vec2 texcoord0;\n"
    "varying vec2 uv;"
    "varying vec4 color;"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  uv = texcoord0;\n"
    "  color = color0;\n"
    "}\n";
static const char* fs_src =
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "varying vec4 color;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 position [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "  float2 uv [[attribute(2)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.position;\n"
    "  out.color = in.color;\n"
    "  out.uv = in.uv;\n"
    "  return out;\n"
    "}\n";
static const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return float4(tex.sample(smp, in.uv).xyz, 1.0) * in.color;\n"
    "};\n";
#elif defined(SOKOL_D3D11)
static const char* vs_src =
    "cbuffer params {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 pos: POSITION;\n"
    "  float4 color: COLOR0;\n"
    "  float2 uv: TEXCOORD0;\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 color: COLOR0;\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = mul(mvp, inp.pos);\n"
    "  outp.color = inp.color;\n"
    "  outp.uv = inp.uv;\n"
    "  return outp;\n"
    "}\n";
static const char* fs_src =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float4 color: COLOR0, float2 uv: TEXCOORD0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv) * color;\n"
    "}\n";
#endif
