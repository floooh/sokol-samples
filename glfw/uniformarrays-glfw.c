//-----------------------------------------------------------------------------
//  uniformarrays-glfw.c
//
//  Tests 'native layout' uniform array handling in the sokol_gfx.h GL backend
//-----------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#include "sokol_debugtext.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

// uniform block C struct
#define ARRAY_COUNT (8)
#define NUM_ARRAYS (3)
typedef struct {
    int sel;
    int idx;
    float offset[2];
    float scale[2];
    float f1[ARRAY_COUNT];
    float f2[ARRAY_COUNT][2];
    float f3[ARRAY_COUNT][3];
} vs_params_t;

static sg_shader create_shader(void);

int main() {

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(640, 480, "GL Uniform Array Test", 0, 0);
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    sg_setup(&(sg_desc){0});
    sdtx_setup(&(sdtx_desc_t){
        .context_pool_size = 1,
        .fonts[0] = sdtx_font_oric(),
    });
    
    const float vertices[] = {
         0.0f,  0.0f,
        +1.0f,  0.0f,
        +1.0f, +1.0f,
         0.0f, +1.0f,
    };
    const uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    sg_bindings bindings = {
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(vertices)
        }),
        .index_buffer = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .data = SG_RANGE(indices)
        })
    };

    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = create_shader(),
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .index_type = SG_INDEXTYPE_UINT16
    });
    
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.5f, 0.75f, 1.0f } }
    };
    
    // initialize uniform block array values
    vs_params_t vs_params = {0};
    for (int idx = 0; idx < ARRAY_COUNT; idx++) {
        float v = (1.0f / ARRAY_COUNT) * idx;
        vs_params.f1[idx]    = v;
        vs_params.f2[idx][0] = v;
        vs_params.f2[idx][1] = v;
        vs_params.f3[idx][0] = v;
        vs_params.f3[idx][1] = v;
        vs_params.f3[idx][2] = v;
    }
    
    while (!glfwWindowShouldClose(win)) {
        int cur_width, cur_height;
        glfwGetFramebufferSize(win, &cur_width, &cur_height);
        
        const float w = (float) cur_width;
        const float h = (float) cur_height;
        const float cw = w * 0.5f;
        const float ch = h * 0.5f;
        const float glyph_w = 8.0f / cw;
        const float glyph_h = 8.0f / ch;
        
        sdtx_canvas(cw, ch);
        sdtx_origin(3, 3);
        sdtx_color3f(1.0f, 1.0f, 1.0f);
        sdtx_puts("You should see 3 rows of increasing\nbrightness (red, yellow, grey)");
        
        sg_begin_default_pass(&pass_action, cur_width, cur_height);
        sg_apply_pipeline(pip);
        sg_apply_bindings(&bindings);
        vs_params.scale[0] = 5.0f * glyph_w;
        vs_params.scale[1] = 2.0f * glyph_h;
        vs_params.offset[1] = 1.0f - (16.0f * glyph_h);
        for (vs_params.sel = 0; vs_params.sel < NUM_ARRAYS; vs_params.sel++) {
            vs_params.offset[0] = -1.0f + (10.0f * glyph_w);
            for (vs_params.idx = 0; vs_params.idx < ARRAY_COUNT; vs_params.idx++) {
                sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));
                sg_draw(0, 6, 1);
                vs_params.offset[0] += glyph_h * 4.0f;
            }
            vs_params.offset[1] -= glyph_h * 4.0f;
        }
        sdtx_draw();
        sg_end_pass();
        sg_commit();
        
        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    sdtx_shutdown();
    sg_shutdown();
    glfwTerminate();
    return 0;
}

// helper function to create shader object
static sg_shader create_shader(void) {
    return sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .layout = SG_UNIFORMLAYOUT_NATIVE,  // this is the default, so not really needed
            .uniforms = {
                { .name = "sel",    .type = SG_UNIFORMTYPE_INT },
                { .name = "idx",    .type = SG_UNIFORMTYPE_INT },
                { .name = "offset", .type = SG_UNIFORMTYPE_FLOAT2 },
                { .name = "scale",  .type = SG_UNIFORMTYPE_FLOAT2 },
                { .name = "f1",     .type = SG_UNIFORMTYPE_FLOAT,  .array_count = ARRAY_COUNT },
                { .name = "f2",     .type = SG_UNIFORMTYPE_FLOAT2, .array_count = ARRAY_COUNT },
                { .name = "f3",     .type = SG_UNIFORMTYPE_FLOAT3, .array_count = ARRAY_COUNT },
            }
        },
        .vs.source =
            "#version 330\n"
            "#define ARRAY_COUNT (8)\n"
            "uniform int sel;\n"
            "uniform int idx;\n"
            "uniform vec2 offset;\n"
            "uniform vec2 scale;\n"
            "uniform float f1[ARRAY_COUNT];\n"
            "uniform vec2 f2[ARRAY_COUNT];\n"
            "uniform vec3 f3[ARRAY_COUNT];\n"
            "in vec2 position;\n"
            "out vec4 color;\n"
            "void main() {\n"
            "  gl_Position = vec4(position * scale + offset, 0.0, 1.0);\n"
            "  if (sel == 0) {\n"
            "    color = vec4(f1[idx], 0.0, 0.0, 1.0);\n"
            "  }\n"
            "  else if (sel == 1) {\n"
            "    color = vec4(f2[idx], 0.0, 1.0);\n"
            "  }\n"
            "  else {\n"
            "    color = vec4(f3[idx], 1.0);\n"
            "  }\n"
            "}\n",
        .fs.source =
            "#version 330\n"
            "in vec4 color;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = color;\n"
            "}\n",
    });
}
