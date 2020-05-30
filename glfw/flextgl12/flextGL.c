/* WARNING: This file was automatically generated */
/* Do not edit. */

#include "flextGL.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(WIN32)
#define FLEXT_CALL __stdcall
#else
#define FLEXT_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* FLEXT_C_EXTENSION
 * Compiling in strict c leads to errors due to void* to function ptr
 * cast being illegal. Mark as extension so that the compiler will
 * accept it.
 */
#if defined(__GNUC__) || defined(__clang__)
#define FLEXT_C_EXTENSION(x) __extension__(x)
#else
#define FLEXT_C_EXTENSION(x) x
#endif

typedef void(FLEXT_CALL *GLPROC)();

void flextLoadOpenGLFunctions(void);

static void open_libgl(void);
static void close_libgl(void);
static GLPROC get_proc(const char *proc);
static void add_extension(const char* extension);

int flextInit(void)
{
    GLint minor, major;
    const char* p;
    GLubyte* extensions;
    GLubyte* i;

    open_libgl();
    flextLoadOpenGLFunctions();
    close_libgl();

    p = (const char *) glGetString(GL_VERSION);
    if (p == NULL) {
        return GL_FALSE;
    }

    for (major = 0; *p >= '0' && *p <= '9'; p++) {
        major = 10 * major + *p - '0';
    }

    for (minor = 0, p++; *p >= '0' && *p <= '9'; p++) {
        minor = 10 * minor + *p - '0';
    }

    /* --- Check for minimal version and profile --- */

    if (major * 10 + minor < 12) {
#if !defined(FLEXT_NO_LOGGING)
        fprintf(stderr, "Error: OpenGL version 1.2 not supported.\n");
        fprintf(stderr, "       Your version is %d.%d.\n", major, minor);
        fprintf(stderr, "       Try updating your graphics driver.\n");
#endif
        return GL_FALSE;
    }


    /* --- Check for extensions --- */

    extensions = (GLubyte*) strdup((const char*)glGetString(GL_EXTENSIONS));
    i = extensions;

    while (*i != '\0') {
        GLubyte *ext = i;
        while (*i != ' ' && *i != '\0') {
            ++i;
        }
        while (*i == ' ') {
            *i = '\0';
            ++i;
        }

        add_extension((const char*)ext);
    }

    free(extensions);


    return GL_TRUE;
}



void flextLoadOpenGLFunctions(void)
{
    /* --- Function pointer loading --- */

    int i;
    for(i = 0 ; i < 4 ; i++){
        glpf.fp[i] = get_proc(glpf.fp[i]);
    }
}

/* ----------------------- Extension flag definitions ---------------------- */

/* ---------------------- Function pointer definitions --------------------- */
GLPF glpf = {{
/* GL_VERSION_1_2 */
/* 4 functions */
"glCopyTexSubImage3D",
"glDrawRangeElements",
"glTexImage3D",
"glTexSubImage3D",

}};


static void add_extension(const char* extension)
{
    (void)extension;
}


/* ------------------ get_proc from Slavomir Kaslev's gl3w ----------------- */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

static HMODULE libgl;

static void open_libgl(void)
{
    libgl = LoadLibraryA("opengl32.dll");
}

static void close_libgl(void)
{
    FreeLibrary(libgl);
}

static GLPROC get_proc(const char *proc)
{
    GLPROC res;

    res = wglGetProcAddress(proc);
    if (!res)
        res = GetProcAddress(libgl, proc);
    return res;
}
#elif defined(__APPLE__) || defined(__APPLE_CC__)
#include <Carbon/Carbon.h>

CFBundleRef bundle;
CFURLRef bundleURL;

static void open_libgl(void)
{
    bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                CFSTR("/System/Library/Frameworks/OpenGL.framework"),
                kCFURLPOSIXPathStyle, true);

    bundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);
    assert(bundle != NULL);
}

static void close_libgl(void)
{
    CFRelease(bundle);
    CFRelease(bundleURL);
}

static GLPROC get_proc(const char *proc)
{
    GLPROC res;

    CFStringRef procname = CFStringCreateWithCString(kCFAllocatorDefault, proc,
                kCFStringEncodingASCII);
    FLEXT_C_EXTENSION(res = CFBundleGetFunctionPointerForName(bundle, procname));
    CFRelease(procname);
    return res;
}
#elif defined(ANDROID)
#include <EGL/egl.h>

static void open_libgl(void)
{
    // nothing to do
}

static void close_libgl(void)
{
    // nothing to do
}

static GLPROC get_proc(const char *proc)
{
    GLPROC res;
    res = eglGetProcAddress((const char *) proc);
    return res;
}
#else
#include <dlfcn.h>
#include <GL/glx.h>

static void *libgl;

static void open_libgl(void)
{
    libgl = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
}

static void close_libgl(void)
{
    dlclose(libgl);
}

static GLPROC get_proc(const char *proc)
{
    GLPROC res;

    res = glXGetProcAddress((const GLubyte *) proc);
    if (!res) {
        FLEXT_C_EXTENSION(res = dlsym(libgl, proc));
    }
    return res;
}
#endif

#ifdef __cplusplus
}
#endif
