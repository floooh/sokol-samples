//------------------------------------------------------------------------------
//  clear-emsc.cc
//------------------------------------------------------------------------------
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#define SOKOL_IMPL
#define SOKOL_USE_GLES2
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;

sg_pass_action pass_action;

void draw();

void main() {
    // setup WebGL context
    emscripten_set_canvas_size(WIDTH, HEIGHT);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    ctx = emscripten_webgl_create_context(0, &attrs);
    emscripten_webgl_make_context_current(ctx);
    
    // setup sokol_gfx
    sg_setup_desc setup_desc;
    sg_init_setup_desc(&setup_desc);
    setup_desc.width = WIDTH;
    setup_desc.height = HEIGHT;
    sg_setup(&setup_desc);
    assert(sg_valid());

    // setup pass action to clear to red
    sg_init_pass_action(&pass_action);
    pass_action.color[0][0] = 1.0f;
    pass_action.color[0][1] = 0.0f;
    pass_action.color[0][2] = 0.0f;
    pass_action.actions = SG_PASSACTION_CLEAR_ALL;

    emscripten_set_main_loop(draw, 0, 1);
}

void draw() {
    // animate clear colors
    float g = pass_action.color[0][1] + 0.01;
    if (g > 1.0f) g = 0.0f;
    pass_action.color[0][1] = g;

    // draw one frame
    sg_begin_pass(SG_DEFAULT_PASS, &pass_action, WIDTH, HEIGHT);
    sg_end_pass();
    sg_commit();
}
