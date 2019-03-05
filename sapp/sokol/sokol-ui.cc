//------------------------------------------------------------------------------
//  sokol-ui.cc
//  This includes all the sokol header implementations and the debug-UI
//  extension header implementations. Since the UI extension headers
//  target Dear ImGui, the implementation must be compiled as C++
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_TRACE_HOOKS
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
#include "imgui.h"
#include "sokol_gfx_imgui.h"