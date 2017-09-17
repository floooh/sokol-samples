//------------------------------------------------------------------------------
//  clear-d3d11.c
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#include "sokol_gfx.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const int WIDTH = 640;
    const int HEIGHT = 480;
    d3d11_init(WIDTH, HEIGHT, 1, "Sokol Clear D3D11");
    while (d3d11_process_events()) {
        d3d11_present();
    }
    d3d11_shutdown();
    return 0;
}
