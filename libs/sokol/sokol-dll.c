#define SOKOL_IMPL
#define SOKOL_DLL
#define SOKOL_NO_ENTRY
#if defined(_WIN32)
#include <Windows.h>
#define SOKOL_LOG(s) OutputDebugStringA(s)
#endif
/* sokol 3D-API defines are provided by build options */
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"
#include "sokol_glue.h"
