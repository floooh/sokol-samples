//------------------------------------------------------------------------------
//  primtypes-sapp.c
//
//  Test/demonstrate the various primitive types.
//------------------------------------------------------------------------------
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"
#include "primtypes-sapp.glsl.h"
#include <assert.h>

#define NUM_X (32)
#define NUM_Y (32)
#define NUM_VERTS (NUM_X * NUM_Y)
#define NUM_LINE_INDICES (NUM_X * (NUM_Y-1) * 2)
#define NUM_LINE_STRIP_INDICES (NUM_X * (NUM_Y-1))
#define NUM_TRIANGLE_INDICES ((NUM_X-1) * (NUM_Y-1) * 3)
#define NUM_TRIANGLE_STRIP_INDICES ((NUM_X-1) * (NUM_Y-1) * 2 + (NUM_Y-1) * 2)

// primitive type array indices
#define POINTS (0)
#define LINES (1)
#define LINE_STRIP (2)
#define TRIANGLES (3)
#define TRIANGLE_STRIP (4)
#define NUM_PRIMITIVE_TYPES (5)

typedef struct {
    float x, y;
    uint32_t color;
} vertex_t;

static struct {
    int cur_prim_type;
    sg_pass_action pass_action;
    sg_buffer vbuf;
    // per-primitive-type data
    struct {
        sg_buffer ibuf;
        sg_pipeline pip;
        int num_elements;
    } prim[NUM_PRIMITIVE_TYPES];
    float rx, ry;
    float point_size;

    vertex_t vertices[NUM_VERTS];
    struct {
        uint16_t lines[NUM_LINE_INDICES];
        uint16_t line_strip[NUM_LINE_STRIP_INDICES];
        uint16_t triangles[NUM_TRIANGLE_INDICES];
        uint16_t triangle_strip[NUM_TRIANGLE_STRIP_INDICES];
    } indices;
} state;

// helper function prototypes
static void print_status_text(float disp_w, float disp_h);
static void setup_vertex_and_index_data(void);
static vs_params_t compute_vsparams(float disp_w, float disp_h, float rx, float ry, float point_size);

static void init(void) {
    state.cur_prim_type = POINTS;
    state.point_size = 4.0f;

    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    __dbgui_setup(sapp_sample_count());
    // setup sokol-debugtext
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_z1013()
    });

    // vertex- and index-buffers
    setup_vertex_and_index_data();
    state.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(state.vertices),
    });
    sg_range index_data[NUM_PRIMITIVE_TYPES] = {
        [POINTS]         = { 0 }, // no index buffer for point list
        [LINES]          = SG_RANGE(state.indices.lines),
        [LINE_STRIP]     = SG_RANGE(state.indices.line_strip),
        [TRIANGLES]      = SG_RANGE(state.indices.triangles),
        [TRIANGLE_STRIP] = SG_RANGE(state.indices.triangle_strip)
    };
    for (int i = 0; i < NUM_PRIMITIVE_TYPES; i++) {
        if (index_data[i].ptr) {
            state.prim[i].ibuf = sg_make_buffer(&(sg_buffer_desc){
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .data = index_data[i]
            });
        }
        else {
            state.prim[i].ibuf.id = SG_INVALID_ID;
        }
    }

    // create pipeline state objects for each primitive type
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
            .attrs[ATTR_vs_color0].format = SG_VERTEXFORMAT_UBYTE4N,
        },
        .shader = sg_make_shader(primtypes_shader_desc(sg_query_backend())),
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL
        }
    };
    sg_primitive_type prim_types[NUM_PRIMITIVE_TYPES] = {
        [POINTS]         = SG_PRIMITIVETYPE_POINTS,
        [LINES]          = SG_PRIMITIVETYPE_LINES,
        [LINE_STRIP]     = SG_PRIMITIVETYPE_LINE_STRIP,
        [TRIANGLES]      = SG_PRIMITIVETYPE_TRIANGLES,
        [TRIANGLE_STRIP] = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    };
    for (int i = 0; i < NUM_PRIMITIVE_TYPES; i++) {
        // point lists don't use indexed rendering
        pip_desc.index_type = (i == POINTS) ? SG_INDEXTYPE_NONE : SG_INDEXTYPE_UINT16;
        pip_desc.primitive_type = prim_types[i];
        state.prim[i].pip = sg_make_pipeline(&pip_desc);
    }

    // the number of elements (vertices or indices) to render
    state.prim[POINTS].num_elements         = NUM_VERTS;
    state.prim[LINES].num_elements          = NUM_LINE_INDICES;
    state.prim[LINE_STRIP].num_elements     = NUM_LINE_STRIP_INDICES;
    state.prim[TRIANGLES].num_elements      = NUM_TRIANGLE_INDICES;
    state.prim[TRIANGLE_STRIP].num_elements = NUM_TRIANGLE_STRIP_INDICES;

    // pass action for clearing the framebuffer
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.2f, 0.4f, 1.0f } }
    };
}

static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 0.3f * t;
    state.ry += 0.2f * t;
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const vs_params_t vs_params = compute_vsparams(w, h, state.rx, state.ry, state.point_size);

    print_status_text(w, h);

    sg_begin_default_passf(&state.pass_action, w, h);
    sg_apply_pipeline(state.prim[state.cur_prim_type].pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.vbuf,
        .index_buffer = state.prim[state.cur_prim_type].ibuf,
    });
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
    sg_draw(0, state.prim[state.cur_prim_type].num_elements, 1);
    sdtx_draw();
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

// input helpers to cycle through primitive types
static void next_prim_type(void) {
    state.cur_prim_type++;
    if (state.cur_prim_type >= NUM_PRIMITIVE_TYPES) {
        state.cur_prim_type = 0;
    }
}

static void prev_prim_type(void) {
    state.cur_prim_type--;
    if (state.cur_prim_type < 0) {
        state.cur_prim_type = NUM_PRIMITIVE_TYPES - 1;
    }
}

static void incr_point_size(void) {
    state.point_size += 1.0f;
}

static void decr_point_size(void) {
    state.point_size -= 1.0f;
    if (state.point_size < 1.0f) {
        state.point_size = 1.0f;
    }
}

static void input(const sapp_event* ev) {
    __dbgui_event(ev);
    switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            switch (ev->key_code) {
                case SAPP_KEYCODE_1: state.cur_prim_type = POINTS; break;
                case SAPP_KEYCODE_2: state.cur_prim_type = LINES; break;
                case SAPP_KEYCODE_3: state.cur_prim_type = LINE_STRIP; break;
                case SAPP_KEYCODE_4: state.cur_prim_type = TRIANGLES; break;
                case SAPP_KEYCODE_5: state.cur_prim_type = TRIANGLE_STRIP; break;
                case SAPP_KEYCODE_UP: prev_prim_type(); break;
                case SAPP_KEYCODE_DOWN: next_prim_type(); break;
                case SAPP_KEYCODE_LEFT: decr_point_size(); break;
                case SAPP_KEYCODE_RIGHT: incr_point_size(); break;
                default: break;
            }
            break;
        // touch/mouse: switch to next primitive type:
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            next_prim_type();
            break;
        default: break;
    }
}

static void cleanup(void) {
    sdtx_shutdown();
    __dbgui_shutdown();
    sg_shutdown();
}

// helper function to compute vertex shader params
vs_params_t compute_vsparams(float disp_w, float disp_h, float rx, float ry, float point_size) {
    hmm_mat4 proj = HMM_Perspective(60.0f, disp_w/disp_h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 1.5f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    hmm_mat4 rxm = HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    return (vs_params_t){
        .mvp = HMM_MultiplyMat4(view_proj, model),
        .point_size = point_size,
    };
}

// helper function to print the help/status text
void print_status_text(float disp_w, float disp_h) {
    sdtx_canvas(disp_w*0.5f, disp_h*0.5f);
    sdtx_origin(1.0f, 2.0f);
    sdtx_color3f(1.0f, 1.0f, 1.0f);
    sdtx_printf("Point Size (left/right keys): %d\n\n", (int)state.point_size);
    sdtx_puts("Primitive Type (1..5/up/down keys):\n");
    const char* items[NUM_PRIMITIVE_TYPES] = {
        "1: Point List\n",
        "2: Line List\n",
        "3: Line Strip\n",
        "4: Triangle List\n",
        "5: Triangle Strip\n",
    };
    for (int i = 0; i < NUM_PRIMITIVE_TYPES; i++) {
        if (i == state.cur_prim_type) {
            sdtx_puts("==> ");
        }
        else {
            sdtx_puts("    ");
        }
        sdtx_puts(items[i]);
    }
}

// helper function to fill index data
void setup_vertex_and_index_data(void) {
    // vertices
    {
        const float dx = 1.0f / NUM_X;
        const float dy = 1.0f / NUM_Y;
        const float offset_x = -dx * (NUM_X / 2);
        const float offset_y = -dy * (NUM_Y / 2);
                                     // red      green       yellow
        const uint32_t colors[3] = { 0xFF0000DD, 0xFF00DD00, 0xFF00DDDD };
        int i = 0;
        for (int y = 0; y < NUM_Y; y++) {
            for (int x = 0; x < NUM_X; x++, i++) {
                assert(i < NUM_VERTS);
                vertex_t* vtx = &state.vertices[i];
                vtx->x = x * dx + offset_x;
                vtx->y = y * dy + offset_y;
                vtx->color = colors[i % 3];
            }
        }
        assert(i == NUM_VERTS);
    }

    { // line-list indices
        int ii = 0;
        for (int y = 0; y < (NUM_Y - 1); y++) {
            for (int x = 0; x < NUM_X; x++) {
                const uint16_t i0 = (x & 1) ? (y * NUM_X + x - 1) : (y * NUM_X + x + 1);
                const uint16_t i1 = (x & 1) ? (i0 + NUM_X + 1) : (i0 + NUM_X - 1);
                assert((ii < (NUM_LINE_INDICES-1)) && (i0 < NUM_VERTS) && (i1 < NUM_VERTS));
                state.indices.lines[ii++] = i0;
                state.indices.lines[ii++] = i1;
            }
        }
        assert(ii == NUM_LINE_INDICES);
    }

    { // line-strip indices
        int ii = 0;
        for (int y = 0; y < (NUM_Y - 1); y++) {
            for (int x = 0; x < NUM_X; x++) {
                const uint16_t i0 = (x & 1) ? (y * NUM_X + x) : ((y + 1) * NUM_X + x);
                assert((ii < NUM_LINE_STRIP_INDICES) && (i0 < NUM_VERTS));
                state.indices.line_strip[ii++] = i0;
            }
        }
        assert(ii == NUM_LINE_STRIP_INDICES);
    }

    { // triangle-list indices
        int ii = 0;
        for (int y = 0; y < (NUM_Y - 1); y++) {
            for (int x = 0; x < (NUM_X - 1); x++) {
                const uint16_t i0 = x + y * NUM_X;
                const uint16_t i1 = (x & 1) ? (i0 + NUM_X) : (i0 + 1);
                const uint16_t i2 = (x & 1) ? (i1 + 1) : (i0 + NUM_X);
                assert((ii < (NUM_TRIANGLE_INDICES-1)) && (i0 < NUM_VERTS) && (i1 < NUM_VERTS) && (i2 < NUM_VERTS));
                state.indices.triangles[ii++] = i0;
                state.indices.triangles[ii++] = i1;
                state.indices.triangles[ii++] = i2;
            }
        }
        assert(ii == NUM_TRIANGLE_INDICES);
    }

    { // triangle strip indices
        int ii = 0;
        for (int y = 0; y < (NUM_Y - 1); y++) {
            uint16_t i0, i1;
            for (int x = 0; x < (NUM_X - 1); x++) {
                i0 = x + y * NUM_Y;
                i1 = i0 + NUM_X;
                assert((ii < (NUM_TRIANGLE_STRIP_INDICES-1)) && (i0 < NUM_VERTS) && (i1 < NUM_VERTS));
                state.indices.triangle_strip[ii++] = i0;
                state.indices.triangle_strip[ii++] = i1;
            }
            // add a degenerate triangle at the end of each 'line'
            i0 = (y + 1) * NUM_X;
            assert((ii < (NUM_TRIANGLE_STRIP_INDICES-1)) && (i0 < NUM_VERTS) && (i1 < NUM_VERTS));
            state.indices.triangle_strip[ii++] = i1;
            state.indices.triangle_strip[ii++] = i0;
        }
        assert(ii == NUM_TRIANGLE_STRIP_INDICES);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = 4,
        .gl_force_gles2 = true,
        .window_title = "Primitive Types",
        .icon.sokol_default = true,
    };
}
