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
#include <float.h>

typedef struct tileset {
    sg_image image;
} tileset;

typedef struct camera {
    float width;
    float height;
    float x;
    float y;
    float zoom;
} camera;

static struct {
    tileset tilesets[LDtkTilesetsLength];
    uint8_t file_buffer[512 * 1024];
    sbatch_context context;
    sbatch_render_state render_state;
    sg_pass_action action;
    camera camera;
    size_t target_level;
    float factor;
} state;

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
        }
    }
}

void load_tileset(tileset* t, int index) {
    t->image = sg_alloc_image();

    const char* relative_path =
        LDtkTopDown.tilesets[index].relPath.data;

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

void update_camera() {
    if (state.factor <= 1.0f)
    {
        state.factor += 1.0f / 60.0f;

        const float offsetX = (float)LDtkTopDown.levels[state.target_level].worldX + ((float)LDtkTopDown.levels[state.target_level].pxWid) / 2.0f;
        const float offsetY = (float)LDtkTopDown.levels[state.target_level].worldY + ((float)LDtkTopDown.levels[state.target_level].pxHei) / 2.0f;

        state.camera.x = lerp(state.camera.x, offsetX, state.factor);
        state.camera.y = lerp(state.camera.y, offsetY, state.factor);

        state.camera.width = sapp_widthf();
        state.camera.height = sapp_heightf();

        const float scale = sapp_heightf() / (float)LDtkTopDown.levels[state.target_level].pxHei;
        state.camera.zoom = lerp(state.camera.zoom, scale, state.factor);

        sbatch_matrix matrix = sbatch_matrix_identity();
        sbatch_matrix_translate(&matrix, state.camera.width / 2, state.camera.height / 2, 0);
        sbatch_matrix_scale(&matrix, state.camera.zoom, state.camera.zoom, 1.0f);
        sbatch_matrix_translate(&matrix, -state.camera.x, -state.camera.y, 0);

        state.render_state.transform = matrix;
        state.render_state.use_pixel_snap = true;
        state.render_state.canvas_height = sapp_height();
        state.render_state.canvas_width = sapp_width();
    }
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

    for (int i = 0; i < LDtkTilesetsLength; i++) {
        load_tileset(state.tilesets + i, i);
    }

    sbatch_setup(&(sbatch_desc) { 0 });

    const sg_color bg_color =
        sg_make_color_1i(LDtkTopDown.bgColor);

    state.context = sbatch_make_context(&(sbatch_context_desc) {
        .label = "ldtk-context",
        .max_sprites_per_frame = 16384
    });

    state.action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = bg_color }
    };

    state.camera = (camera){
        .height = sapp_heightf(),
        .width = sapp_widthf()
    };

    state.factor = 1.0f - FLT_MIN;
    update_camera();
}

void draw_tile(sg_image image, float x, float y, float source_x, float source_y, float source_width, float source_height, float origin_x, float origin_y, uint32_t flags, const sg_color* color) {
    sbatch_push_sprite(&(sbatch_sprite) {
        .image = image,
        .origin = {
            .x = origin_x,
            .y = origin_y,
        },
        .position = {
            .x = x,
            .y = y
        },
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

void draw_layer_auto_layer(const LDtkLayerAutoLayer* layer, float levelX, float levelY) {
    const tileset* tileset = state.tilesets + layer->tilesetIndex;
    const sg_color color = sg_color_multiply(&sg_white, layer->opacity);
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
            &color);
    }
}

void draw_layer_tiles(const LDtkLayerTiles* layer, float levelX, float levelY) {
    const tileset* tileset = state.tilesets + layer->tilesetIndex;
    const sg_color color = sg_color_multiply(&sg_white, layer->opacity);
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
            &color);
    }
}

void draw_layer_int_grid(const LDtkLayerIntGrid* layer, float levelX, float levelY) {
    const tileset* tileset = state.tilesets + layer->tilesetIndex;
    const sg_color color = sg_color_multiply(&sg_white, layer->opacity);
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
            &color);
    }
}

void draw_layer_entities(const LDtkLayerEntities* layer, float levelX, float levelY) {
    for (size_t i = 0; i < layer->InteractivesLength; i++) {
        const LDtkEntityInteractive* interactive = layer->Interactives + i;
        if (interactive->hasTile) {
            const tileset* tileset = state.tilesets + interactive->tile.tilesetIndex;
            const sg_color color = sg_color_multiply(&sg_white, layer->opacity);
            const float xOffset = levelX + (float)layer->pxTotalOffsetX;
            const float yOffset = levelY + (float)layer->pxTotalOffsetY;
            draw_tile(tileset->image,
                xOffset + (float)interactive->px.x,
                yOffset + (float)interactive->px.y,
                (float)interactive->tile.srcRect.x,
                (float)interactive->tile.srcRect.y,
                (float)interactive->tile.srcRect.width,
                (float)interactive->tile.srcRect.height,
                interactive->pivot.x * (float)interactive->tile.srcRect.width,
                interactive->pivot.y * (float)interactive->tile.srcRect.height,
                SBATCH_FLIP_NONE,
                &color);
        }
    }
}

void frame(void) {
    update_camera();

    sfetch_dowork();

    sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
    sdtx_origin(3.0f, 3.0f);
    sdtx_color1i(SG_WHITE_RGBA32);
    for (int i = 0; i < 1; i++) {
        sdtx_font(i);
        sdtx_printf("level:\t %s\n", LDtkTopDown.levels[state.target_level].identifier.data);
        sdtx_move_y(0.5f);
        sdtx_printf("worldX:\t %d\n", LDtkTopDown.levels[state.target_level].worldX);
        sdtx_move_y(0.5f);
        sdtx_printf("worldY:\t %d\n", LDtkTopDown.levels[state.target_level].worldY);
        sdtx_move_y(0.5f);
        sdtx_printf("pxWid:\t %d\n", LDtkTopDown.levels[state.target_level].pxWid);
        sdtx_move_y(0.5f);
        sdtx_printf("pxHei:\t %d\n", LDtkTopDown.levels[state.target_level].pxHei);
    }

    sg_begin_default_pass(&state.action, sapp_width(), sapp_height());
    {
        sbatch_begin(state.context, &state.render_state);
        {
            for (size_t i = 0; i < LDtkTopDown.levelsLength; i++) {
                const LDtkLevel* level = LDtkTopDown.levels + i;
                draw_layer_auto_layer(&level->layers.Ground, (float)level->worldX, (float)level->worldY);
                draw_layer_tiles(&level->layers.Custom_grounds, (float)level->worldX, (float)level->worldY);
                draw_layer_int_grid(&level->layers.Collisions, (float)level->worldX, (float)level->worldY);
                draw_layer_tiles(&level->layers.Custom_tiles, (float)level->worldX, (float)level->worldY);
                draw_layer_auto_layer(&level->layers.Wall_overlays, (float)level->worldX, (float)level->worldY);
                draw_layer_entities(&level->layers.Entities, (float)level->worldX, (float)level->worldY);
            }
        }
        sbatch_end();

        sdtx_draw();
        __dbgui_draw();
    }
    sg_end_pass();

    sg_commit();
    sbatch_frame();
}

void cleanup(void) {
    sbatch_destroy_context(state.context);
    sbatch_shutdown();
    sdtx_shutdown();
    __dbgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

void handle_event(const sapp_event* e) {
    switch(e->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
        {
            if(e->key_code == SAPP_KEYCODE_LEFT)
            {
                state.target_level = (state.target_level + 1) % LDtkTopDown.levelsLength;
                state.factor = 0.0f;
            }
            else if (e->key_code == SAPP_KEYCODE_RIGHT)
            {
                state.target_level = (state.target_level - 1 + LDtkTopDown.levelsLength) % LDtkTopDown.levelsLength;
                state.factor = 0.0f;
            }
            break;
        }
        case SAPP_EVENTTYPE_RESIZED:
        {
            state.factor = 0.0f;
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
