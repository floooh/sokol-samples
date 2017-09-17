#include "d3d11entry.h"

static LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static bool quit_requested = false;
static bool in_create_window = false;
static HWND hwnd = NULL;
static DWORD win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX; 
static DWORD win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

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
}

void d3d11_shutdown() {
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
