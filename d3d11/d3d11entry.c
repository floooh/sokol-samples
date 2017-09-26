#include "d3d11entry.h"

static LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void d3d11_create_default_render_target();
static void d3d11_destroy_default_render_target();
static void d3d11_update_default_render_target();

static bool quit_requested = false;
static bool in_create_window = false;
static HWND hwnd = NULL;
static DWORD win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX; 
static DWORD win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
static DXGI_SWAP_CHAIN_DESC dxgi_swap_chain_desc = { 0 };
static int width = 0;
static int height = 0;
static ID3D11Device* d3d11_device = 0;
static ID3D11DeviceContext* d3d11_device_context = 0;
static IDXGISwapChain* dxgi_swap_chain = 0;
static ID3D11Texture2D* d3d11_render_target = 0;
static ID3D11RenderTargetView* d3d11_render_target_view = 0;
static ID3D11Texture2D* d3d11_depth_stencil_buffer = 0;
static ID3D11DepthStencilView* d3d11_depth_stencil_view = 0;

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
        .hIcon = LoadIconW(NULL, IDI_WINLOGO),
        .lpszClassName = L"SOKOLD3D11"
    });

    /* create window */
    in_create_window = true;
    RECT rect = { .left = 0, .top = 0, .right = w, .bottom = h };
    AdjustWindowRectEx(&rect, win_style, FALSE, win_ex_style);
    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;
    hwnd = CreateWindowEx(
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
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    in_create_window = false;

    /* create device and swap chain */
    dxgi_swap_chain_desc = (DXGI_SWAP_CHAIN_DESC) {
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
        &dxgi_swap_chain_desc,      /* pSwapChainDesc */
        &dxgi_swap_chain,           /* ppSwapChain */
        &d3d11_device,              /* ppDevice */
        &feature_level,             /* pFeatureLevel */
        &d3d11_device_context);     /* ppImmediateContext */
    assert(SUCCEEDED(hr) && dxgi_swap_chain && d3d11_device && d3d11_device_context);

    /* default render target and depth-stencil-buffer */
    d3d11_create_default_render_target();
}

void d3d11_shutdown() {
    d3d11_destroy_default_render_target();
    SAFE_RELEASE(IDXGISwapChain, dxgi_swap_chain);
    SAFE_RELEASE(ID3D11DeviceContext, d3d11_device_context);
    SAFE_RELEASE(ID3D11Device, d3d11_device);
    DestroyWindow(hwnd); hwnd = 0;
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
}

void d3d11_create_default_render_target() {
    HRESULT hr;
    hr = IDXGISwapChain_GetBuffer(dxgi_swap_chain, 0, &IID_ID3D11Texture2D, (void**)&d3d11_render_target);
    assert(SUCCEEDED(hr) && d3d11_render_target);
    hr = ID3D11Device_CreateRenderTargetView(d3d11_device, (ID3D11Resource*)d3d11_render_target, NULL, &d3d11_render_target_view);
    assert(SUCCEEDED(hr) && d3d11_render_target_view);

    D3D11_TEXTURE2D_DESC ds_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = dxgi_swap_chain_desc.SampleDesc,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    };
    hr = ID3D11Device_CreateTexture2D(d3d11_device, &ds_desc, NULL, &d3d11_depth_stencil_buffer);
    assert(SUCCEEDED(hr) && d3d11_depth_stencil_buffer);

    const int sample_count = dxgi_swap_chain_desc.SampleDesc.Count;
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
        .Format = ds_desc.Format,
        .ViewDimension = sample_count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D
    };
    hr = ID3D11Device_CreateDepthStencilView(d3d11_device, (ID3D11Resource*)d3d11_depth_stencil_buffer, &dsv_desc, &d3d11_depth_stencil_view);
    assert(SUCCEEDED(hr) && d3d11_depth_stencil_view);
}

void d3d11_destroy_default_render_target() {
    SAFE_RELEASE(ID3D11Texture2D, d3d11_render_target);
    SAFE_RELEASE(ID3D11RenderTargetView, d3d11_render_target_view);
    SAFE_RELEASE(ID3D11Texture2D, d3d11_depth_stencil_buffer);
    SAFE_RELEASE(ID3D11DepthStencilView, d3d11_depth_stencil_view);
}

void d3d11_update_default_render_target() {
    if (dxgi_swap_chain) {
        d3d11_destroy_default_render_target();
        IDXGISwapChain_ResizeBuffers(dxgi_swap_chain, 1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        d3d11_create_default_render_target();
    }
}

bool d3d11_process_events() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
            quit_requested = true;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } 
    return !quit_requested;
}

void d3d11_present() {
    IDXGISwapChain_Present(dxgi_swap_chain, 1, 0);
}

LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            quit_requested = true;
            return 0;

        case WM_SIZE:
            if (!in_create_window) {
                width = LOWORD(lParam);
                height = HIWORD(lParam);
                d3d11_update_default_render_target();
            }
            return 0;

        case WM_ERASEBKGND:
            return TRUE;

        default:
            break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
