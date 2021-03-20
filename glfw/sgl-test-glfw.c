//------------------------------------------------------------------------------
//  sgl-test-glfw.c
//  This doesn't have any sokol header code, it's a pure GL 1.2 program
//  for testing whether sokol-gl behaves like a real GL.
//------------------------------------------------------------------------------
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "flextgl12/flextGL.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif
#include "glu.h"

static void reset_state(void) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

static void draw_triangle(void) {
    reset_state();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(0.0f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(-0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.5f, -0.5f);
    glEnd();
}

static float rad(float deg) {
    return (deg * (float)M_PI) / 180.0f;
}

static void draw_quad(void) {
    static float angle_deg = 0.0f;
    float scale = 1.0f + sinf(rad(angle_deg)) * 0.5f;
    angle_deg += 1.0f;
    reset_state();
    glRotatef(angle_deg, 0.0f, 0.0f, 1.0f);
    glScalef(scale, scale, 1.0f);
    glBegin(GL_QUADS);
    glColor3f(1.0f, 1.0f, 0.0f);
    glVertex2f(-0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.5f, -0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(0.5f, 0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(-0.5f, 0.5f);
    glEnd();
}

/* vertex specification for a cube with colored sides and texture coords */
static void cube(void) {
    glBegin(GL_QUADS);
    glColor3f(1.0f, 0.0f, 0.0f);
        glTexCoord2f(-1.0f,  1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
        glTexCoord2f( 1.0f,  1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
        glTexCoord2f( 1.0f, -1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
        glTexCoord2f(-1.0f, -1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
        glTexCoord2f(-1.0f,  1.0f); glVertex3f(-1.0, -1.0,  1.0);
        glTexCoord2f( 1.0f,  1.0f); glVertex3f( 1.0, -1.0,  1.0);
        glTexCoord2f( 1.0f, -1.0f); glVertex3f( 1.0,  1.0,  1.0);
        glTexCoord2f(-1.0f, -1.0f); glVertex3f(-1.0,  1.0,  1.0);
    glColor3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(-1.0f,  1.0f); glVertex3f(-1.0, -1.0,  1.0);
        glTexCoord2f( 1.0f,  1.0f); glVertex3f(-1.0,  1.0,  1.0);
        glTexCoord2f( 1.0f, -1.0f); glVertex3f(-1.0,  1.0, -1.0);
        glTexCoord2f(-1.0f, -1.0f); glVertex3f(-1.0, -1.0, -1.0);
    glColor3f(1.0f, 0.5f, 0.0f);
        glTexCoord2f(-1.0f,  1.0f); glVertex3f(1.0, -1.0,  1.0);
        glTexCoord2f( 1.0f,  1.0f); glVertex3f(1.0, -1.0, -1.0);
        glTexCoord2f( 1.0f, -1.0f); glVertex3f(1.0,  1.0, -1.0);
        glTexCoord2f(-1.0f, -1.0f); glVertex3f(1.0,  1.0,  1.0);
    glColor3f(0.0f, 0.5f, 1.0f);
        glTexCoord2f(-1.0f,  1.0f); glVertex3f( 1.0, -1.0, -1.0);
        glTexCoord2f( 1.0f,  1.0f); glVertex3f( 1.0, -1.0,  1.0);
        glTexCoord2f( 1.0f, -1.0f); glVertex3f(-1.0, -1.0,  1.0);
        glTexCoord2f(-1.0f, -1.0f); glVertex3f(-1.0, -1.0, -1.0);
    glColor3f(1.0f, 0.0f, 0.5f);
        glTexCoord2f(-1.0f,  1.0f); glVertex3f(-1.0,  1.0, -1.0);
        glTexCoord2f( 1.0f,  1.0f); glVertex3f(-1.0,  1.0,  1.0);
        glTexCoord2f( 1.0f, -1.0f); glVertex3f( 1.0,  1.0,  1.0);
        glTexCoord2f(-1.0f, -1.0f); glVertex3f( 1.0,  1.0, -1.0);
    glEnd();
}

static void draw_cubes(void) {
    static float rot[2] = { 0.0f, 0.0f };
    rot[0] += 1.0f;
    rot[1] += 2.0f;

    reset_state();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    gluPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.0f, 0.0f, -12.0f);
    glRotatef(rot[0], 1.0f, 0.0f, 0.0f);
    glRotatef(rot[1], 0.0f, 1.0f, 0.0f);
    cube();
    glPushMatrix();
        glTranslatef(0.0f, 0.0f, 3.0f);
        glScalef(0.5f, 0.5f, 0.5f);
        glRotatef(-2.0f * rot[0], 1.0f, 0.0f, 0.0f);
        glRotatef(-2.0f * rot[1], 0.0f, 1.0f, 0.0f);
        cube();
        glPushMatrix();
            glTranslatef(0.0f, 0.0f, 3.0f);
            glScalef(0.5f, 0.5f, 0.5f);
            glRotatef(-3.0f * 2*rot[0], 1.0f, 0.0f, 0.0f);
            glRotatef(3.0f * 2*rot[1], 0.0f, 0.0f, 1.0f);
            cube();
        glPopMatrix();
    glPopMatrix();
}

static void draw_tex_cube(void) {
    static float frame_count = 0.0f;
    frame_count += 1.0f;
    float a = (float) frame_count;

    /* texture matrix rotation and scale */
    float tex_rot = 0.5f * a;
    const float tex_scale = 1.0f + sinf(rad(a)) * 0.5f;

    /* compute an orbiting eye-position for testing sgl_lookat() */
    float eye_x = sinf(rad(a)) * 6.0f;
    float eye_z = cosf(rad(a)) * 6.0f;
    float eye_y = sinf(rad(a)) * 3.0f;

    reset_state();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    gluPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    gluLookAt(eye_x, eye_y, eye_z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    glMatrixMode(GL_TEXTURE);
    glRotatef(tex_rot, 0.0f, 0.0f, 1.0f);
    glScalef(tex_scale, tex_scale, 1.0f);
    cube();
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    GLFWwindow* w = glfwCreateWindow(512, 512, "SGL/GL Test GLFW", 0, 0);
    glfwMakeContextCurrent(w);
    glfwSwapInterval(1);
    flextInit();

    /* a checkerboard texture */
    uint32_t pixels[8][8];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            pixels[y][x] = ((y ^ x) & 1) ? 0xFFFFFFFF : 0xFF000000;
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    while (!glfwWindowShouldClose(w)) {
        int dw, dh;
        glfwGetFramebufferSize(w, &dw, &dh);
        const int ww = dh/2; /* not a bug */
        const int hh = dh/2;
        const int x0 = dw/2 - hh;
        const int x1 = dw/2;
        const int y0 = dh/2;
        const int y1 = 0;

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glViewport(x0, y0, ww, hh);
        draw_triangle();
        glViewport(x1, y0, ww, hh);
        draw_quad();
        glViewport(x0, y1, ww, hh);
        draw_cubes();
        glViewport(x1, y1, ww, hh);
        draw_tex_cube();
        glViewport(0, 0, dw, dh);
        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    glfwTerminate();
}

