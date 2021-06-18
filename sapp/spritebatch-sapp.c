//------------------------------------------------------------------------------
//  spritebatch-sapp.c
//------------------------------------------------------------------------------

#include "sokol_time.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_SPRITEBATCH_IMPL
#include "sokol_spritebatch.h"
#define SOKOL_COLOR_IMPL
#include "sokol_color.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

static struct {
    uint64_t time;
    sg_image tiles;
    sg_image horse;
    sg_pass_action pass_action;
    float elapsed_seconds;
    float frame_time;
    int frame;
} state;

static sg_image load_sprite(const char* filepath, sg_filter filter) {

    int width, height, num_channels;
    const int desired_channels = 4;

    stbi_uc* pixels = stbi_load(filepath,
                                &width, &height,
                                &num_channels, desired_channels);
    assert(pixels != NULL);
    sb_premultiply_alpha(pixels, width * height);

    const sg_image image = sg_make_image(&(sg_image_desc) {
        .width        = width,
        .height       = height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter   = filter,
        .mag_filter   = filter,
        .wrap_u       = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v       = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr  = pixels,
            .size = (size_t)width * height * 4
        }
    });
    assert(image.id != SG_INVALID_ID);

    stbi_image_free(pixels);

    return image;
}

void init(void) {
    stm_setup();

    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = sg_black }
    };

    sb_setup(&(sb_desc) { 0 });

    state.tiles = load_sprite("tiles_packed.png", SG_FILTER_LINEAR);
    state.horse = load_sprite("nightmare-galloping.png", SG_FILTER_NEAREST);
}

void frame(void) {

    const float delta_time = (float)stm_sec(stm_laptime(&state.time));
    state.elapsed_seconds += delta_time;

    const int width = sapp_width();
    const int height = sapp_height();

    const int sprite_width = 368;
    const int sprite_height = 128;

    sb_begin(&(sb_render_state) {
        .sort_mode = SB_SORT_MODE_FRONT_TO_BACK,
        .viewport = {
            .width  = width,
            .height = height
        }
    });

    for (size_t x = 0; x < 4; x++) {
        for (size_t y = 0; y < 4; y++) {
            const sg_color color = sg_color_lerp(&sg_blue, &sg_red, (float)x / 4.0f);
            const bool is_even = (x + y) % 2 == 0;
            sb_sprite(&(sb_sprite_info) {
                .image      = state.tiles,
                .position.x = (float)sprite_width  * (float)x + 128.0f,
                .position.y = (float)sprite_height * (float)y + 128.0f,
                .rotation   = state.elapsed_seconds,
                .origin.x   = (float)sprite_width  / 2.0f,
                .origin.y   = (float)sprite_height / 2.0f,
                .flags      = is_even ? SB_FLIP_BOTH : SB_FLIP_NONE,
                .color      = color
            });
        }
    }

    for (size_t x = 0; x < sprite_width / 16; x++) {
        for (size_t y = 0; y < sprite_height / 16; y++) {
            sb_sprite(&(sb_sprite_info) {
                .image      = state.tiles,
                .position.x = 16.0f * (float)x + 2.0f * (float)x + 800.0f,
                .position.y = 16.0f * (float)y + 2.0f * (float)y + 256.0f,
                .width      = 16.0f,
                .height     = 16.0f,
                .source.x   = 16.0f * (float)x,
                .source.y   = 16.0f * (float)y,
                .depth      = -1.0f
            });
        }
    }

    const int horse_width = 576;
    const int frame_width = horse_width / 4;

    for (int i = 0; i < width / frame_width + 1; ++i) {
        sb_sprite(&(sb_sprite_info) {
            .image      = state.horse,
            .position.x = (float)i * (float)frame_width,
            .position.y = 128.0f,
            .source.x   = 0.0f + (float)state.frame * (float)frame_width,
            .source.y   = 0.0f,
            .origin.y   = 1.0f,
            .origin.x   = 0.5f,
            .width      = (float)frame_width,
            .height     = 96.0f
        });
    }

    sb_sprite(&(sb_sprite_info) {
        .image      = state.tiles,
        .position.y = 256.0f,
        .scale.x    = 2.0f,
        .scale.y    = 2.0f,
    });

    const int scaled_frame_width = frame_width * 2;

    for (int i = 0; i < width / scaled_frame_width + 1; ++i) {
        sb_sprite(&(sb_sprite_info) {
            .image      = state.horse,
            .position.x = (float)i * (float)scaled_frame_width,
            .position.y = 384.0f,
            .source.x   = (float)state.frame * (float)frame_width,
            .source.y   = 0.0f,
            .width      = (float)frame_width,
            .height     = 96.0f,
            .scale.x    = 2.0f,
            .scale.y    = 2.0f,
            .origin.y   = 1.0f,
            .origin.x   = 0.5f,
            .flags      = SB_FLIP_X | SB_Z_TILT
        });
    }

    sb_end();

    sg_begin_default_pass(&state.pass_action, width, height);
    sb_draw();
    __dbgui_draw();
    sg_end_pass();

    sg_commit();

    state.frame_time += delta_time;
    if (state.frame_time >= 1.0f / 12.0f) {
        state.frame = (state.frame + 1) % 4;
        state.frame_time = 0.0f;
    }
}

void cleanup(void) {
    sg_destroy_image(state.tiles);
    sg_destroy_image(state.horse);
    __dbgui_shutdown();
    sb_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = __dbgui_event,
        .width = 1280,
        .height = 720,
        .gl_force_gles2 = true,
        .window_title = "SpriteBatch (sokol-app)",
        .icon.sokol_default = true,
    };
}
