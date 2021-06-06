#include "d3d11entry.h"
#pragma warning(disable:4201)   // needed for /W4 and including d3d11.h
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>

static LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void d3d11_create_default_render_target(void);
static void d3d11_destroy_default_render_target(void);
static void d3d11_update_default_render_target(void);

static const IID _d3d11entry_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c };

static struct {
    bool quit_requested;
    bool in_create_window;
    HWND hwnd;
    DWORD win_style;
    DWORD win_ex_style;
    DXGI_SWAP_CHAIN_DESC swap_chain_desc;
    int width;
    int height;
    int sample_count;
    ID3D11Device* device;
    ID3D11DeviceContext* device_context;
    IDXGISwapChain* swap_chain;
    ID3D11Texture2D* render_target;
    ID3D11RenderTargetView* render_target_view;
    ID3D11Texture2D* depth_stencil_buffer;
    ID3D11DepthStencilView* depth_stencil_view;
    d3d11_key_func key_down_func;
    d3d11_key_func key_up_func;
    d3d11_char_func char_func;
    d3d11_mouse_btn_func mouse_btn_down_func;
    d3d11_mouse_btn_func mouse_btn_up_func;
    d3d11_mouse_pos_func mouse_pos_func;
    d3d11_mouse_wheel_func mouse_wheel_func;
} state = {
    .win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
    .win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE
};

#define SAFE_RELEASE(class, obj) if (obj) { class##_Release(obj); obj=0; }

void d3d11_init(int w, int h, int sample_count, const wchar_t* title) {

    state.width = w;
    state.height = h;
    state.sample_count = sample_count;

    /* register window class */
    RegisterClassW(&(WNDCLASSW){
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = (WNDPROC) d3d11_winproc,
        .hInstance = GetModuleHandleW(NULL),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hIcon = LoadIcon(NULL, IDI_WINLOGO),
        .lpszClassName = L"SOKOLD3D11"
    });

    /* create window */
    state.in_create_window = true;
    RECT rect = { .left = 0, .top = 0, .right = w, .bottom = h };
    AdjustWindowRectEx(&rect, state.win_style, FALSE, state.win_ex_style);
    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;
    state.hwnd = CreateWindowExW(
        state.win_ex_style, // dwExStyle
        L"SOKOLD3D11",      // lpClassName
        title,              // lpWindowName
        state.win_style,    // dwStyle
        CW_USEDEFAULT,      // X
        CW_USEDEFAULT,      // Y
        win_width,          // nWidth
        win_height,         // nHeight
        NULL,               // hWndParent
        NULL,               // hMenu
        GetModuleHandle(NULL),  //hInstance
        NULL);              // lpParam
    ShowWindow(state.hwnd, SW_SHOW);
    state.in_create_window = false;

    /* create device and swap chain */
    state.swap_chain_desc = (DXGI_SWAP_CHAIN_DESC) {
        .BufferDesc = {
            .Width = (UINT)w,
            .Height = (UINT)h,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .RefreshRate = {
                .Numerator = 60,
                .Denominator = 1
            }
        },
        .OutputWindow = state.hwnd,
        .Windowed = true,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
        .BufferCount = 1,
        .SampleDesc = {
            .Count = (UINT) state.sample_count,
            .Quality = (UINT) (state.sample_count > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0)
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT
    };
    UINT create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
    #ifdef _DEBUG
        create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,                       /* pAdapter (use default) */
        D3D_DRIVER_TYPE_HARDWARE,   /* DriverType */
        NULL,                       /* Software */
        create_flags,               /* Flags */
        NULL,                       /* pFeatureLevels */
        0,                          /* FeatureLevels */
        D3D11_SDK_VERSION,          /* SDKVersion */
        &state.swap_chain_desc,     /* pSwapChainDesc */
        &state.swap_chain,          /* ppSwapChain */
        &state.device,              /* ppDevice */
        &feature_level,             /* pFeatureLevel */
        &state.device_context);     /* ppImmediateContext */
    (void)hr;
    assert(SUCCEEDED(hr) && state.swap_chain && state.device && state.device_context);

    /* default render target and depth-stencil-buffer */
    d3d11_create_default_render_target();
}

void d3d11_shutdown(void) {
    d3d11_destroy_default_render_target();
    SAFE_RELEASE(IDXGISwapChain, state.swap_chain);
    SAFE_RELEASE(ID3D11DeviceContext, state.device_context);
    SAFE_RELEASE(ID3D11Device, state.device);
    DestroyWindow(state.hwnd); state.hwnd = 0;
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
}

void d3d11_create_default_render_target(void) {
    HRESULT hr;
    hr = IDXGISwapChain_GetBuffer(state.swap_chain, 0, &_d3d11entry_IID_ID3D11Texture2D, (void**)&state.render_target);
    assert(SUCCEEDED(hr) && state.render_target);
    hr = ID3D11Device_CreateRenderTargetView(state.device, (ID3D11Resource*)state.render_target, NULL, &state.render_target_view);
    assert(SUCCEEDED(hr) && state.render_target_view);

    D3D11_TEXTURE2D_DESC ds_desc = {
        .Width = (UINT)state.width,
        .Height = (UINT)state.height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = state.swap_chain_desc.SampleDesc,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    };
    hr = ID3D11Device_CreateTexture2D(state.device, &ds_desc, NULL, &state.depth_stencil_buffer);
    assert(SUCCEEDED(hr) && state.depth_stencil_buffer);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
        .Format = ds_desc.Format,
        .ViewDimension = state.sample_count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D
    };
    hr = ID3D11Device_CreateDepthStencilView(state.device, (ID3D11Resource*)state.depth_stencil_buffer, &dsv_desc, &state.depth_stencil_view);
    assert(SUCCEEDED(hr) && state.depth_stencil_view);
}

void d3d11_destroy_default_render_target(void) {
    SAFE_RELEASE(ID3D11Texture2D, state.render_target);
    SAFE_RELEASE(ID3D11RenderTargetView, state.render_target_view);
    SAFE_RELEASE(ID3D11Texture2D, state.depth_stencil_buffer);
    SAFE_RELEASE(ID3D11DepthStencilView, state.depth_stencil_view);
}

void d3d11_update_default_render_target(void) {
    if (state.swap_chain) {
        d3d11_destroy_default_render_target();
        IDXGISwapChain_ResizeBuffers(state.swap_chain, 1, state.width, state.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        d3d11_create_default_render_target();
    }
}

bool d3d11_process_events(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
            state.quit_requested = true;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return !state.quit_requested;
}

void d3d11_present(void) {
    IDXGISwapChain_Present(state.swap_chain, 1, 0);
    /* handle window resizing */
    RECT r;
    if (GetClientRect(state.hwnd, &r)) {
        const int cur_width = r.right - r.left;
        const int cur_height = r.bottom - r.top;
        if (((cur_width > 0) && (cur_width != state.width)) ||
            ((cur_height > 0) && (cur_height != state.height)))
        {
            /* need to reallocate the default render target */
            state.width = cur_width;
            state.height = cur_height;
            d3d11_update_default_render_target();
        }
    }
}

LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            state.quit_requested = true;
            return 0;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_LBUTTONDOWN:
            if (state.mouse_btn_down_func) {
                state.mouse_btn_down_func(0);
            }
            break;
        case WM_RBUTTONDOWN:
            if (state.mouse_btn_down_func) {
                state.mouse_btn_down_func(1);
            }
            break;
        case WM_LBUTTONUP:
            if (state.mouse_btn_up_func) {
                state.mouse_btn_up_func(0);
            }
            break;
        case WM_RBUTTONUP:
            if (state.mouse_btn_up_func) {
                state.mouse_btn_up_func(1);
            }
            break;
        case WM_MOUSEMOVE:
            if (state.mouse_pos_func) {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                state.mouse_pos_func((float)x, (float)y);
            }
            break;
        case WM_MOUSEWHEEL:
            if (state.mouse_wheel_func) {
                state.mouse_wheel_func((float)((SHORT)HIWORD(wParam) / 30.0f));
            }
            break;
        case WM_CHAR:
            if (state.char_func) {
                state.char_func((wchar_t)wParam);
            }
            break;
        case WM_KEYDOWN:
            if (state.key_down_func) {
                state.key_down_func((int)wParam);
            }
            break;
        case WM_KEYUP:
            if (state.key_up_func) {
                state.key_up_func((int)wParam);
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static const void* d3d11_device(void) {
    return (const void*) state.device;
}

static const void* d3d11_device_context(void) {
    return (const void*) state.device_context;
}

static const void* d3d11_render_target_view(void* user_data) {
    assert(0xDEADBEEF == (uintptr_t) user_data);
    (void)user_data; // silence unused warning in release mode
    return (const void*) state.render_target_view;
}

static const void* d3d11_depth_stencil_view(void* user_data) {
    assert(0xDEADBEEF == (uintptr_t) user_data);
    (void)user_data; // silence unused warning in release mode
    return (const void*) state.depth_stencil_view;
}

sg_context_desc d3d11_get_context(void) {
    return (sg_context_desc) {
        .sample_count = state.sample_count,
        .d3d11 = {
            .device = d3d11_device(),
            .device_context = d3d11_device_context(),
            .render_target_view_userdata_cb = d3d11_render_target_view,
            .depth_stencil_view_userdata_cb = d3d11_depth_stencil_view,
            .user_data = (void*)(uintptr_t)0xDEADBEEF
        }
    };
}

int d3d11_width(void) {
    return state.width;
}

int d3d11_height() {
    return state.height;
}

/* register input callbacks */
void d3d11_key_down(d3d11_key_func f) {
    state.key_down_func = f;
}

void d3d11_key_up(d3d11_key_func f) {
    state.key_up_func = f;
}

void d3d11_char(d3d11_char_func f) {
    state.char_func = f;
}

void d3d11_mouse_btn_down(d3d11_mouse_btn_func f) {
    state.mouse_btn_down_func = f;
}

void d3d11_mouse_btn_up(d3d11_mouse_btn_func f) {
    state.mouse_btn_up_func = f;
}

void d3d11_mouse_pos(d3d11_mouse_pos_func f) {
    state.mouse_pos_func = f;
}

void d3d11_mouse_wheel(d3d11_mouse_wheel_func f) {
    state.mouse_wheel_func = f;
}
