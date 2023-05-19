//------------------------------------------------------------------------------
//  metal-glfw.c
//
//  Demonstrates how to use GLFW with the sokol_gfx.h Metal backend.
//
//  NOTE: This example does *NOT* create and manage a depth-stencil-buffer,
//  so it's quite useless for 3D rendering.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol_gfx.h"
#include "sokol_log.h"
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

static CAMetalLayer* layer;
static id<CAMetalDrawable> next_drawable;

// callback to obtain MTLRenderPassDescriptor, called from sg_begin_default_pass()
static const void* get_render_pass_descriptor(void) {
    next_drawable = [layer nextDrawable];
    MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
    pass_desc.colorAttachments[0].texture = next_drawable.texture;
    return (__bridge const void*) pass_desc;
}

// callback to obtain next drawable, called from sg_commit()
static const void* get_next_drawable(void) {
    return (__bridge const void*) next_drawable;
}

int main(void) {
    // create Metal device and layer
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    layer = [CAMetalLayer layer];
    layer.device = device;
    layer.opaque = YES;

    // create GLFW window without GL context
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(640, 480, "GLFW + sokol_gfx.h + Metal", NULL, NULL);
    NSWindow* nswindow = glfwGetCocoaWindow(window);

    // attach CAMetalLayer to window
    nswindow.contentView.layer = layer;
    nswindow.contentView.wantsLayer = YES;

    // setup sokol-gfx, pass relevant Metal objects and callbacks, also tell
    // sokol-gfx that there's no default depth-stencil-buffer
    sg_setup(&(sg_desc){
        .context = {
            .depth_format = SG_PIXELFORMAT_NONE,
            .metal = {
                .device = (__bridge const void*) device,
                .renderpass_descriptor_cb = get_render_pass_descriptor,
                .drawable_cb = get_next_drawable,
            }
        },
        .logger = {
            .func = slog_func
        }
    });

    // a vertex buffer with 3 vertices
    float vertices[] = {
        // positions        colors
         0.0f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f
    };
    sg_bindings bindings = {
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(vertices)
        })
    };

    // a shader pair, compiled from source code
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        /*
            The shader main() function cannot be called 'main' in
            the Metal shader languages, thus we define '_main' as the
            default function. This can be override with the
            sg_shader_desc.vs.entry and sg_shader_desc.fs.entry fields.
        */
        .vs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct vs_in {\n"
            "  float4 position [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 position [[position]];\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(vs_in inp [[stage_in]]) {\n"
            "  vs_out outp;\n"
            "  outp.position = inp.position;\n"
            "  outp.color = inp.color;\n"
            "  return outp;\n"
            "}\n",
        .fs.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "};\n"
    });

    // create a pipeline object
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        /* Metal has explicit attribute locations, and the vertex layout
           has no gaps, so we don't need to provide stride, offsets
           or attribute names
        */
        .layout = {
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 }
            },
        },
        .shader = shd
    });

    // a pass action to clear to black
    const sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    // the GLFW render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // the sokol-gfx frame needs to run in an autoreleasepool
        @autoreleasepool {
            const CGSize fb_size = layer.drawableSize;
            sg_begin_default_pass(&pass_action, fb_size.width, fb_size.height);
            sg_apply_pipeline(pip);
            sg_apply_bindings(&bindings);
            sg_draw(0, 3, 1);
            sg_end_pass();
            sg_commit();
        }
    }
    sg_shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
