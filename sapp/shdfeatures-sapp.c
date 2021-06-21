//------------------------------------------------------------------------------
//  shdfeatures-sapp.c
//
//  FIXME: explanation
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "dbgui/dbgui.h"

// shader feature bits
#define NORMALS (1<<0)      // diffuse color == normals
#define LIT (1<<1)          // lit / unlit
#define SKINNED (1<<2)      // vertex skinned
