/* sokol implementations need to live in it's own source file, because
on MacOS and iOS the implementation must be compiled as Objective-C, so there
must be a *.m file on MacOS/iOS, and *.c file everywhere else
*/
#define SOKOL_IMPL
#define SOKOL_D3D11_SHADER_COMPILER
#if defined(_WIN32)
#include <Windows.h>
#define SOKOL_LOG(s) OutputDebugStringA(s)
#endif
/* sokol 3D-API defines are provided by build options */
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"
