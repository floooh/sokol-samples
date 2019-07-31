//------------------------------------------------------------------------------
//  pixelformats-sapp.c
//  Test pixelformat capabilities.
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#define SOKOL_CIMGUI_IMPL
#include "sokol_cimgui.h"

static struct {
    sg_image img_invalid;
    struct {
        sg_image img_sample;
        sg_image img_filter;
        sg_image img_render;
        sg_image img_msaa;
    } items[_SG_PIXELFORMAT_NUM];
    sg_pass_action pass_action;
} state;

static const char* pixelformat_string(sg_pixel_format fmt);
static void create_disabled_texture(void);

static void init(void) {
    sg_setup(&(sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });
    scimgui_setup(&(scimgui_desc_t){0});
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.0f, 0.5f, 0.7f, 1.0f } }
    };

    create_disabled_texture();


    //
}

static void frame(void) {
    const int w = sapp_width();
    const int h = sapp_height();
    scimgui_new_frame(w, h, 1.0f/60.0f);

    igSetNextWindowSize((ImVec2){420, 256}, ImGuiCond_Once);
    if (igBegin("Pixel Formats", 0, 0)) {
        for (int i = SG_PIXELFORMAT_NONE+1; i < SG_PIXELFORMAT_DEPTH; i++) {
            const char* fmt_string = pixelformat_string((sg_pixel_format)i);
            if (igBeginChild(fmt_string, (ImVec2){0,48}, true, ImGuiWindowFlags_NoMouseInputs|ImGuiWindowFlags_NoScrollbar)) {
                igText("%s", fmt_string);
                igSameLine(256.0f, 0.0f);
                igImage(state.img_invalid.id, (ImVec2){32,32}, (ImVec2){0,0}, (ImVec2){1,1}, (ImVec4){1,1,1,1}, (ImVec4){1,1,1,1});
            }
            igEndChild();
        }
    }
    igEnd();

    sg_begin_default_pass(&state.pass_action, w, h);
    scimgui_render();
    sg_end_pass();
    sg_commit();
}

static void input(const sapp_event* e) {
    scimgui_handle_event(e);
}

static void cleanup(void) {
    scimgui_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "Pixelformat Test",
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

static void create_disabled_texture(void) {
    state.img_invalid = sg_make_image(&(sg_image_desc){
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

typedef struct {
    void* ptr;
    int size;
} ptr_size_t;

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

static ptr_size_t gen_pixels(sg_pixel_format fmt, int* out_num_bytes) {
    switch (fmt) {
        case SG_PIXELFORMAT_R8:     gen_pixels_8(0xFF);     return (ptr_size_t) {pixels, 8*8};
        case SG_PIXELFORMAT_R8SN:   gen_pixels_8(0x7F);     return (ptr_size_t) {pixels, 8*8};
        case SG_PIXELFORMAT_R8UI:   gen_pixels_8(1);        return (ptr_size_t) {pixels, 8*8};
        case SG_PIXELFORMAT_R8SI:   gen_pixels_8(1);        return (ptr_size_t) {pixels, 8*8};
        case SG_PIXELFORMAT_R16:    gen_pixels_16(0xFFFF);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16SN:  gen_pixels_16(0x7FFF);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16UI:  gen_pixels_16(1);       return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16SI:  gen_pixels_16(1);       return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_R16F:   gen_pixels_16(0x3C00);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8:    gen_pixels_16(0xFFFF);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8SN:  gen_pixels_16(0x7F7F);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8UI:  gen_pixels_16(0x0101);  return (ptr_size_t) {pixels, 8*8*2};
        case SG_PIXELFORMAT_RG8SI:  gen_pixels_16(0x0101);  return (ptr_size_t) {pixels, 8*8*2};
        default: return (ptr_size_t){pixels, 0};
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
