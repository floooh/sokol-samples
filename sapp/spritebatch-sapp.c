//------------------------------------------------------------------------------
//  spritebatch-sapp.c
//------------------------------------------------------------------------------

/*
 * All artwork used in this sample was released into the public domain.
 * Luis Zuno (ansimuz):
 * https://opengameart.org/content/gothicvania-cemetery-pack
 * https://opengameart.org/content/gothicvania-patreons-collection
 * Michele Bucelli (Buch):
 * https://opengameart.org/content/shiny-hud
 */

#define SOKOL_SPRITEBATCH_IMPL
#define SOKOL_COLOR_IMPL
#define GAMEPLAY_WIDTH (480)
#define GAMEPLAY_HEIGHT (270)

#include "sokol_time.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_spritebatch.h"
#include "sokol_color.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "stb/stb_image.h"
#include "spritebatch-sapp-crt.glsl.h"

typedef struct bg_tile {
    sbatch_float2 position;
    sbatch_rect source;
} bg_tile;

typedef struct scroll_layer {
    sbatch_rect source;
    sbatch_float2 position;
    int repeat_count;
} scroll_layer;

typedef struct animation {
    sbatch_rect source;
    sbatch_float2 position;
    float frame_time;
    int frame_count;
    int frame_index;
    sg_color color;
    uint32_t mask;
} animation;

static struct {
    uint64_t last_time;
    float accumulator;

    int screen_width;
    int screen_height;
    int viewport_width;
    int viewport_height;
    int viewport_x;
    int viewport_y;

    sg_image atlas;
    scroll_layer mountains;
    scroll_layer graveyard;
    scroll_layer objects;
    scroll_layer tiles;

    animation nightmare;
    animation demon;

    sg_pass_action pass_action;

    sbatch_pipeline gameplay_pipeline;
    sg_image gameplay_render_target;
    sbatch_context gameplay_context;
    sg_pass gameplay_pass;

    sg_shader crt_shader;
    sbatch_pipeline crt_pipeline;
    sbatch_context crt_context;
    crt_fs_params_t crt_params;
} state;

static sg_image load_sprite(const char* filepath, const char* label, sg_filter filter, sg_wrap wrap) {
    int w, h, num_channels;
    const int desired_channels = 4;

    stbi_uc* pixels = stbi_load(filepath, &w, &h, &num_channels, desired_channels);
    assert(pixels != NULL);

    sbatch_premultiply_alpha_rgba8(pixels, w * h);

    const sg_image image = sg_make_image(&(sg_image_desc) {
        .label = label,
        .width = w,
        .height = h,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = filter,
        .mag_filter = filter,
        .wrap_u = wrap,
        .wrap_v = wrap,
        .data.subimage[0][0] = {
            .ptr = pixels,
            .size = (size_t)(w * h * 4)
        }
    });
    assert(image.id != SG_INVALID_ID);

    stbi_image_free(pixels);

    return image;
}

void make_resolution_dependent_resources() {
    if (state.crt_context.id != SG_INVALID_ID) {
        sbatch_destroy_context(state.crt_context);
    }

    state.screen_height = sapp_height();
    state.screen_width = sapp_width();
    const float aspect = (float)GAMEPLAY_WIDTH / (float)GAMEPLAY_HEIGHT;

    state.viewport_width = state.screen_width;
    state.viewport_height = (int)((float)state.viewport_width / aspect + 0.5f);

    if (state.viewport_height > state.screen_height) {
        state.viewport_height = state.screen_height;
        state.viewport_width = (int)((float)state.viewport_height * aspect + 0.5f);
    }

    state.crt_context = sbatch_make_context(&(sbatch_context_desc) {
        .pipeline = state.crt_pipeline,
        .canvas_height = state.screen_height,
        .canvas_width = state.screen_width,
        .max_sprites = 1
    });

    state.viewport_x = (state.screen_width / 2) - (state.viewport_width / 2);
    state.viewport_y = (state.screen_height / 2) - (state.viewport_height / 2);
}

scroll_layer init_layer(int x, int y, int width, int height, int y_offset) {
    const float ratio = ceilf((float)GAMEPLAY_WIDTH / (float)width);
    return (scroll_layer){
        .position = {
            .x = 0,
            .y = (float)(GAMEPLAY_HEIGHT - height) - (float)y_offset
        },
        .repeat_count = (int)ratio + 1,
        .source = {
            .x = (float)x,
            .y = (float)y,
            .width = (float)width,
            .height = (float)height
        }
    };
}

animation init_animation(int x, int y, int width, int height, int frame_count, int x_offset, int y_offset, uint32_t mask) {
    return (animation) {
        .position = {
            .x = (float)x_offset,
            .y = (float)(GAMEPLAY_HEIGHT - height) - (float)y_offset
        },
        .frame_count = frame_count,
        .color = sg_white,
        .source = {
            .x = (float)x,
            .y = (float)y,
            .width = (float)width / (float)frame_count,
            .height = (float)height
        },
        .mask = mask
    };
}

void init(void) {
    stm_setup();
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext()
    });
    __dbgui_setup(sapp_sample_count());

    state.pass_action = (sg_pass_action) {
        .colors[0] = {.action = SG_ACTION_CLEAR, .value = sg_black }
    };

    state.atlas = load_sprite("atlas.png", "spritebatch-sprite-atlas", SG_FILTER_NEAREST, SG_WRAP_REPEAT);
    state.mountains = init_layer(0, 270, 179, 192, 0);
    state.graveyard = init_layer(192, 326, 384, 123, 0);
    state.objects = init_layer(0, 449, 1024, 169, 32);
    state.tiles = init_layer(192, 270, 64, 41, 0);
    state.nightmare = init_animation(0, 741, 576, 96, 4, GAMEPLAY_WIDTH / 2 - 72, 32, SBATCH_FLIP_X);
    state.demon = init_animation(0, 618, 960, 123, 6, 0, 128, 0);

    sbatch_setup(&(sbatch_desc) { 0 });

    state.gameplay_render_target = sg_make_image(&(sg_image_desc) {
        .label = "spritebatch-gameplay-render-target",
        .render_target = true,
        .width = GAMEPLAY_WIDTH,
        .height = GAMEPLAY_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_BGRA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    state.gameplay_pipeline = sbatch_make_pipeline(&(sg_pipeline_desc) {
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .label = "spritebatch-gameplay-pipeline"
    });

    state.gameplay_context = sbatch_make_context(&(sbatch_context_desc) {
        .canvas_height = GAMEPLAY_HEIGHT,
        .canvas_width = GAMEPLAY_WIDTH,
        .pipeline = state.gameplay_pipeline,
        .label = "spritebatch-gameplay-context"
    });

    state.gameplay_pass = sg_make_pass(&(sg_pass_desc) {
        .color_attachments[0].image = state.gameplay_render_target,
        .label = "spritebatch-gameplay-pass"
    });

    state.crt_shader = sg_make_shader(spritebatch_crt_shader_desc(sg_query_backend()));

    state.crt_pipeline = sbatch_make_pipeline(&(sg_pipeline_desc) {
        .shader = state.crt_shader,
        .label = "spritebatch-crt-pipeline"
    });

    make_resolution_dependent_resources();
}

void draw_layer(float scroll_speed, scroll_layer* layer) {
    layer->position.x -= scroll_speed;
    if (layer->position.x <= -layer->source.width) {
        layer->position.x = 0;
    }
    for (int i = 0; i < layer->repeat_count; i++) {
        sbatch_push_sprite(&(sbatch_sprite) {
            .image = state.atlas,
            .position = {
                .x = floorf(layer->position.x + (float)i * layer->source.width),
                .y = floorf(layer->position.y)
            },
            .source = layer->source
        });
    }
}

void draw_animation(float delta_time, animation* ani) {
    ani->frame_time += delta_time;

    if(ani->frame_time >= 1.0f / 12.0f) {
        ani->frame_time = 0.0f;
        ani->frame_index = (ani->frame_index + 1) % ani->frame_count;
    }

    sbatch_push_sprite(&(sbatch_sprite) {
        .image = state.atlas,
        .position = {
            .x = floorf(ani->position.x),
            .y = floorf(ani->position.y)
        },
        .source = {
            .x = ani->source.x + ani->source.width * (float)ani->frame_index,
            .y = ani->source.y,
            .width = ani->source.width,
            .height = ani->source.height,
        },
        .flags = SBATCH_FLIP_X,
        .color = &ani->color
    });
}

void frame(void) {
    const float delta_time = (float)stm_sec(stm_laptime(&state.last_time));

    state.accumulator += delta_time;

    sg_begin_pass(state.gameplay_pass, &state.pass_action);
    {
        sbatch_begin(state.gameplay_context);
        {
            sbatch_push_sprite_rect(&(sbatch_sprite_rect) {
                .image = state.atlas,
                .destination = {
                    .width = GAMEPLAY_WIDTH,
                    .height = GAMEPLAY_HEIGHT
                },
                .source = {
                    .width = GAMEPLAY_WIDTH,
                    .height = GAMEPLAY_HEIGHT
                }
            });

            draw_layer(30.0f * delta_time, &state.mountains);
            draw_layer(60.0f * delta_time, &state.graveyard);
            draw_layer(120.0f * delta_time, &state.objects);
            draw_layer(240.0f * delta_time, &state.tiles);
            draw_animation(delta_time, &state.demon);
            draw_animation(delta_time, &state.nightmare);

            sbatch_push_sprite_rect(&(sbatch_sprite_rect) {
                .image = state.atlas,
                .destination = {
                    .x = GAMEPLAY_WIDTH - 152,
                    .y = 0,
                    .width = 152,
                    .height = 88
                },
                .source = {
                    .x = 480,
                    .y = 0,
                    .width = 152,
                    .height = 88
                }
            });

            state.demon.color = sg_color_multiply(&sg_white, 0.25f + (sinf(state.accumulator) / 2.0f) + 0.5f);
            state.demon.position.x += sinf(state.accumulator);
            state.demon.position.y += cosf(state.accumulator) / 2.0f;
        }
        sbatch_end();
    }
    sg_end_pass();

    sg_begin_default_pass(&state.pass_action, state.screen_width, state.screen_height);
    {
        sbatch_begin(state.crt_context);
        {
            sbatch_push_sprite_rect(&(sbatch_sprite_rect) {
                .image = state.gameplay_render_target,
                .destination = {
                    .x = (float)state.viewport_x,
                    .y = (float)state.viewport_y,
                    .width = (float)state.viewport_width,
                    .height = (float)state.viewport_height
                }
            });

            state.crt_params.OutputSize[0] = (float)state.viewport_width;
            state.crt_params.OutputSize[1] = (float)state.viewport_height;
            state.crt_params.OutputSize[2] = 1.0f / (float)state.viewport_width;
            state.crt_params.OutputSize[3] = 1.0f / (float)state.viewport_height;

            state.crt_params.SourceSize[0] = GAMEPLAY_WIDTH;
            state.crt_params.SourceSize[1] = GAMEPLAY_HEIGHT;
            state.crt_params.SourceSize[2] = 1.0f / GAMEPLAY_WIDTH;
            state.crt_params.SourceSize[3] = 1.0f / GAMEPLAY_HEIGHT;

            sbatch_apply_fs_uniforms(0, &SG_RANGE(state.crt_params));
        }
        sbatch_end();
        __dbgui_draw();
    }
    sg_end_pass();

    sg_commit();
    sbatch_frame();
}

void cleanup(void) {
    sg_destroy_shader(state.crt_shader);
    sbatch_destroy_pipeline(state.crt_pipeline);
    sbatch_destroy_context(state.crt_context);
    sg_destroy_pass(state.gameplay_pass);
    sbatch_destroy_context(state.gameplay_context);
    sg_destroy_image(state.gameplay_render_target);
    sg_destroy_image(state.atlas);
    __dbgui_shutdown();
    sbatch_shutdown();
    sg_shutdown();
}

void handle_event(const sapp_event* e) {
    if(e->type == SAPP_EVENTTYPE_RESIZED) {
        make_resolution_dependent_resources();
    }
    __dbgui_event(e);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = handle_event,
        .width = 1440,
        .height = 810,
        .gl_force_gles2 = true,
        .window_title = "SpriteBatch (sokol-app)",
        .icon.sokol_default = true,
    };
}