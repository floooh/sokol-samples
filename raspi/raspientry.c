#include "raspientry.h"
#include <EGL/egl.h>
#include "bcm_host.h"

static EGLDisplay egl_display;
static EGLConfig egl_config;
static EGLSurface egl_surface;
static EGLContext egl_context;
static EGLint egl_width;
static EGLint egl_height;

void rapi_init(int sample_count) {
    EGLBoolean res;
    bcm_host_init();
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(eglGetError() == EGL_SUCCESS);
    res = eglInitialize(egl_display, 0, 0);
    assert(res);
    EGLint egl_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_SAMPLES, sample_count,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLint num_configs = 0;
    res = eglChooseConfig(egl_display, egl_attrs, &egl_config, 1, &num_configs);
    assert(res && (eglGetError() == EGL_SUCCESS));
    res = eglBindAPI(EGL_OPENGL_ES_API);
    assert(res);
    EGLint ctx_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, ctx_attrs);
    assert(egl_context && (eglGetError() == EGL_SUCCESS));

    uint32_t rpi_width, rpi_height;
    int32_t success = graphics_get_display_size(0, &rpi_width, &rpi_height);
    assert(success >= 0);

    VC_RECT_T dst_rect, src_rect;
    dst_rect.x = dst_rect.y = 0;
    dst_rect.width = egl_width;
    dst_rect.height = egl_height;
    src_rect.x = src_rect.y = 0;
    src_rect.width = egl_width << 16;
    src_rect.height = egl_height << 16;

    DISPMANX_DISPLAY_HANDLE_T dm_display = vc_dispmanx_display_open(0);
    DISPMANX_UPDATE_HANDLE_T dm_update = vc_dispmanx_update_start(0);
    DISPMANX_ELEMENT_HANDLE_T dm_elm = vc_dispmanx_element_add(
        dm_update,
        dm_display,
        0, // layer
        &dst_rect,
        0, // src
        &src_rect,
        DISPMANX_PROTECTION_NONE,
        0, // alpha
        0, // clamp
        DISPMANX_NO_ROTATE); // transform
    vc_dispmanx_update_submit_sync(dm_update);

    EGL_DISPMANX_WINDOW_T rpi_win;
    rpi_win.element = dm_elm;
    rpi_win.width = rpi_width;
    rpi_win.height = rpi_height;

    EGLNativeWindowType window = &rpi_win;
    egl_surface = eglCreateWindowSurface(egl_display, egl_config, window, 0);
    assert(egl_surface && (eglGetError() != EGL_SUCCESS));
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &egl_width);
    eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &egl_height);
}

void raspi_shutdown() {
    assert(egl_display && egl_context && egl_surface);
    eglDestroyContext(egl_display, egl_context); egl_context = 0;
    eglDestroySurface(egl_display, egl_surface); egl_surface = 0;
    eglTerminate(egl_display);
    egl_config = 0;
    egl_display = 0;
}

void raspi_present() {
    assert(egl_display && egl_surface);
    eglSwapBuffers(egl_display, egl_surface);
}

