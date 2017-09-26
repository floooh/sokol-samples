#include "d3d11entry.h"

static LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static bool quit_requested = false;
static bool in_create_window = false;
static HWND hwnd = NULL;
static DWORD win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX; 
static DWORD win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
static ID3D11Device* d3d11_device = 0;
static ID3D11DeviceContext* d3d11_device_context = 0;
static IDXGISwapChain* dxgi_swap_chain = 0;

void d3d11_init(int w, int h, int sample_count, const wchar_t* title) {

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
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {
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
        &dxgi_swap_chain,           /* ppSwapChain */
        &d3d11_device,              /* ppDevice */
        &feature_level,             /* pFeatureLevel */
        &d3d11_device_context);     /* ppImmediateContext */
}

void d3d11_shutdown() {
    if (dxgi_swap_chain) {
        IDXGISwapChain_Release(dxgi_swap_chain);
        dxgi_swap_chain = 0;
    }
    if (d3d11_device_context) {
        ID3D11DeviceContext_Release(d3d11_device_context);
        d3d11_device_context = 0;
    }
    if (d3d11_device) {
        ID3D11Device_Release(d3d11_device);
        d3d11_device = 0;
    }
    DestroyWindow(hwnd);
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
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
    /* FIXME */
}

LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            quit_requested = true;
            return 0;

        case WM_SIZE:
            if (!in_create_window) {
                // FIXME
            }
            return 0;

        case WM_ERASEBKGND:
            return TRUE;

        default:
            break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
