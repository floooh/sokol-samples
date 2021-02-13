//------------------------------------------------------------------------------
//  clear-d3d11.c
//------------------------------------------------------------------------------
#include "d3d11entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_LOG(s) OutputDebugStringA(s)
#include "sokol_gfx.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    /* setup the D3D11 app wrapper */
    d3d11_init(640, 480, 1, L"Sokol Clear D3D11");

    /* setup sokol */
    sg_setup(&(sg_desc){
        .context = d3d11_get_context()
    });

    /* initial pass action: clear to red */
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    /* draw loop */
    while (d3d11_process_events()) {
        float g = pass_action.colors[0].value.g + 0.01f;
        if (g > 1.0f) g = 0.0f;
        pass_action.colors[0].value.g = g;
        sg_begin_default_pass(&pass_action, d3d11_width(), d3d11_height());
        sg_end_pass();
        sg_commit();
        d3d11_present();
    }
    /* shutdown sokol_gfx and D3D11 app wrapper */
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
