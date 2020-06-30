//------------------------------------------------------------------------------
//  pixelformats-sapp.c
//  Test pixelformat capabilities.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "pixelformats-sapp.glsl.h"

static struct {
    struct {
        bool valid;
        sg_image sample;
        sg_image filter;
        sg_image render;
        sg_image blend;
        sg_image msaa;
        sg_pipeline cube_render_pip;
        sg_pipeline cube_blend_pip;
        sg_pipeline cube_msaa_pip;
        sg_pipeline bg_render_pip;
        sg_pipeline bg_msaa_pip;
        sg_pass render_pass;
        sg_pass blend_pass;
        sg_pass msaa_pass;
    } fmt[_SG_PIXELFORMAT_NUM];
    sg_bindings cube_bindings;
    sg_bindings bg_bindings;
    float rx, ry;
    cube_vs_params_t cube_vs_params;
    bg_fs_params_t bg_fs_params;
} state;

typedef struct {
    void* ptr;
    int size;
} ptr_size_t;

static const char* pixelformat_string(sg_pixel_format fmt);
static ptr_size_t gen_pixels(sg_pixel_format fmt);
static sg_image setup_invalid_texture(void);

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){
        .pipeline_pool_size = 256,
        .pass_pool_size = 128,
        .context = sapp_sgcontext()
    });

    // setup cimgui
    simgui_setup(&(simgui_desc_t){0});

    // create all the textures and render targets
    sg_image render_depth_img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 64,
        .height = 64,
        .pixel_format = SG_PIXELFORMAT_DEPTH
    });
    sg_image msaa_depth_img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 64,
        .height = 64,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 4,
    });
    sg_image invalid_img = setup_invalid_texture();

    for (int i = 0; i < _SG_PIXELFORMAT_NUM; i++) {
        state.fmt[i].sample = invalid_img;
        state.fmt[i].filter = invalid_img;
        state.fmt[i].render = invalid_img;
        state.fmt[i].blend  = invalid_img;
        state.fmt[i].msaa = invalid_img;
    }

    sg_pipeline_desc cube_render_pip_desc = {
        .layout = {
            .attrs = {
                [ATTR_vs_cube_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_cube_color0].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = sg_make_shader(cube_shader_desc()),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .blend = {
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .rasterizer = {
            .cull_mode = SG_CULLMODE_BACK
        }
    };
    sg_pipeline_desc bg_render_pip_desc = {
        .layout.attrs[ATTR_vs_bg_position].format = SG_VERTEXFORMAT_FLOAT2,
        .shader = sg_make_shader(bg_shader_desc()),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .blend = {
            .depth_format = SG_PIXELFORMAT_DEPTH
        }
    };
    sg_pipeline_desc cube_blend_pip_desc = cube_render_pip_desc;
    cube_blend_pip_desc.blend.enabled = true;
    cube_blend_pip_desc.blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
    cube_blend_pip_desc.blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
    sg_pipeline_desc cube_msaa_pip_desc = cube_render_pip_desc;
    sg_pipeline_desc bg_msaa_pip_desc = bg_render_pip_desc;
    cube_msaa_pip_desc.rasterizer.sample_count = 4;
    bg_msaa_pip_desc.rasterizer.sample_count = 4;

    for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
        sg_pixel_format fmt = (sg_pixel_format)i;
        ptr_size_t pair = gen_pixels(fmt);
        if (pair.ptr) {
            state.fmt[i].valid = true;
            // create unfiltered texture
            if (sg_query_pixelformat(fmt).sample) {
                state.fmt[i].sample = sg_make_image(&(sg_image_desc){
                    .width = 8,
                    .height = 8,
                    .pixel_format = fmt,
                    .content.subimage[0][0] = {
                        .ptr = pair.ptr,
                        .size = pair.size
                    }
                });
            }
            // create filtered texture
            if (sg_query_pixelformat(fmt).filter) {
                state.fmt[i].filter = sg_make_image(&(sg_image_desc){
                    .width = 8,
                    .height = 8,
                    .pixel_format = fmt,
                    .min_filter = SG_FILTER_LINEAR,
                    .mag_filter = SG_FILTER_LINEAR,
                    .content.subimage[0][0] = {
                        .ptr = pair.ptr,
                        .size = pair.size
                    }
                });
            }
            // create non-MSAA render target, pipeline state and pass
            if (sg_query_pixelformat(fmt).render) {
                state.fmt[i].render = sg_make_image(&(sg_image_desc){
                    .render_target = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                });
                cube_render_pip_desc.blend.color_format = fmt;
                bg_render_pip_desc.blend.color_format = fmt;
                state.fmt[i].cube_render_pip = sg_make_pipeline(&cube_render_pip_desc);
                state.fmt[i].bg_render_pip = sg_make_pipeline(&bg_render_pip_desc);
                state.fmt[i].render_pass = sg_make_pass(&(sg_pass_desc){
                    .color_attachments[0].image = state.fmt[i].render,
                    .depth_stencil_attachment.image = render_depth_img,
                });
            }
            // create non-MSAA blend render target, pipeline states and pass
            if (sg_query_pixelformat(fmt).blend) {
                state.fmt[i].blend = sg_make_image(&(sg_image_desc){
                    .render_target = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                });
                cube_blend_pip_desc.blend.color_format = fmt;
                state.fmt[i].cube_blend_pip = sg_make_pipeline(&cube_blend_pip_desc);
                state.fmt[i].blend_pass = sg_make_pass(&(sg_pass_desc){
                    .color_attachments[0].image = state.fmt[i].blend,
                    .depth_stencil_attachment.image = render_depth_img,
                });
            }
            // create MSAA render target and matching pipeline state
            if (sg_query_pixelformat(fmt).msaa) {
                state.fmt[i].msaa = sg_make_image(&(sg_image_desc){
                    .render_target = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                    .sample_count = 4,
                });
                cube_msaa_pip_desc.blend.color_format = fmt;
                bg_msaa_pip_desc.blend.color_format = fmt;
                state.fmt[i].cube_msaa_pip = sg_make_pipeline(&cube_msaa_pip_desc);
                state.fmt[i].bg_msaa_pip = sg_make_pipeline(&bg_msaa_pip_desc);
                state.fmt[i].msaa_pass = sg_make_pass(&(sg_pass_desc){
                    .color_attachments[0].image = state.fmt[i].msaa,
                    .depth_stencil_attachment.image = msaa_depth_img,
                });
            }
        }
    }

    // cube vertex and index buffer
    float cube_vertices[] = {
        -1.0f, -1.0f, -1.0f,   0.7f, 0.3f, 0.3f, 1.0f,
         1.0f, -1.0f, -1.0f,   0.7f, 0.3f, 0.3f, 1.0f,
         1.0f,  1.0f, -1.0f,   0.7f, 0.3f, 0.3f, 1.0f,
        -1.0f,  1.0f, -1.0f,   0.7f, 0.3f, 0.3f, 1.0f,

        -1.0f, -1.0f,  1.0f,   0.3f, 0.7f, 0.3f, 1.0f,
         1.0f, -1.0f,  1.0f,   0.3f, 0.7f, 0.3f, 1.0f,
         1.0f,  1.0f,  1.0f,   0.3f, 0.7f, 0.3f, 1.0f,
        -1.0f,  1.0f,  1.0f,   0.3f, 0.7f, 0.3f, 1.0f,

        -1.0f, -1.0f, -1.0f,   0.3f, 0.3f, 0.7f, 1.0f,
        -1.0f,  1.0f, -1.0f,   0.3f, 0.3f, 0.7f, 1.0f,
        -1.0f,  1.0f,  1.0f,   0.3f, 0.3f, 0.7f, 1.0f,
        -1.0f, -1.0f,  1.0f,   0.3f, 0.3f, 0.7f, 1.0f,

        1.0f, -1.0f, -1.0f,    0.7f, 0.5f, 0.3f, 1.0f,
        1.0f,  1.0f, -1.0f,    0.7f, 0.5f, 0.3f, 1.0f,
        1.0f,  1.0f,  1.0f,    0.7f, 0.5f, 0.3f, 1.0f,
        1.0f, -1.0f,  1.0f,    0.7f, 0.5f, 0.3f, 1.0f,

        -1.0f, -1.0f, -1.0f,   0.3f, 0.5f, 0.7f, 1.0f,
        -1.0f, -1.0f,  1.0f,   0.3f, 0.5f, 0.7f, 1.0f,
         1.0f, -1.0f,  1.0f,   0.3f, 0.5f, 0.7f, 1.0f,
         1.0f, -1.0f, -1.0f,   0.3f, 0.5f, 0.7f, 1.0f,

        -1.0f,  1.0f, -1.0f,   0.7f, 0.3f, 0.5f, 1.0f,
        -1.0f,  1.0f,  1.0f,   0.7f, 0.3f, 0.5f, 1.0f,
         1.0f,  1.0f,  1.0f,   0.7f, 0.3f, 0.5f, 1.0f,
         1.0f,  1.0f, -1.0f,   0.7f, 0.3f, 0.5f, 1.0f
    };
    state.cube_bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(cube_vertices),
        .content = cube_vertices,
    });

    uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    state.cube_bindings.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(cube_indices),
        .content = cube_indices,
    });

    // background quad vertices
    float vertices[] = { -1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f, };
    state.bg_bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices
    });
}

static void frame(void) {

    const int w = sapp_width();
    const int h = sapp_height();

    // compute the model-view-proj matrix for rendering to render targets
    hmm_mat4 proj = HMM_Perspective(60.0f, 1.0f, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.rx += 1.0f; state.ry += 2.0f;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    state.cube_vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
    state.bg_fs_params.tick += 1.0f;

    // render into all the offscreen render targets
    for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
        if (!state.fmt[i].valid) {
            continue;
        }
        sg_pixel_format fmt = (sg_pixel_format)i;
        sg_pixelformat_info fmt_info = sg_query_pixelformat(fmt);
        if (fmt_info.render) {
            sg_begin_pass(state.fmt[i].render_pass, &(sg_pass_action){0});
            sg_apply_pipeline(state.fmt[i].bg_render_pip);
            sg_apply_bindings(&state.bg_bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_bg_fs_params, &state.bg_fs_params, sizeof(state.bg_fs_params));
            sg_draw(0, 4, 1);
            sg_apply_pipeline(state.fmt[i].cube_render_pip);
            sg_apply_bindings(&state.cube_bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cube_vs_params, &state.cube_vs_params, sizeof(state.cube_vs_params));
            sg_draw(0, 36, 1);
            sg_end_pass();
        }
        if (fmt_info.blend) {
            sg_begin_pass(state.fmt[i].blend_pass, &(sg_pass_action){0});
            sg_apply_pipeline(state.fmt[i].bg_render_pip);  // not a bug
            sg_apply_bindings(&state.bg_bindings); // not a bug
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_bg_fs_params, &state.bg_fs_params, sizeof(state.bg_fs_params));
            sg_draw(0, 4, 1);
            sg_apply_pipeline(state.fmt[i].cube_blend_pip);
            sg_apply_bindings(&state.cube_bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cube_vs_params, &state.cube_vs_params, sizeof(state.cube_vs_params));
            sg_draw(0, 36, 1);
            sg_end_pass();
        }
        if (fmt_info.msaa) {
            sg_begin_pass(state.fmt[i].msaa_pass, &(sg_pass_action){0});
            sg_apply_pipeline(state.fmt[i].bg_msaa_pip);
            sg_apply_bindings(&state.bg_bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_bg_fs_params, &state.bg_fs_params, sizeof(state.bg_fs_params));
            sg_draw(0, 4, 1);
            sg_apply_pipeline(state.fmt[i].cube_msaa_pip);
            sg_apply_bindings(&state.cube_bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cube_vs_params, &state.cube_vs_params, sizeof(state.cube_vs_params));
            sg_draw(0, 36, 1);
            sg_end_pass();
        }
    }

    // ImGui rendering...
    simgui_new_frame(w, h, 1.0f/60.0f);
    igSetNextWindowSize((ImVec2){640, 480}, ImGuiCond_Once);
    if (igBegin("Pixel Formats (without UINT and SINT formats)", 0, 0)) {
        igText("format"); igSameLine(264, 0);
        igText("sample"); igSameLine(264 + 1*66, 0);
        igText("filter"); igSameLine(264 + 2*66, 0);
        igText("render"); igSameLine(264 + 3*66, 0);
        igText("blend");  igSameLine(264 + 4*66, 0);
        igText("msaa");
        igSeparator();
        igBeginChildStr("#scrollregion", (ImVec2){0,0}, false, ImGuiWindowFlags_None);
        for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
            if (!state.fmt[i].valid) {
                continue;
            }
            const char* fmt_string = pixelformat_string((sg_pixel_format)i);
            if (igBeginChildStr(fmt_string, (ImVec2){0,80}, false, ImGuiWindowFlags_NoMouseInputs|ImGuiWindowFlags_NoScrollbar)) {
                igText("%s", fmt_string);
                igSameLine(256, 0);
                igImage((ImTextureID)(uintptr_t)state.fmt[i].sample.id, (ImVec2){64,64}, (ImVec2){0,0}, (ImVec2){1,1}, (ImVec4){1,1,1,1}, (ImVec4){1,1,1,1});
                igSameLine(0,0);
                igImage((ImTextureID)(uintptr_t)state.fmt[i].filter.id, (ImVec2){64,64}, (ImVec2){0,0}, (ImVec2){1,1}, (ImVec4){1,1,1,1}, (ImVec4){1,1,1,1});
                igSameLine(0,0);
                igImage((ImTextureID)(uintptr_t)state.fmt[i].render.id, (ImVec2){64,64}, (ImVec2){0,0}, (ImVec2){1,1}, (ImVec4){1,1,1,1}, (ImVec4){1,1,1,1});
                igSameLine(0,0);
                igImage((ImTextureID)(uintptr_t)state.fmt[i].blend.id, (ImVec2){64,64}, (ImVec2){0,0}, (ImVec2){1,1}, (ImVec4){1,1,1,1}, (ImVec4){1,1,1,1});
                igSameLine(0,0);
                igImage((ImTextureID)(uintptr_t)state.fmt[i].msaa.id, (ImVec2){64,64}, (ImVec2){0,0}, (ImVec2){1,1}, (ImVec4){1,1,1,1}, (ImVec4){1,1,1,1});
            }
            igEndChild();
        }
        igEndChild();
    }
    igEnd();

    // sokol-gfx rendering...
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };
    sg_begin_default_pass(&pass_action, w, h);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* e) {
    simgui_handle_event(e);
}

static void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        #if defined(USE_GLES2)
        .gl_force_gles2 = true,
        #endif
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        #if defined(USE_GLES2)
        .window_title = "Pixelformat Test (GLES2)",
        #endif
    };
}

/* create a texture for "feature disabled) */
#define X 0xFF0000FF
#define o 0xFFCCCCCC
static const uint32_t disabled_texture_pixels[8][8] = {
    { X, o, o, o, o, o, o, X },
    { o, X, o, o, o, o, X, o },
    { o, o, X, o, o, X, o, o },
    { o, o, o, X, X, o, o, o },
    { o, o, o, X, X, o, o, o },
    { o, o, X, o, o, X, o, o },
    { o, X, o, o, o, o, X, o },
    { X, o, o, o, o, o, o, X }
};
#undef X
#undef o

static sg_image setup_invalid_texture(void) {
    return sg_make_image(&(sg_image_desc){
        .width = 8,
        .height = 8,
        .content.subimage[0][0] = {
            .ptr = disabled_texture_pixels,
            .size = sizeof(disabled_texture_pixels)
        }
    });
}

/* generate checkerboard pixel values */
static uint8_t pixels[8 * 8 * 16];

static void gen_pixels_8(uint8_t val) {
    uint8_t* ptr = pixels;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *ptr++ = ((x ^ y) & 1) ? val : 0;
        }
    }
}

static void gen_pixels_16(uint16_t val) {
    uint16_t* ptr = (uint16_t*) pixels;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *ptr++ = ((x ^ y) & 1) ? val : 0;
        }
    }
}

static void gen_pixels_32(uint32_t val) {
    uint32_t* ptr = (uint32_t*) pixels;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *ptr++ = ((x ^ y) & 1) ? val : 0;
        }
    }
}

static void gen_pixels_64(uint64_t val) {
    uint64_t* ptr = (uint64_t*) pixels;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *ptr++ = ((x ^ y) & 1) ? val : 0;
        }
    }
}

static void gen_pixels_128(uint64_t hi, uint64_t lo) {
    uint64_t* ptr = (uint64_t*) pixels;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            *ptr++ = ((x ^ y) & 1) ? lo : 0;
            *ptr++ = ((x ^ y) & 1) ? hi : 0;
        }
    }
}

static ptr_size_t gen_pixels(sg_pixel_format fmt) {
    /* NOTE the UI and SI (unsigned/signed) formats are not renderable
        with the ImGui shader, since that expects a texture which can be
        sampled into a float
    */
    switch (fmt) {
        case SG_PIXELFORMAT_R8:     gen_pixels_8(0xFF);     return (ptr_size_t) {pixels, 8*8};
        case SG_PIXELFORMAT_R8SN:   gen_pixels_8(0x7F);     return (ptr_size_t) {pixels, 8*8};
        case SG_PIXELFORMAT_R16:    gen_pixels_16(0xFFFF);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16SN:  gen_pixels_16(0x7FFF);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16F:   gen_pixels_16(0x3C00);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8:    gen_pixels_16(0xFFFF);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8SN:  gen_pixels_16(0x7F7F);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R32F:   gen_pixels_32(0x3F800000);  return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG16:   gen_pixels_32(0xFFFFFFFF);  return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG16SN: gen_pixels_32(0x7FFF7FFF);  return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG16F:      gen_pixels_32(0x3C003C00);  return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RGBA8:      gen_pixels_32(0xFFFFFFFF);  return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RGBA8SN:    gen_pixels_32(0x7F7F7F7F); return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_BGRA8:      gen_pixels_32(0xFFFFFFFF); return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RGB10A2:    gen_pixels_32(0x3<<30 | 0x3FF<<20 | 0x3FF<<10 | 0x3FF); return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG11B10F:   gen_pixels_32(0x1E0<<22 | 0x3C0<<11 | 0x3C0); return (ptr_size_t) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG32F:      gen_pixels_64(0x3F8000003F800000); return (ptr_size_t) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA16:     gen_pixels_64(0xFFFFFFFFFFFFFFFF); return (ptr_size_t) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA16SN:   gen_pixels_64(0x7FFF7FFF7FFF7FFF); return (ptr_size_t) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA16F:    gen_pixels_64(0x3C003C003C003C00); return (ptr_size_t) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA32F:    gen_pixels_128(0x3F8000003F800000, 0x3F8000003F800000); return (ptr_size_t) {pixels, 8*8*16};
        default: return (ptr_size_t){0,0};
    }
}

/* translate pixel format enum to string */
static const char* pixelformat_string(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_NONE: return "SG_PIXELFORMAT_NONE";
        case SG_PIXELFORMAT_R8: return "SG_PIXELFORMAT_R8";
        case SG_PIXELFORMAT_R8SN: return "SG_PIXELFORMAT_R8SN";
        case SG_PIXELFORMAT_R8UI: return "SG_PIXELFORMAT_R8UI";
        case SG_PIXELFORMAT_R8SI: return "SG_PIXELFORMAT_R8SI";
        case SG_PIXELFORMAT_R16: return "SG_PIXELFORMAT_R16";
        case SG_PIXELFORMAT_R16SN: return "SG_PIXELFORMAT_R16SN";
        case SG_PIXELFORMAT_R16UI: return "SG_PIXELFORMAT_R16UI";
        case SG_PIXELFORMAT_R16SI: return "SG_PIXELFORMAT_R16SI";
        case SG_PIXELFORMAT_R16F: return "SG_PIXELFORMAT_R16F";
        case SG_PIXELFORMAT_RG8: return "SG_PIXELFORMAT_RG8";
        case SG_PIXELFORMAT_RG8SN: return "SG_PIXELFORMAT_RG8SN";
        case SG_PIXELFORMAT_RG8UI: return "SG_PIXELFORMAT_RG8UI";
        case SG_PIXELFORMAT_RG8SI: return "SG_PIXELFORMAT_RG8SI";
        case SG_PIXELFORMAT_R32UI: return "SG_PIXELFORMAT_R32UI";
        case SG_PIXELFORMAT_R32SI: return "SG_PIXELFORMAT_R32SI";
        case SG_PIXELFORMAT_R32F: return "SG_PIXELFORMAT_R32F";
        case SG_PIXELFORMAT_RG16: return "SG_PIXELFORMAT_RG16";
        case SG_PIXELFORMAT_RG16SN: return "SG_PIXELFORMAT_RG16SN";
        case SG_PIXELFORMAT_RG16UI: return "SG_PIXELFORMAT_RG16UI";
        case SG_PIXELFORMAT_RG16SI: return "SG_PIXELFORMAT_RG16SI";
        case SG_PIXELFORMAT_RG16F: return "SG_PIXELFORMAT_RG16F";
        case SG_PIXELFORMAT_RGBA8: return "SG_PIXELFORMAT_RGBA8";
        case SG_PIXELFORMAT_RGBA8SN: return "SG_PIXELFORMAT_RGBA8SN";
        case SG_PIXELFORMAT_RGBA8UI: return "SG_PIXELFORMAT_RGBA8UI";
        case SG_PIXELFORMAT_RGBA8SI: return "SG_PIXELFORMAT_RGBA8SI";
        case SG_PIXELFORMAT_BGRA8: return "SG_PIXELFORMAT_BGRA8";
        case SG_PIXELFORMAT_RGB10A2: return "SG_PIXELFORMAT_RGB10A2";
        case SG_PIXELFORMAT_RG11B10F: return "SG_PIXELFORMAT_RG11B10F";
        case SG_PIXELFORMAT_RG32UI: return "SG_PIXELFORMAT_RG32UI";
        case SG_PIXELFORMAT_RG32SI: return "SG_PIXELFORMAT_RG32SI";
        case SG_PIXELFORMAT_RG32F: return "SG_PIXELFORMAT_RG32F";
        case SG_PIXELFORMAT_RGBA16: return "SG_PIXELFORMAT_RGBA16";
        case SG_PIXELFORMAT_RGBA16SN: return "SG_PIXELFORMAT_RGBA16SN";
        case SG_PIXELFORMAT_RGBA16UI: return "SG_PIXELFORMAT_RGBA16UI";
        case SG_PIXELFORMAT_RGBA16SI: return "SG_PIXELFORMAT_RGBA16SI";
        case SG_PIXELFORMAT_RGBA16F: return "SG_PIXELFORMAT_RGBA16F";
        case SG_PIXELFORMAT_RGBA32UI: return "SG_PIXELFORMAT_RGBA32UI";
        case SG_PIXELFORMAT_RGBA32SI: return "SG_PIXELFORMAT_RGBA32SI";
        case SG_PIXELFORMAT_RGBA32F: return "SG_PIXELFORMAT_RGBA32F";
        case SG_PIXELFORMAT_DEPTH: return "SG_PIXELFORMAT_DEPTH";
        case SG_PIXELFORMAT_DEPTH_STENCIL: return "SG_PIXELFORMAT_DEPTH_STENCIL";
        case SG_PIXELFORMAT_BC1_RGBA: return "SG_PIXELFORMAT_BC1_RGBA";
        case SG_PIXELFORMAT_BC2_RGBA: return "SG_PIXELFORMAT_BC2_RGBA";
        case SG_PIXELFORMAT_BC3_RGBA: return "SG_PIXELFORMAT_BC3_RGBA";
        case SG_PIXELFORMAT_BC4_R: return "SG_PIXELFORMAT_BC4_R";
        case SG_PIXELFORMAT_BC4_RSN: return "SG_PIXELFORMAT_BC4_RSN";
        case SG_PIXELFORMAT_BC5_RG: return "SG_PIXELFORMAT_BC5_RG";
        case SG_PIXELFORMAT_BC5_RGSN: return "SG_PIXELFORMAT_BC5_RGSN";
        case SG_PIXELFORMAT_BC6H_RGBF: return "SG_PIXELFORMAT_BC6H_RGBF";
        case SG_PIXELFORMAT_BC6H_RGBUF: return "SG_PIXELFORMAT_BC6H_RGBUF";
        case SG_PIXELFORMAT_BC7_RGBA: return "SG_PIXELFORMAT_BC7_RGBA";
        case SG_PIXELFORMAT_PVRTC_RGB_2BPP: return "SG_PIXELFORMAT_PVRTC_RGB_2BPP";
        case SG_PIXELFORMAT_PVRTC_RGB_4BPP: return "SG_PIXELFORMAT_PVRTC_RGB_4BPP";
        case SG_PIXELFORMAT_PVRTC_RGBA_2BPP: return "SG_PIXELFORMAT_PVRTC_RGBA_2BPP";
        case SG_PIXELFORMAT_PVRTC_RGBA_4BPP: return "SG_PIXELFORMAT_PVRTC_RGBA_4BPP";
        case SG_PIXELFORMAT_ETC2_RGB8: return "SG_PIXELFORMAT_ETC2_RGB8";
        case SG_PIXELFORMAT_ETC2_RGB8A1: return "SG_PIXELFORMAT_ETC2_RGB8A1";
        default: return "???";
    }
}
