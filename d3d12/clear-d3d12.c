//------------------------------------------------------------------------------
//  clear-d3d12.c
//------------------------------------------------------------------------------
#include "d3d12entry.h"
#define SOKOL_IMPL
#define SOKOL_D3D12
#include "sokol_gfx.h"
#include "sokol_log.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    // setup the D3D12 app wrapper
    d3d12_init(&(d3d12_desc_t){ .width = 640, .height = 480, .title = L"clear-d3d12.c" });

    // setup sokol
    sg_setup(&(sg_desc){
        .environment = d3d12_environment(),
        .logger.func = slog_func,
    });

    // initial pass action: clear to red
    sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    // draw loop
    while (d3d12_process_events()) {
        float g = pass_action.colors[0].clear_value.g + 0.01f;
        if (g > 1.0f) g = 0.0f;
        pass_action.colors[0].clear_value.g = g;
        sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = d3d12_swapchain() });
        sg_end_pass();
        sg_commit();
        d3d12_present();
    }
    // shutdown sokol_gfx and D3D12 app wrapper
    sg_shutdown();
    d3d12_shutdown();
    return 0;
}
