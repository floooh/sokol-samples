//------------------------------------------------------------------------------
//  spritebatch-ldtk-sapp.c
//------------------------------------------------------------------------------

#define LDTK_IMPL
#define SOKOL_SPRITEBATCH_IMPL
#define SOKOL_DEBUGTEXT_IMPL
#define SOKOL_COLOR_IMPL

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_spritebatch.h"
#include "sokol_color.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "sokol_debugtext.h"
#include "dbgui/dbgui.h"
#include "stb/stb_image.h"
#include "TopDown.h"

typedef struct {
    sg_image image;
    void(*on_loaded)(void);
} tileset;

typedef enum {
    camera_state_default,
    camera_state_move_to_selection,
    camera_state_zoom_to_fit_selection,
    camera_state_zoom_reset
} camera_state;

typedef struct {
    float width;
    float height;
    float x;
    float y;
    float zoom;
    camera_state state;
} camera;

bool value_in_range(float value, float min, float max) {
    return (value >= min) && (value <= max);
}

bool is_visible(const camera* camera, const LDtkLevel* level) {
    const float level_x = (float)level->worldX;
    const float level_y = (float)level->worldY;
    const float level_width = (float)level->pxWid;
    const float level_height = (float)level->pxHei;

    const float camera_x = camera->x - ((camera->width / 2) / camera->zoom);
    const float camera_y = camera->y - ((camera->height / 2) / camera->zoom);
    const float camera_height = camera->height / camera->zoom;
    const float camera_width = camera->width / camera->zoom;

    const bool x_overlap = value_in_range(level_x, camera_x, camera_x + camera_width) ||
        value_in_range(camera_x, level_x, level_x + level_width);

    const bool y_overlap = value_in_range(level_y, camera_y, camera_y + camera_height) ||
        value_in_range(camera_y, level_y, level_y + level_height);

    return x_overlap && y_overlap;
}

static struct {
    tileset tilesets[LDtkTilesetsLength];
    sg_image level_previews[LDtkLevelsLength];
    uint8_t file_buffer[512 * 1024];
    sbatch_context context;
    sbatch_render_state render_state;
    sg_pass_action action;
    camera camera;
    size_t selected_level;
    size_t target_level;
    sbatch_float2 screen_mouse;
    sbatch_float2 world_mouse;
    float camera_update_time;
} state;

bool is_mouse_over(const LDtkLevel* level) {
    const float level_x = (float)level->worldX;
    const float level_y = (float)level->worldY;
    const float level_width = (float)level->pxWid;
    const float level_height = (float)level->pxHei;
    return state.world_mouse.x >= level_x &&
        state.world_mouse.x <= level_x + level_width &&
        state.world_mouse.y >= level_y &&
        state.world_mouse.y <= level_y + level_height;
}

static void fetch_callback(const sfetch_response_t* response) {
    if (response->fetched) {

        int w, h, num_channels;
        const int desired_channels = 4;

        stbi_uc* pixels = stbi_load_from_memory(
            response->buffer_ptr,
            (int)response->fetched_size,
            &w, &h,
            &num_channels, desired_channels);

        if (pixels) {

            sbatch_premultiply_alpha_rgba8(pixels, w * h);

            tileset* t = (tileset*)response->user_data;

            sg_init_image(t->image, &(sg_image_desc) {
                .label = response->path,
                .width = w,
                .height = h,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .min_filter = SG_FILTER_NEAREST,
                .mag_filter = SG_FILTER_NEAREST,
                .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
                .data.subimage[0][0] = {
                    .ptr = pixels,
                    .size = (size_t)(w * h * 4)
                }
            });

            stbi_image_free(pixels);

            if(t->on_loaded) {
                t->on_loaded();
            }
        }
    }
}

void load_tileset(tileset* t, int index) {
    t->image = sg_alloc_image();

    const char* relative_path =
        LDtkGridVania.tilesets[index].relPath.data;

    sfetch_send(&(sfetch_request_t) {
        .path = relative_path,
        .callback = fetch_callback,
        .buffer_ptr = state.file_buffer,
        .user_data_ptr = t,
        .user_data_size = sizeof(tileset),
        .buffer_size = sizeof(state.file_buffer)
    });
}

float lerp(float a, float b, float amount) {
    return a + (b - a) * amount;
}

sbatch_matrix update_camera(void) {
    if (state.camera_update_time <= 1.0f) {

        state.camera_update_time += 1.0f / 60.0f;

        state.camera.width = sapp_widthf();
        state.camera.height = sapp_heightf();

        const float offsetX = (float)LDtkGridVania.levels[state.selected_level].worldX + ((float)LDtkGridVania.levels[state.selected_level].pxWid) / 2.0f;
        const float offsetY = (float)LDtkGridVania.levels[state.selected_level].worldY + ((float)LDtkGridVania.levels[state.selected_level].pxHei) / 2.0f;
        const float scaleY = sapp_heightf() / ((float)LDtkGridVania.levels[state.selected_level].pxHei + 32.0f);
        const float scaleX = sapp_widthf() / ((float)LDtkGridVania.levels[state.selected_level].pxWid + 32.0f);

        switch(state.camera.state) {
            case camera_state_move_to_selection: {
                state.camera.x = lerp(state.camera.x, offsetX, state.camera_update_time);
                state.camera.y = lerp(state.camera.y, offsetY, state.camera_update_time);
                if (scaleY < state.camera.zoom || scaleX < state.camera.zoom) {
                    state.camera.zoom = lerp(state.camera.zoom, scaleY < scaleX ? scaleY : scaleX, state.camera_update_time);
                }
                break;
            }
            case camera_state_zoom_reset: {
                state.camera.zoom = lerp(state.camera.zoom, 1.0f, state.camera_update_time);
                break;
            }
            case camera_state_zoom_to_fit_selection: {
                state.camera.x = lerp(state.camera.x, offsetX, state.camera_update_time);
                state.camera.y = lerp(state.camera.y, offsetY, state.camera_update_time);
                state.camera.zoom = lerp(state.camera.zoom, scaleY < scaleX ? scaleY : scaleX, state.camera_update_time);
                break;
            }
            case camera_state_default: {
                break;
            }
        }
    } else {
        state.camera.state = camera_state_default;
    }

    state.camera.height = sapp_heightf();
    state.camera.width = sapp_widthf();

    sbatch_matrix camera_view = sbatch_matrix_identity();
    sbatch_matrix_translate(&camera_view, state.camera.width / 2.0f, state.camera.height / 2.0f, 0.0f);
    sbatch_matrix_scale(&camera_view, state.camera.zoom, state.camera.zoom, 1.0f);
    sbatch_matrix_translate(&camera_view, -state.camera.x, -state.camera.y, 0.0f);
    return camera_view;
}

void draw_tile(sg_image image, float x, float y, float source_x, float source_y, float source_width, float source_height, float origin_x, float origin_y, uint32_t flags, const sg_color* color) {
    sbatch_push_sprite(&(sbatch_sprite) {
        .image = image,
        .origin = { .x = origin_x, .y = origin_y },
        .position = { .x = x, .y = y },
        .flags = flags,
        .source = {
            .width = source_width,
            .height = source_height,
            .x = source_x,
            .y = source_y
        },
        .color = color
    });
}

void draw_layer_auto_layer(const LDtkLayerAutoLayer* layer, const sg_color* color, float levelX, float levelY) {
    const tileset* tileset = state.tilesets + layer->tilesetIndex;
    const sg_color layer_color = sg_color_multiply(color, layer->opacity);
    const float xOffset = levelX + (float)layer->pxTotalOffsetX;
    const float yOffset = levelY + (float)layer->pxTotalOffsetY;
    const float gridSize = (float)layer->gridSize;
    for (size_t i = 0; i < layer->autoLayerTilesLength; i++) {
        const LDtkTile* tile = layer->autoLayerTiles + i;
        draw_tile(tileset->image,
            xOffset + (float)tile->px.x,
            yOffset + (float)tile->px.y,
            (float)tile->src.x,
            (float)tile->src.y,
            gridSize,
            gridSize,
            0.0f,
            0.0f,
            tile->f,
            &layer_color);
    }
}

void draw_layer_tiles(const LDtkLayerTiles* layer, const sg_color* color, float levelX, float levelY) {
    const tileset* tileset = state.tilesets + layer->tilesetIndex;
    const sg_color layer_color = sg_color_multiply(color, layer->opacity);
    const float xOffset = levelX + (float)layer->pxTotalOffsetX;
    const float yOffset = levelY + (float)layer->pxTotalOffsetY;
    const float gridSize = (float)layer->gridSize;
    for (size_t i = 0; i < layer->gridTilesLength; i++) {
        const LDtkTile* tile = layer->gridTiles + i;
        draw_tile(tileset->image,
            xOffset + (float)tile->px.x,
            yOffset + (float)tile->px.y,
            (float)tile->src.x,
            (float)tile->src.y,
            gridSize,
            gridSize,
            0.0f,
            0.0f,
            tile->f,
            &layer_color);
    }
}

void draw_layer_int_grid(const LDtkLayerIntGrid* layer, const sg_color* color, float levelX, float levelY) {
    const tileset* tileset = state.tilesets + layer->tilesetIndex;
    const sg_color layer_color = sg_color_multiply(color, layer->opacity);
    const float xOffset = levelX + (float)layer->pxTotalOffsetX;
    const float yOffset = levelY + (float)layer->pxTotalOffsetY;
    const float gridSize = (float)layer->gridSize;
    for (size_t i = 0; i < layer->autoLayerTilesLength; i++) {
        const LDtkTile* tile = layer->autoLayerTiles + i;
        draw_tile(tileset->image,
            xOffset + (float)tile->px.x,
            yOffset + (float)tile->px.y,
            (float)tile->src.x,
            (float)tile->src.y,
            gridSize,
            gridSize,
            0.0f,
            0.0f,
            tile->f,
            &layer_color);
    }
}

void draw_layer_entities(const LDtkLayerEntities* layer, const sg_color* color, float levelX, float levelY) {
    for (size_t i = 0; i < layer->ItemsLength; i++) {
        const LDtkEntityItem* item = layer->Items + i;
        if (item->hasTile) {
            const tileset* tileset = state.tilesets + item->tile.tilesetIndex;
            const sg_color layer_color = sg_color_multiply(color, layer->opacity);
            const float xOffset = levelX + (float)layer->pxTotalOffsetX;
            const float yOffset = levelY + (float)layer->pxTotalOffsetY;
            draw_tile(tileset->image,
                xOffset + (float)item->px.x,
                yOffset + (float)item->px.y,
                (float)item->tile.srcRect.x,
                (float)item->tile.srcRect.y,
                (float)item->tile.srcRect.width,
                (float)item->tile.srcRect.height,
                item->pivot.x * (float)item->tile.srcRect.width,
                item->pivot.y * (float)item->tile.srcRect.height,
                SBATCH_FLIP_NONE,
                &layer_color);
        }
    }
}

void render_previews(void) {
    const sbatch_pipeline pipeline = sbatch_make_pipeline(&(sg_pipeline_desc) {
        .depth.pixel_format = SG_PIXELFORMAT_NONE
    });

    for (size_t i = 0; i < LDtkGridVania.levelsLength; i++) {
        const LDtkLevel* level = LDtkGridVania.levels + i;
        const sbatch_context context = sbatch_make_context(&(sbatch_context_desc){ 0 });

        sg_init_image(state.level_previews[i], &(sg_image_desc) {
            .label = level->identifier.data,
            .render_target = true,
            .width = level->pxWid / 16,
            .height = level->pxHei / 16,
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE
        });

        const sg_pass pass = sg_make_pass(&(sg_pass_desc) {
            .color_attachments[0].image = state.level_previews[i]
        });

        sbatch_matrix transform = sbatch_matrix_identity();
        const float scale = 1.0f / 16.0f;
        sbatch_matrix_scale(&transform, scale, scale, scale);
        sbatch_matrix_translate(&transform, -(float)level->worldX, -(float)level->worldY, 0);

        sg_begin_pass(pass, &state.action);
        {
            sbatch_begin(context, &(sbatch_render_state) {
                .use_pixel_snap = true,
                .pipeline = pipeline,
                .canvas_width = level->pxWid / 16,
                .canvas_height = level->pxHei / 16,
                .transform = transform
            });
            {
                draw_layer_int_grid(&level->layers.Collisions, &sg_white, (float)level->worldX, (float)level->worldY);
                draw_layer_auto_layer(&level->layers.Shadows, &sg_white, (float)level->worldX, (float)level->worldY);
                draw_layer_entities(&level->layers.Entities, &sg_white, (float)level->worldX, (float)level->worldY);
            }
            sbatch_end();
        }
        sg_end_pass();

        sg_destroy_pass(pass);
        sbatch_destroy_context(context);
    }
    sbatch_destroy_pipeline(pipeline);
}

void init(void) {

    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext()
    });

    sdtx_setup(&(sdtx_desc_t) {
        .fonts = {
            [0] = sdtx_font_kc854(),
        }
    });

    sfetch_setup(&(sfetch_desc_t) { 0 });

    __dbgui_setup(sapp_sample_count());

    for (int i = 0; i < LDtkLevelsLength; i++) {
        state.level_previews[i] = sg_alloc_image();
    }

    state.tilesets[0].on_loaded = render_previews;

    for (int i = 0; i < LDtkTilesetsLength; i++) {
        load_tileset(state.tilesets + i, i);
    }

    sbatch_setup(&(sbatch_desc) { 0 });

    const sg_color bg_color =
        sg_make_color_1i(LDtkGridVania.bgColor);

    state.context = sbatch_make_context(&(sbatch_context_desc) {
        .label = "ldtk-context",
        .max_sprites_per_frame = 16384 * 2
    });

    state.action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = bg_color }
    };

    state.camera = (camera) {
        .state = camera_state_move_to_selection,
        .height = sapp_heightf(),
        .width = sapp_widthf(),
        .zoom = 1.0f
    };

    state.camera_update_time = 1.0f;
    update_camera();
}

void render_world(void) {
    const sbatch_matrix camera_view = update_camera();

    sdtx_canvas(sapp_widthf(), sapp_heightf());
    sdtx_color1i(SG_WHITE_RGBA32);
    sg_begin_default_pass(&state.action, sapp_width(), sapp_height());
    {
        sbatch_begin(state.context, &(sbatch_render_state){
            .transform = camera_view,
            .use_pixel_snap = true,
            .canvas_height = (int)sapp_height(),
            .canvas_width = (int)sapp_width()
        });
        {
            state.target_level = SIZE_MAX;
            for (size_t i = 0; i < LDtkGridVania.levelsLength; i++) {
                const LDtkLevel* level = LDtkGridVania.levels + i;

                state.world_mouse = sbatch_canvas_to_world(state.screen_mouse);

                const bool is_selected_level = i == state.selected_level;
                if (is_mouse_over(level)) {
                    state.target_level = i;
                }

                if (is_visible(&state.camera, level)) {

                    const sbatch_float2 screen_space = sbatch_world_to_canvas((sbatch_float2) {
                        .x = (float)level->worldX + 16.0f,
                        .y = (float)level->worldY + 16.0f
                    });

                    sdtx_pos(screen_space.x / 8.0f, screen_space.y / 8.0f);
                    sdtx_printf(level->identifier.data);

                    if (is_selected_level || i == state.target_level) {
                        draw_layer_int_grid(&level->layers.Collisions, &sg_white, (float)level->worldX, (float)level->worldY);
                        draw_layer_auto_layer(&level->layers.Shadows, &sg_white, (float)level->worldX, (float)level->worldY);
                        draw_layer_entities(&level->layers.Entities, &sg_white, (float)level->worldX, (float)level->worldY);
                    }
                    else {
                        const sg_color color = sg_color_multiply(&sg_white, 0.5f);
                        if (state.level_previews[i].id != 0) {
                            sbatch_push_sprite_rect(&(sbatch_sprite_rect) {
                                .image = state.level_previews[i],
                                .destination = {
                                    .width = (float)level->pxWid,
                                    .height = (float)level->pxHei,
                                    .x = (float)level->worldX,
                                    .y = (float)level->worldY
                                },
                                .color = &color
                            });
                        }
                    }
                }
            }
        }
        sbatch_end();
        sdtx_draw();
        __dbgui_draw();
    }
    sg_end_pass();
}

void frame(void) {
    sfetch_dowork();
    render_world();
    sg_commit();
    sbatch_frame();
}

void cleanup(void) {
    for (size_t i = 0; i < LDtkLevelsLength; i++) {
        sg_destroy_image(state.level_previews[i]);
    }
    sbatch_destroy_context(state.context);
    sbatch_shutdown();
    sdtx_shutdown();
    __dbgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

void handle_key_press(size_t index) {
    const size_t level_index = index - 1;
    if (LDtkGridVania.levels[state.selected_level].neighboursLength >= index) {
        state.selected_level = LDtkGridVania.levels[state.selected_level].neighbours[level_index].levelIndex;
        state.camera_update_time = 0.0f;
    }
}

void handle_event(const sapp_event* e) {
    switch(e->type) {
        case SAPP_EVENTTYPE_MOUSE_SCROLL: {
            state.camera.state = camera_state_default;
            state.camera.zoom += (1.0f / 60.0f) * e->scroll_y;
            if (state.camera.zoom < 0.1f) state.camera.zoom = 0.1f;
            if (state.camera.zoom > 4.0f) state.camera.zoom = 4.0f;
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_MOVE: {
            state.screen_mouse.x = e->mouse_x;
            state.screen_mouse.y = e->mouse_y;
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_DOWN: {
            if(state.target_level != SIZE_MAX) {
                state.camera_update_time = 0.0f;
                state.camera.state = camera_state_move_to_selection;
                state.selected_level = state.target_level;
            }
            break;
        }
        case SAPP_EVENTTYPE_KEY_DOWN: {
            switch (e->key_code) {
            case SAPP_KEYCODE_LEFT_CONTROL: {
                state.camera_update_time = 0.0f;
                state.camera.state = camera_state_zoom_reset;
                break;
            }
            case SAPP_KEYCODE_SPACE: {
                state.camera_update_time = 0.0f;
                state.camera.state = camera_state_zoom_to_fit_selection;
                break;
            }
            case SAPP_KEYCODE_LEFT: {
                if (state.camera.state == camera_state_default) {
                    state.camera.x -= 16.0f;
                }
                break;
            }
            case SAPP_KEYCODE_RIGHT: {
                if (state.camera.state == camera_state_default) {
                    state.camera.x += 16.0f;
                }
                break;
            }
            case SAPP_KEYCODE_UP: {
                if (state.camera.state == camera_state_default) {
                    state.camera.y -= 16.0f;
                }
                break;
            }
            case SAPP_KEYCODE_DOWN: {
                if (state.camera.state == camera_state_default) {
                    state.camera.y += 16.0f;
                }
                break;
            }
            default:
                break;
            }
            break;
        }
        case SAPP_EVENTTYPE_RESIZED: {
            state.camera_update_time = 0.0f;
            break;
        }
    default:
        break;
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
        .sample_count = 1,
        .width = 1280,
        .height = 720,
        .gl_force_gles2 = true,
        .window_title = "SpriteBatch (sokol-app)",
        .icon.sokol_default = true,
    };
}
