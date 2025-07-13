//------------------------------------------------------------------------------
//  pixelformats-sapp.c
//  Test pixelformat capabilities.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "cimgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "pixelformats-sapp.glsl.h"

typedef struct {
    sg_image img;
    sg_view tex_view;
    sg_view att_view;
} image_and_views_t;

static struct {
    struct {
        bool valid;
        image_and_views_t unfiltered;
        image_and_views_t filtered;
        image_and_views_t render;
        image_and_views_t blend;
        image_and_views_t msaa_render;
        image_and_views_t msaa_resolve;
        sg_pipeline cube_render_pip;
        sg_pipeline cube_blend_pip;
        sg_pipeline cube_msaa_pip;
        sg_pipeline bg_render_pip;
        sg_pipeline bg_msaa_pip;
    } fmt[_SG_PIXELFORMAT_NUM];
    sg_view depth_att_view;
    sg_view msaa_depth_att_view;
    sg_sampler smp_linear;
    sg_bindings cube_bindings;
    sg_bindings bg_bindings;
    float rx, ry;
    cube_vs_params_t cube_vs_params;
    bg_fs_params_t bg_fs_params;
} state;

static image_and_views_t make_image_and_views(const sg_image_desc* img_desc, bool has_tex_view, sg_view_type att_view_type);
static const char* pixelformat_string(sg_pixel_format fmt);
static sg_range gen_pixels(sg_pixel_format fmt);

// helper function to construct ImTextureRef from ImTextureID
// FIXME: remove when Dear Bindings offers such helper
static ImTextureRef imtexref(ImTextureID tex_id) {
    return (ImTextureRef){ ._TexID = tex_id };
}

// a 'disabled' texture pattern with a cross
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

static void init(void) {
    // setup sokol-gfx
    sg_setup(&(sg_desc){
        .pipeline_pool_size = 256,
        .image_pool_size = 256,
        .view_pool_size = 512,
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    // setup cimgui
    simgui_setup(&(simgui_desc_t){
        .logger.func = slog_func,
    });

    // create all the textures, samplers and render targets
    state.depth_att_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment = {
            .image = sg_make_image(&(sg_image_desc){
                .usage.depth_stencil_attachment = true,
                .width = 64,
                .height = 64,
                .pixel_format = SG_PIXELFORMAT_DEPTH,
                .sample_count = 1,
            }),
        },
    });
    state.msaa_depth_att_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment = {
            .image = sg_make_image(&(sg_image_desc){
                .usage.depth_stencil_attachment = true,
                .width = 64,
                .height = 64,
                .pixel_format = SG_PIXELFORMAT_DEPTH,
                .sample_count = 4,
            }),
        },
    });
    const image_and_views_t invalid_img = make_image_and_views(&(sg_image_desc){
        .width = 8,
        .height = 8,
        .data.subimage[0][0] = SG_RANGE(disabled_texture_pixels)
    }, true, SG_VIEWTYPE_INVALID);
    for (int i = 0; i < _SG_PIXELFORMAT_NUM; i++) {
        state.fmt[i].unfiltered = invalid_img;
        state.fmt[i].filtered = invalid_img;
        state.fmt[i].render = invalid_img;
        state.fmt[i].blend  = invalid_img;
        state.fmt[i].msaa_resolve = invalid_img;
    }

    state.smp_linear = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    sg_pipeline_desc cube_render_pip_desc = {
        .layout = {
            .attrs = {
                [ATTR_cube_pos].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_cube_color0].format = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = sg_make_shader(cube_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .sample_count = 1,
        .depth = {
            .write_enabled = true,
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
    };
    sg_pipeline_desc bg_render_pip_desc = {
        .layout.attrs[ATTR_bg_position].format = SG_VERTEXFORMAT_FLOAT2,
        .shader = sg_make_shader(bg_shader_desc(sg_query_backend())),
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .sample_count = 1,
        .depth.pixel_format = SG_PIXELFORMAT_DEPTH,
    };
    sg_pipeline_desc cube_blend_pip_desc = cube_render_pip_desc;
    cube_blend_pip_desc.colors[0].blend = (sg_blend_state) {
        .enabled = true,
        .src_factor_rgb = SG_BLENDFACTOR_ONE,
        .dst_factor_rgb = SG_BLENDFACTOR_ONE,
    };
    sg_pipeline_desc cube_msaa_pip_desc = cube_render_pip_desc;
    sg_pipeline_desc bg_msaa_pip_desc = bg_render_pip_desc;
    cube_msaa_pip_desc.sample_count = 4;
    bg_msaa_pip_desc.sample_count = 4;

    for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
        sg_pixel_format fmt = (sg_pixel_format)i;
        sg_range img_data = gen_pixels(fmt);
        if (img_data.ptr) {
            state.fmt[i].valid = true;
            const sg_pixelformat_info fmt_info = sg_query_pixelformat(fmt);
            // create unfiltered and filtered texture and associated views
            if (fmt_info.sample) {
                image_and_views_t img = make_image_and_views(&(sg_image_desc){
                    .width = 8,
                    .height = 8,
                    .pixel_format = fmt,
                    .data.subimage[0][0] = img_data,
                }, true, SG_VIEWTYPE_INVALID);
                state.fmt[i].unfiltered = img;
                if (fmt_info.filter) {
                    state.fmt[i].filtered = img;
                }
            }

            // create non-MSAA render target, pipeline state and pass-attachments
            if (fmt_info.render) {
                state.fmt[i].render = make_image_and_views(&(sg_image_desc){
                    .usage.color_attachment = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                    .sample_count = 1,
                }, true, SG_VIEWTYPE_COLORATTACHMENT);
                cube_render_pip_desc.colors[0].pixel_format = fmt;
                bg_render_pip_desc.colors[0].pixel_format = fmt;
                state.fmt[i].cube_render_pip = sg_make_pipeline(&cube_render_pip_desc);
                state.fmt[i].bg_render_pip = sg_make_pipeline(&bg_render_pip_desc);
            }
            // create non-MSAA blend render target, pipeline states and pass-attachments
            if (fmt_info.blend) {
                state.fmt[i].blend = make_image_and_views(&(sg_image_desc){
                    .usage.color_attachment = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                    .sample_count = 1,
                }, true, SG_VIEWTYPE_COLORATTACHMENT);
                cube_blend_pip_desc.colors[0].pixel_format = fmt;
                state.fmt[i].cube_blend_pip = sg_make_pipeline(&cube_blend_pip_desc);
            }
            // create MSAA render target, resolve texture and matching pipeline state
            if (fmt_info.msaa) {
                state.fmt[i].msaa_render = make_image_and_views(&(sg_image_desc){
                    .usage.color_attachment = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                    .sample_count = 4,
                }, false, SG_VIEWTYPE_COLORATTACHMENT);
                state.fmt[i].msaa_resolve = make_image_and_views(&(sg_image_desc){
                    .usage.resolve_attachment = true,
                    .width = 64,
                    .height = 64,
                    .pixel_format = fmt,
                    .sample_count = 1,
                }, true, SG_VIEWTYPE_RESOLVEATTACHMENT);
                cube_msaa_pip_desc.colors[0].pixel_format = fmt;
                bg_msaa_pip_desc.colors[0].pixel_format = fmt;
                state.fmt[i].cube_msaa_pip = sg_make_pipeline(&cube_msaa_pip_desc);
                state.fmt[i].bg_msaa_pip = sg_make_pipeline(&bg_msaa_pip_desc);
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
        .data = SG_RANGE(cube_vertices)
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
        .usage.index_buffer = true,
        .data = SG_RANGE(cube_indices)
    });

    // background quad vertices
    float vertices[] = { -1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f, };
    state.bg_bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });
}

static void frame(void) {

    const int w = sapp_width();
    const int h = sapp_height();
    const float t = (float)(sapp_frame_duration() * 60.0);

    // compute the model-view-proj matrix for rendering to render targets
    hmm_mat4 proj = HMM_Perspective(60.0f, 1.0f, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    state.cube_vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
    state.bg_fs_params.tick += 1.0f * t;

    // render into all the offscreen render targets
    for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
        if (!state.fmt[i].valid) {
            continue;
        }
        sg_pixel_format fmt = (sg_pixel_format)i;
        sg_pixelformat_info fmt_info = sg_query_pixelformat(fmt);
        if (fmt_info.render) {
            sg_begin_pass(&(sg_pass){
                .attachments = {
                    .colors[0] = state.fmt[i].render.att_view,
                    .depth_stencil = state.depth_att_view,
                },
            });
            sg_apply_pipeline(state.fmt[i].bg_render_pip);
            sg_apply_bindings(&state.bg_bindings);
            sg_apply_uniforms(UB_bg_fs_params, &SG_RANGE(state.bg_fs_params));
            sg_draw(0, 4, 1);
            sg_apply_pipeline(state.fmt[i].cube_render_pip);
            sg_apply_bindings(&state.cube_bindings);
            sg_apply_uniforms(UB_cube_vs_params, &SG_RANGE(state.cube_vs_params));
            sg_draw(0, 36, 1);
            sg_end_pass();
        }
        if (fmt_info.blend) {
            sg_begin_pass(&(sg_pass){
                .attachments = {
                    .colors[0] = state.fmt[i].blend.att_view,
                    .depth_stencil = state.depth_att_view,
                },
            });
            sg_apply_pipeline(state.fmt[i].bg_render_pip);  // not a bug
            sg_apply_bindings(&state.bg_bindings); // not a bug
            sg_apply_uniforms(UB_bg_fs_params, &SG_RANGE(state.bg_fs_params));
            sg_draw(0, 4, 1);
            sg_apply_pipeline(state.fmt[i].cube_blend_pip);
            sg_apply_bindings(&state.cube_bindings);
            sg_apply_uniforms(UB_cube_vs_params, &SG_RANGE(state.cube_vs_params));
            sg_draw(0, 36, 1);
            sg_end_pass();
        }
        if (fmt_info.msaa) {
            sg_begin_pass(&(sg_pass){
                .attachments = {
                    .colors[0] = state.fmt[i].msaa_render.att_view,
                    .resolves[0] = state.fmt[i].msaa_resolve.att_view,
                    .depth_stencil = state.msaa_depth_att_view,
                },
                .action = {
                    .colors[0].store_action = SG_STOREACTION_DONTCARE,
                },
            });
            sg_apply_pipeline(state.fmt[i].bg_msaa_pip);
            sg_apply_bindings(&state.bg_bindings);
            sg_apply_uniforms(UB_bg_fs_params, &SG_RANGE(state.bg_fs_params));
            sg_draw(0, 4, 1);
            sg_apply_pipeline(state.fmt[i].cube_msaa_pip);
            sg_apply_bindings(&state.cube_bindings);
            sg_apply_uniforms(UB_cube_vs_params, &SG_RANGE(state.cube_vs_params));
            sg_draw(0, 36, 1);
            sg_end_pass();
        }
    }

    // ImGui rendering...
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = w,
        .height = h,
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale()
    });
    igSetNextWindowSize((ImVec2){640, 480}, ImGuiCond_Once);
    if (igBegin("Pixel Formats (without UINT and SINT formats)", 0, 0)) {
        igText("format"); igSameLineEx(264, 0);
        igText("sample"); igSameLineEx(264 + 1*66, 0);
        igText("filter"); igSameLineEx(264 + 2*66, 0);
        igText("render"); igSameLineEx(264 + 3*66, 0);
        igText("blend");  igSameLineEx(264 + 4*66, 0);
        igText("msaa");
        igSeparator();
        igBeginChild("#scrollregion", (ImVec2){0,0}, false, ImGuiWindowFlags_None);
        for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
            if (!state.fmt[i].valid) {
                continue;
            }
            const char* fmt_string = pixelformat_string((sg_pixel_format)i);
            if (igBeginChild(fmt_string, (ImVec2){0,80}, false, ImGuiWindowFlags_NoMouseInputs|ImGuiWindowFlags_NoScrollbar)) {
                igText("%s", fmt_string);
                igSameLineEx(256, 0);
                igImage(imtexref(simgui_imtextureid(state.fmt[i].unfiltered.tex_view)), (ImVec2){64,64});
                igSameLine();
                igImage(imtexref(simgui_imtextureid_with_sampler(state.fmt[i].filtered.tex_view, state.smp_linear)), (ImVec2){64,64});
                igSameLine();
                igImage(imtexref(simgui_imtextureid(state.fmt[i].render.tex_view)), (ImVec2){64,64});
                igSameLine();
                igImage(imtexref(simgui_imtextureid(state.fmt[i].blend.tex_view)), (ImVec2){64,64});
                igSameLine();
                igImage(imtexref(simgui_imtextureid(state.fmt[i].msaa_resolve.tex_view)), (ImVec2){64,64});
            }
            igEndChild();
        }
        igEndChild();
    }
    igEnd();

    // sokol-gfx rendering...
    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 0.7f, 1.0f } }
        },
        .swapchain = sglue_swapchain()
    });
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
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .icon.sokol_default = true,
        .window_title = "Pixelformat Test",
        .logger.func = slog_func,
    };
}

// create image object and associated views
static image_and_views_t make_image_and_views(const sg_image_desc* img_desc, bool has_tex_view, sg_view_type att_view_type) {
    image_and_views_t res = { .img = sg_make_image(img_desc) };
    if (has_tex_view) {
        res.tex_view = sg_make_view(&(sg_view_desc){
            .texture_binding = { .image = res.img },
        });
    }
    if (att_view_type == SG_VIEWTYPE_COLORATTACHMENT) {
        res.att_view = sg_make_view(&(sg_view_desc){
            .color_attachment = { .image = res.img },
        });
    } else if (att_view_type == SG_VIEWTYPE_RESOLVEATTACHMENT) {
        res.att_view = sg_make_view(&(sg_view_desc){
            .resolve_attachment = { .image = res.img },
        });
    }
    return res;
}


// generate checkerboard pixel values
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

static sg_range gen_pixels(sg_pixel_format fmt) {
    /* NOTE the UI and SI (unsigned/signed) formats are not renderable
        with the ImGui shader, since that expects a texture which can be
        sampled into a float
    */
    switch (fmt) {
        case SG_PIXELFORMAT_R8:     gen_pixels_8(0xFF);     return (sg_range) {pixels, 8*8};
        case SG_PIXELFORMAT_R8SN:   gen_pixels_8(0x7F);     return (sg_range) {pixels, 8*8};
        case SG_PIXELFORMAT_R16:    gen_pixels_16(0xFFFF);  return (sg_range) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16SN:  gen_pixels_16(0x7FFF);  return (sg_range) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16F:   gen_pixels_16(0x3C00);  return (sg_range) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8:    gen_pixels_16(0xFFFF);  return (sg_range) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8SN:  gen_pixels_16(0x7F7F);  return (sg_range) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R32F:   gen_pixels_32(0x3F800000);  return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG16:   gen_pixels_32(0xFFFFFFFF);  return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG16SN: gen_pixels_32(0x7FFF7FFF);  return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG16F:      gen_pixels_32(0x3C003C00);  return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RGBA8:      gen_pixels_32(0xFFFFFFFF);  return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_SRGB8A8:    gen_pixels_32(0xFFFFFFFF);  return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RGBA8SN:    gen_pixels_32(0x7F7F7F7F); return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_BGRA8:      gen_pixels_32(0xFFFFFFFF); return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RGB10A2:    gen_pixels_32((uint32_t)(0x3<<30 | 0x3FF<<20 | 0x3FF<<10 | 0x3FF)); return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG11B10F:   gen_pixels_32(0x1E0<<22 | 0x3C0<<11 | 0x3C0); return (sg_range) {pixels, 8*8*4};
        case SG_PIXELFORMAT_RG32F:      gen_pixels_64(0x3F8000003F800000); return (sg_range) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA16:     gen_pixels_64(0xFFFFFFFFFFFFFFFF); return (sg_range) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA16SN:   gen_pixels_64(0x7FFF7FFF7FFF7FFF); return (sg_range) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA16F:    gen_pixels_64(0x3C003C003C003C00); return (sg_range) {pixels, 8*8*8};
        case SG_PIXELFORMAT_RGBA32F:    gen_pixels_128(0x3F8000003F800000, 0x3F8000003F800000); return (sg_range) {pixels, 8*8*16};
        default: return (sg_range){0,0};
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
        case SG_PIXELFORMAT_SRGB8A8: return "SG_PIXELFORMAT_SRGB8A8";
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
        case SG_PIXELFORMAT_ETC2_RGB8: return "SG_PIXELFORMAT_ETC2_RGB8";
        case SG_PIXELFORMAT_ETC2_RGB8A1: return "SG_PIXELFORMAT_ETC2_RGB8A1";
        default: return "???";
    }
}
