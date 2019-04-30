//------------------------------------------------------------------------------
//  quad-sapp.c
//  Simple 2D rendering with vertex- and index-buffer.
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "ui/dbgui.h"
#include "quad-sapp.glsl.h"

static sg_pass_action pass_action = {
    .colors[0] = { .action=SG_ACTION_CLEAR, .val={0.0f, 0.0f, 0.0f, 1.0f } }
};
static sg_pipeline pip;
static sg_bindings bind;

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
    __dbgui_setup(1);

    /* a vertex buffer */
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,        
    };
    bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
        .label = "quad-vertices"
    });

    /* an index buffer with 2 triangles */
    uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
        .label = "quad-indices"
    });

    /* a shader (use separate shader sources here */
    sg_shader shd = sg_make_shader(&quad_shader_desc);

    /* a pipeline state object */
    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [vs_position].format = SG_VERTEXFORMAT_FLOAT3,
                [vs_color0].format   = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .label = "quad-pipeline"
    });
}

void frame(void) {
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_draw(0, 6, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 800,
        .height = 600,
        .gl_force_gles2 = true,
        .window_title = "Quad (sokol-app)",
    };
}
