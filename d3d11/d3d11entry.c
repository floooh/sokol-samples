#include "d3d11entry.h"
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>

static LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void d3d11_create_default_render_target();
static void d3d11_destroy_default_render_target();
static void d3d11_update_default_render_target();

static bool quit_requested = false;
static bool in_create_window = false;
static HWND hwnd = NULL;
static DWORD win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX; 
static DWORD win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
static DXGI_SWAP_CHAIN_DESC swap_chain_desc = { 0 };
static int width = 0;
static int height = 0;
static ID3D11Device* device = 0;
static ID3D11DeviceContext* device_context = 0;
static IDXGISwapChain* swap_chain = 0;
static ID3D11Texture2D* render_target = 0;
static ID3D11RenderTargetView* render_target_view = 0;
static ID3D11Texture2D* depth_stencil_buffer = 0;
static ID3D11DepthStencilView* depth_stencil_view = 0;

static d3d11_key_func key_down_func = 0;
static d3d11_key_func key_up_func = 0;
static d3d11_char_func char_func = 0;
static d3d11_mouse_btn_func mouse_btn_down_func = 0;
static d3d11_mouse_btn_func mouse_btn_up_func = 0;
static d3d11_mouse_pos_func mouse_pos_func = 0;
static d3d11_mouse_wheel_func mouse_wheel_func = 0;

#define SAFE_RELEASE(class, obj) if (obj) { class##_Release(obj); obj=0; }

void d3d11_init(int w, int h, int sample_count, const wchar_t* title) {

    width = w;
    height = h;

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
    in_create_window = true;
    RECT rect = { .left = 0, .top = 0, .right = w, .bottom = h };
    AdjustWindowRectEx(&rect, win_style, FALSE, win_ex_style);
    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;
    hwnd = CreateWindowExW(
        win_ex_style,       // dwExStyle
        L"SOKOLD3D11",      // lpClassName
        title,              // lpWindowName
        win_style,          // dwStyle
        CW_USEDEFAULT,      // X
        CW_USEDEFAULT,      // Y
        win_width,          // nWidth
        win_height,         // nHeight
        NULL,               // hWndParent
        NULL,               // hMenu
        GetModuleHandle(NULL),  //hInstance
        NULL);              // lpParam
    ShowWindow(hwnd, SW_SHOW);
    in_create_window = false;

    /* create device and swap chain */
    swap_chain_desc = (DXGI_SWAP_CHAIN_DESC) {
        .BufferDesc = {
            .Width = w,
            .Height = h,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .RefreshRate = {
                .Numerator = 60,
                .Denominator = 1
            }
        },
        .OutputWindow = hwnd,
        .Windowed = true,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
        .BufferCount = 1,
        .SampleDesc = {
            .Count = sample_count,
            .Quality = sample_count > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT
    };
    int create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
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
        &swap_chain_desc,           /* pSwapChainDesc */
        &swap_chain,                /* ppSwapChain */
        &device,                    /* ppDevice */
        &feature_level,             /* pFeatureLevel */
        &device_context);           /* ppImmediateContext */
    assert(SUCCEEDED(hr) && swap_chain && device && device_context);

    /* default render target and depth-stencil-buffer */
    d3d11_create_default_render_target();
}

void d3d11_shutdown() {
    d3d11_destroy_default_render_target();
    SAFE_RELEASE(IDXGISwapChain, swap_chain);
    SAFE_RELEASE(ID3D11DeviceContext, device_context);
    SAFE_RELEASE(ID3D11Device, device);
    DestroyWindow(hwnd); hwnd = 0;
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
}

void d3d11_create_default_render_target() {
    HRESULT hr;
    hr = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void**)&render_target);
    assert(SUCCEEDED(hr) && render_target);
    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)render_target, NULL, &render_target_view);
    assert(SUCCEEDED(hr) && render_target_view);

    D3D11_TEXTURE2D_DESC ds_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = swap_chain_desc.SampleDesc,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    };
    hr = ID3D11Device_CreateTexture2D(device, &ds_desc, NULL, &depth_stencil_buffer);
    assert(SUCCEEDED(hr) && depth_stencil_buffer);

    const int sample_count = swap_chain_desc.SampleDesc.Count;
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
        .Format = ds_desc.Format,
        .ViewDimension = sample_count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D
    };
    hr = ID3D11Device_CreateDepthStencilView(device, (ID3D11Resource*)depth_stencil_buffer, &dsv_desc, &depth_stencil_view);
    assert(SUCCEEDED(hr) && depth_stencil_view);
}

void d3d11_destroy_default_render_target() {
    SAFE_RELEASE(ID3D11Texture2D, render_target);
    SAFE_RELEASE(ID3D11RenderTargetView, render_target_view);
    SAFE_RELEASE(ID3D11Texture2D, depth_stencil_buffer);
    SAFE_RELEASE(ID3D11DepthStencilView, depth_stencil_view);
}

void d3d11_update_default_render_target() {
    if (swap_chain) {
        d3d11_destroy_default_render_target();
        IDXGISwapChain_ResizeBuffers(swap_chain, 1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        d3d11_create_default_render_target();
    }
}

bool d3d11_process_events() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
            quit_requested = true;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    } 
    return !quit_requested;
}

void d3d11_present() {
    IDXGISwapChain_Present(swap_chain, 1, 0);
    /* handle window resizing */
    RECT r;
    if (GetClientRect(hwnd, &r)) {
        const int cur_width = r.right - r.left;
        const int cur_height = r.bottom - r.top;
        if (((cur_width > 0) && (cur_width != width)) ||
            ((cur_height > 0) && (cur_height != height))) 
        {
            /* need to reallocate the default render target */
            width = cur_width;
            height = cur_height;
            d3d11_update_default_render_target();
        }
    }
}

LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            quit_requested = true;
            return 0;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_LBUTTONDOWN:
            if (mouse_btn_down_func) {
                mouse_btn_down_func(0);
            }
            break;
        case WM_RBUTTONDOWN:
            if (mouse_btn_down_func) {
                mouse_btn_down_func(1);
            }
            break;
        case WM_LBUTTONUP:
            if (mouse_btn_up_func) {
                mouse_btn_up_func(0);
            }
            break;
        case WM_RBUTTONUP:
            if (mouse_btn_up_func) {
                mouse_btn_up_func(1);
            }
            break;
        case WM_MOUSEMOVE:
            if (mouse_pos_func) {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                mouse_pos_func((float)x, (float)y);
            }
            break;
        case WM_MOUSEWHEEL:
            if (mouse_wheel_func) {
                mouse_wheel_func((float)((SHORT)HIWORD(wParam) / 30.0f));
            }
            break;
        case WM_CHAR:
            if (char_func) {
                char_func((wchar_t)wParam);
            }
            break;
        case WM_KEYDOWN:
            if (key_down_func) {
                key_down_func((int)wParam);
            }
            break;
        case WM_KEYUP:
            if (key_up_func) {
                key_up_func((int)wParam);
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

const void* d3d11_device() { 
    return (const void*) device;
}

const void* d3d11_device_context() {
    return (const void*) device_context;
}

const void* d3d11_render_target_view() {
    return (const void*) render_target_view;
}

const void* d3d11_depth_stencil_view() {
    return (const void*) depth_stencil_view;
}

int d3d11_width() {
    return width;
}

int d3d11_height() {
    return height;
}

/* register input callbacks */
void d3d11_key_down(d3d11_key_func f) {
    key_down_func = f;
}

void d3d11_key_up(d3d11_key_func f) {
    key_up_func = f;
}

void d3d11_char(d3d11_char_func f) {
    char_func = f;
}

void d3d11_mouse_btn_down(d3d11_mouse_btn_func f) {
    mouse_btn_down_func = f;
}

void d3d11_mouse_btn_up(d3d11_mouse_btn_func f) {
    mouse_btn_up_func = f;
}

void d3d11_mouse_pos(d3d11_mouse_pos_func f) {
    mouse_pos_func = f;
}

void d3d11_mouse_wheel(d3d11_mouse_wheel_func f) {
    mouse_wheel_func = f;
}
