#define SOKOL_IMPL
#define SOKOL_D3D11_SHADER_COMPILER
/* sokol 3D-API defines are provided by build options */
#if defined(_WIN32) && defined(SOKOL_GLCORE33)
#include "gl.h"
#endif
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
