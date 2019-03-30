//------------------------------------------------------------------------------
//  sgl-microui.c
//
//  https://github.com/rxi/microui sample using sokol_gl.h, sokol_gfx.h
//  and sokol_app.h
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "microui/microui.h"
#include "microui/atlas.inl"
#define SOKOL_GL_IMPL
#include "sokol_gl.h"
#include <stdio.h> /* sprintf */

static mu_Context mu_ctx;

static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[3] = { 90.0f, 95.0f, 100.0f };

/* UI functions */
static void test_window(mu_Context* ctx);
static void log_window(mu_Context* ctx);
static void style_window(mu_Context* ctx);

/* microui renderer functions (implementation is at the end of this file) */
static void r_init(void);
static void r_begin(int disp_width, int disp_height);
static void r_end(void);
static void r_draw(void);
static void r_push_quad(mu_Rect dst, mu_Rect src, mu_Color color);
static void r_draw_rect(mu_Rect rect, mu_Color color);
static void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color);
static void r_draw_icon(int id, mu_Rect rect, mu_Color color);
static int r_get_text_width(const char* text, int len);
static int r_get_text_height(void);
static void r_set_clip_rect(mu_Rect rect);

/* callbacks */
static int text_width_cb(mu_Font font, const char* text, int len) {
    if (len == -1) {
        len = strlen(text);
    }
    return r_get_text_width(text, len);
}

static int text_height_cb(mu_Font font) {
    return r_get_text_height();
}

static void write_log(const char* text) {
    /* FIXME: THIS IS UNSAFE! */
    if (logbuf[0]) {
        strcat(logbuf, "\n");
    }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

/* initialization */
static void init() {
    /* setup sokol-gfx */
    sg_setup(&(sg_desc){
        .gl_force_gles2 = sapp_gles2(),
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view,
    });

    /* setup sokol-gl */
    sgl_setup(&(sgl_desc_t){0});

    /* setup microui renderer */
    r_init();

    /* setup microui */
    mu_init(&mu_ctx);
    mu_ctx.text_width = text_width_cb;
    mu_ctx.text_height = text_height_cb;
}

static const char key_map[512] = {
    [SAPP_KEYCODE_LEFT_SHIFT]       = MU_KEY_SHIFT,
    [SAPP_KEYCODE_RIGHT_SHIFT]      = MU_KEY_SHIFT,
    [SAPP_KEYCODE_LEFT_CONTROL]     = MU_KEY_CTRL,
    [SAPP_KEYCODE_RIGHT_CONTROL]    = MU_KEY_CTRL,
    [SAPP_KEYCODE_LEFT_ALT]         = MU_KEY_ALT,
    [SAPP_KEYCODE_RIGHT_ALT]        = MU_KEY_ALT,
    [SAPP_KEYCODE_ENTER]            = MU_KEY_RETURN,
    [SAPP_KEYCODE_BACKSPACE]        = MU_KEY_BACKSPACE,
};

static void event(const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            mu_input_mousedown(&mu_ctx, ev->mouse_x, ev->mouse_y, (1<<ev->mouse_button));
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            mu_input_mouseup(&mu_ctx, ev->mouse_x, ev->mouse_y, (1<<ev->mouse_button));
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            mu_input_mousemove(&mu_ctx, ev->mouse_x, ev->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            mu_input_mousewheel(&mu_ctx, ev->scroll_y);
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            mu_input_keydown(&mu_ctx, key_map[ev->key_code & 511]);
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            mu_input_keyup(&mu_ctx, key_map[ev->key_code & 511]);
            break;
        case SAPP_EVENTTYPE_CHAR:
            {
                char txt[2] = { ev->char_code & 255, 0 };
                mu_input_text(&mu_ctx, txt);
            }
            break;
        default:
            break;
    }
}

/* do one frame */
void frame(void) {

    /* UI definition */
    mu_begin(&mu_ctx);
    test_window(&mu_ctx);
    log_window(&mu_ctx);
    style_window(&mu_ctx);
    mu_end(&mu_ctx);

    /* micro-ui rendering */
    r_begin(sapp_width(), sapp_height());
    mu_Command* cmd = 0;
    while(mu_next_command(&mu_ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
        }
    }
    r_end();
    
    /* render the sokol-gfx default pass */
    sg_begin_default_pass(&(sg_pass_action){
            .colors[0] = {
                .action = SG_ACTION_CLEAR,
                .val = { bg[0] / 255.0f, bg[1] / 255.0f, bg[2] / 255.0f, 1.0f }
            }
        }, sapp_width(), sapp_height());
    r_draw();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    sgl_shutdown();
    sg_shutdown();
}

static void test_window(mu_Context* ctx) {
    static mu_Container window;

    /* init window manually so we can set its position and size */
    if (!window.inited) {
        mu_init_window(ctx, &window, 0);
        window.rect = mu_rect(40, 40, 300, 450);
    }

    /* limit window to minimum size */
    window.rect.w = mu_max(window.rect.w, 240);
    window.rect.h = mu_max(window.rect.h, 300);

    /* do window */
    if (mu_begin_window(ctx, &window, "Demo Window")) {

        /* window info */
        static int show_info = 0;
        if (mu_header(ctx, &show_info, "Window Info")) {
            char buf[64];
            mu_layout_row(ctx, 2, (int[]) { 54, -1 }, 0);
            mu_label(ctx,"Position:");
            sprintf(buf, "%d, %d", window.rect.x, window.rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", window.rect.w, window.rect.h); mu_label(ctx, buf);
        }

        /* labels + buttons */
        static int show_buttons = 1;
        if (mu_header(ctx, &show_buttons, "Test Buttons")) {
            mu_layout_row(ctx, 3, (int[]) { 86, -110, -1 }, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
            if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
            mu_label(ctx, "Test buttons 2:");
            if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
            if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
        }

        /* tree */
        static int show_tree = 1;
        if (mu_header(ctx, &show_tree, "Tree and Text")) {
            mu_layout_row(ctx, 2, (int[]) { 140, -1 }, 0);
            mu_layout_begin_column(ctx);
            static int states[8];
            if (mu_begin_treenode(ctx, &states[0], "Test 1")) {
                if (mu_begin_treenode(ctx, &states[1], "Test 1a")) {
                mu_label(ctx, "Hello");
                mu_label(ctx, "world");
                mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, &states[2], "Test 1b")) {
                if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
                if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
                mu_end_treenode(ctx);
                }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, &states[3], "Test 2")) {
                mu_layout_row(ctx, 2, (int[]) { 54, 54 }, 0);
                if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
                if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
                if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
                if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, &states[4], "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(ctx, &checks[0], "Checkbox 1");
                mu_checkbox(ctx, &checks[1], "Checkbox 2");
                mu_checkbox(ctx, &checks[2], "Checkbox 3");
                mu_end_treenode(ctx);
            }
            mu_layout_end_column(ctx);

            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
            mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(ctx);
        }

        /* background color sliders */
        static int show_sliders = 1;
        if (mu_header(ctx, &show_sliders, "Background Color")) {
            mu_layout_row(ctx, 2, (int[]) { -78, -1 }, 74);
            /* sliders */
            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 2, (int[]) { 46, -1 }, 0);
            mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* color preview */
            mu_Rect r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            char buf[32];
            sprintf(buf, "#%02X%02X%02X", (int) bg[0], (int) bg[1], (int) bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}

static void log_window(mu_Context *ctx) {
  static mu_Container window;

    /* init window manually so we can set its position and size */
    if (!window.inited) {
        mu_init_window(ctx, &window, 0);
        window.rect = mu_rect(350, 40, 300, 200);
    }

    if (mu_begin_window(ctx, &window, "Log Window")) {

        /* output text panel */
        static mu_Container panel;
        mu_layout_row(ctx, 1, (int[]) { -1 }, -28);
        mu_begin_panel(ctx, &panel);
        mu_layout_row(ctx, 1, (int[]) { -1 }, -1);
        mu_text(ctx, logbuf);
        mu_end_panel(ctx);
        if (logbuf_updated) {
            panel.scroll.y = panel.content_size.y;
            logbuf_updated = 0;
        }

        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        mu_layout_row(ctx, 2, (int[]) { -70, -1 }, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }
        mu_end_window(ctx);
    }
}

static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
    static float tmp;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
}

static void style_window(mu_Context *ctx) {
    static mu_Container window;

    /* init window manually so we can set its position and size */
    if (!window.inited) {
        mu_init_window(ctx, &window, 0);
        window.rect = mu_rect(350, 250, 300, 240);
    }

    static struct { const char *label; int idx; } colors[] = {
        { "text:",         MU_COLOR_TEXT        },
        { "border:",       MU_COLOR_BORDER      },
        { "windowbg:",     MU_COLOR_WINDOWBG    },
        { "titlebg:",      MU_COLOR_TITLEBG     },
        { "titletext:",    MU_COLOR_TITLETEXT   },
        { "panelbg:",      MU_COLOR_PANELBG     },
        { "button:",       MU_COLOR_BUTTON      },
        { "buttonhover:",  MU_COLOR_BUTTONHOVER },
        { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
        { "base:",         MU_COLOR_BASE        },
        { "basehover:",    MU_COLOR_BASEHOVER   },
        { "basefocus:",    MU_COLOR_BASEFOCUS   },
        { "scrollbase:",   MU_COLOR_SCROLLBASE  },
        { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
        { NULL }
    };

    if (mu_begin_window(ctx, &window, "Style Editor")) {
        int sw = mu_get_container(ctx)->body.w * 0.14;
        mu_layout_row(ctx, 6, (int[]) { 80, sw, sw, sw, sw, -1 }, 0);
        for (int i = 0; colors[i].label; i++) {
          mu_label(ctx, colors[i].label);
          uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
          uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
          uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
          uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
          mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
        }
        mu_end_window(ctx);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 720,
        .height = 540,
        .gl_force_gles2 = true,
        .window_title = "microui+sokol_gl.h"
    };
}

/*== micrui renderer =========================================================*/
static sg_image atlas_img;

static void r_init(void) {

    /* atlas image data is in atlas.inl file, this only contains alpha 
       values, need to expand this to RGBA8
    */
    uint32_t rgba8_size = ATLAS_WIDTH * ATLAS_HEIGHT * 4;
    uint32_t* rgba8_pixels = (uint32_t*) malloc(rgba8_size);
    for (int y = 0; y < ATLAS_HEIGHT; y++) {
        for (int x = 0; x < ATLAS_WIDTH; x++) {
            uint32_t index = y*ATLAS_WIDTH + x;
            rgba8_pixels[index] = 0x00FFFFFF | (atlas_texture[index]<<24);
        }
    }
    atlas_img = sg_make_image(&(sg_image_desc){
        .width = ATLAS_WIDTH,
        .height = ATLAS_HEIGHT,
        /* LINEAR would be better for text quality in HighDPI, but the
           atlas texture is "leaking" from neighbouring pixels unfortunately
        */
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .content = {
            .subimage[0][0] = {
                .ptr = rgba8_pixels,
                .size = rgba8_size
            }
        }
    });
    free(rgba8_pixels);
}

static void r_begin(int disp_width, int disp_height) {
    sgl_defaults();
    sgl_state_blend(true);
    sgl_state_cull_face(false);
    sgl_state_texture(true);
    sgl_texture(atlas_img);
    sgl_ortho(0.0f, (float) disp_width, (float) disp_height, 0.0f, -1.0f, +1.0f);
    sgl_begin_quads();
}

static void r_end(void) {
    sgl_end();
}

static void r_draw(void) {
    sgl_draw();
}

static void r_push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    const float u0 = src.x / (float) ATLAS_WIDTH;
    const float v0 = src.y / (float) ATLAS_HEIGHT;
    const float u1 = (src.x + src.w) / (float) ATLAS_WIDTH;
    const float v1 = (src.y + src.h) / (float) ATLAS_HEIGHT;

    const float x0 = dst.x;
    const float y0 = dst.y;
    const float x1 = dst.x + dst.w;
    const float y1 = dst.y + dst.h;

    sgl_c4b(color.r, color.g, color.b, color.a);
    sgl_v2f_t2f(x0, y0, u0, v0);
    sgl_v2f_t2f(x1, y0, u1, v0);
    sgl_v2f_t2f(x1, y1, u1, v1);
    sgl_v2f_t2f(x0, y1, u0, v1);
}

static void r_draw_rect(mu_Rect rect, mu_Color color) {
    r_push_quad(rect, atlas[ATLAS_WHITE], color);
}

static void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color) {
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (const char* p = text; *p; p++) {
        mu_Rect src = atlas[ATLAS_FONT + (unsigned char)*p];
        dst.w = src.w;
        dst.h = src.h;
        r_push_quad(dst, src, color);
        dst.x += dst.w;
    }
}

static void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    r_push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

static int r_get_text_width(const char* text, int len) {
    int res = 0;
    for (const char* p = text; *p && len--; p++) {
        res += atlas[ATLAS_FONT + (unsigned char)*p].w;
    }
    return res;
}

static int r_get_text_height(void) {
    return 18;
}

static void r_set_clip_rect(mu_Rect rect) {
    sgl_end();
    sgl_scissor_rect(rect.x, rect.y, rect.w, rect.h, true);
    sgl_begin_quads();
}
