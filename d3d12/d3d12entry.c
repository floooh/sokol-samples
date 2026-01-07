#include "d3d12entry.h"
#pragma warning(disable:4201)   // needed for /W4 and including d3d12.h
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d12.h>
#include <dxgi1_4.h>

static LRESULT CALLBACK d3d12_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void d3d12_create_default_render_targets(void);
static void d3d12_destroy_default_render_targets(void);
static void d3d12_update_default_render_targets(void);
static void d3d12_wait_for_gpu(void);

// IID constants for C compilation
static const IID _d3d12entry_IID_IDXGIFactory4 = { 0x1bc6ea02,0xef36,0x464f, {0xbf,0x0c,0x21,0xca,0x39,0xe5,0x16,0x8a} };
static const IID _d3d12entry_IID_IDXGISwapChain3 = { 0x94d99bdb,0xf1f8,0x4ab0, {0xb2,0x36,0x7d,0xa0,0x17,0x0e,0xda,0xb1} };
static const IID _d3d12entry_IID_ID3D12Debug = { 0x344488b7,0x6846,0x474b, {0xb9,0x89,0xf0,0x27,0x44,0x82,0x45,0xe0} };
static const IID _d3d12entry_IID_ID3D12Device = { 0x189819f1,0x1db6,0x4b57, {0xbe,0x54,0x18,0x21,0x33,0x9b,0x85,0xf7} };
static const IID _d3d12entry_IID_ID3D12CommandQueue = { 0x0ec870a6,0x5d7e,0x4c22, {0x8c,0xfc,0x5b,0xaa,0xe0,0x76,0x16,0xed} };
static const IID _d3d12entry_IID_ID3D12DescriptorHeap = { 0x8efb471d,0x616c,0x4f49, {0x90,0xf7,0x12,0x7b,0xb7,0x63,0xfa,0x51} };
static const IID _d3d12entry_IID_ID3D12CommandAllocator = { 0x6102dee4,0xaf59,0x4b09, {0xb9,0x99,0xb4,0x4d,0x73,0xf0,0x9b,0x24} };
static const IID _d3d12entry_IID_ID3D12GraphicsCommandList = { 0x5b160d0f,0xac1b,0x4185, {0x8b,0xa8,0xb3,0xae,0x42,0xa5,0xa4,0x55} };
static const IID _d3d12entry_IID_ID3D12Fence = { 0x0a753dcf,0xc4d8,0x4b91, {0xad,0xf6,0xbe,0x5a,0x60,0xd9,0x5a,0x76} };
static const IID _d3d12entry_IID_ID3D12Resource = { 0x696442be,0xa72e,0x4059, {0xbc,0x79,0x5b,0x5c,0x98,0x04,0x0f,0xad} };

// Helper macro for C vs C++ IID reference
#if defined(__cplusplus)
    #define _d3d12entry_refiid(iid) (iid)
#else
    #define _d3d12entry_refiid(iid) (&iid)
#endif

#define _d3d12_def(val, def) (((val) == 0) ? (def) : (val))
#define NUM_FRAMES 2

static struct {
    bool quit_requested;
    bool in_create_window;
    HWND hwnd;
    DWORD win_style;
    DWORD win_ex_style;
    int width;
    int height;
    int sample_count;
    bool no_depth_buffer;
    // D3D12 objects
    IDXGIFactory4* factory;
    ID3D12Device* device;
    ID3D12CommandQueue* cmd_queue;
    IDXGISwapChain3* swap_chain;
    ID3D12DescriptorHeap* rtv_heap;
    ID3D12DescriptorHeap* dsv_heap;
    UINT rtv_descriptor_size;
    ID3D12Resource* render_targets[NUM_FRAMES];
    ID3D12Resource* msaa_render_target;
    ID3D12Resource* depth_stencil;
    ID3D12CommandAllocator* cmd_allocators[NUM_FRAMES];
    ID3D12GraphicsCommandList* cmd_list;
    ID3D12Fence* fence;
    UINT64 fence_value;
    HANDLE fence_event;
    UINT frame_index;
    // input callbacks
    d3d12_key_func key_down_func;
    d3d12_key_func key_up_func;
    d3d12_char_func char_func;
    d3d12_mouse_btn_func mouse_btn_down_func;
    d3d12_mouse_btn_func mouse_btn_up_func;
    d3d12_mouse_pos_func mouse_pos_func;
    d3d12_mouse_wheel_func mouse_wheel_func;
} state = {
    .win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
    .win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE
};

#define SAFE_RELEASE(class, obj) if (obj) { class##_Release(obj); obj=0; }

void d3d12_init(const d3d12_desc_t* desc) {
    assert(desc);
    assert(desc->width > 0);
    assert(desc->height > 0);
    assert(desc->title);

    d3d12_desc_t desc_def = *desc;
    desc_def.sample_count = _d3d12_def(desc_def.sample_count, 1);

    state.width = desc_def.width;
    state.height = desc_def.height;
    state.sample_count = desc_def.sample_count;
    state.no_depth_buffer = desc_def.no_depth_buffer;

    // enable debug layer in debug builds
    #ifdef _DEBUG
    {
        ID3D12Debug* debug_controller = NULL;
        if (SUCCEEDED(D3D12GetDebugInterface(_d3d12entry_refiid(_d3d12entry_IID_ID3D12Debug), (void**)&debug_controller))) {
            ID3D12Debug_EnableDebugLayer(debug_controller);
            SAFE_RELEASE(ID3D12Debug, debug_controller);
        }
    }
    #endif

    // create DXGI factory
    HRESULT hr = CreateDXGIFactory1(_d3d12entry_refiid(_d3d12entry_IID_IDXGIFactory4), (void**)&state.factory);
    assert(SUCCEEDED(hr) && state.factory);

    // create D3D12 device
    hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, _d3d12entry_refiid(_d3d12entry_IID_ID3D12Device), (void**)&state.device);
    assert(SUCCEEDED(hr) && state.device);

    // create command queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
    };
    hr = ID3D12Device_CreateCommandQueue(state.device, &queue_desc, _d3d12entry_refiid(_d3d12entry_IID_ID3D12CommandQueue), (void**)&state.cmd_queue);
    assert(SUCCEEDED(hr) && state.cmd_queue);

    // register window class
    RegisterClassW(&(WNDCLASSW){
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = (WNDPROC) d3d12_winproc,
        .hInstance = GetModuleHandleW(NULL),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hIcon = LoadIcon(NULL, IDI_WINLOGO),
        .lpszClassName = L"SOKOLD3D12"
    });

    // create window
    state.in_create_window = true;
    RECT rect = { .left = 0, .top = 0, .right = state.width, .bottom = state.height };
    AdjustWindowRectEx(&rect, state.win_style, FALSE, state.win_ex_style);
    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;
    state.hwnd = CreateWindowExW(
        state.win_ex_style,
        L"SOKOLD3D12",
        desc_def.title,
        state.win_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        win_width,
        win_height,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL);
    ShowWindow(state.hwnd, SW_SHOW);
    state.in_create_window = false;

    // create swap chain
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {
        .Width = (UINT)state.width,
        .Height = (UINT)state.height,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = NUM_FRAMES,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = 0
    };
    IDXGISwapChain1* swap_chain1 = NULL;
    hr = IDXGIFactory4_CreateSwapChainForHwnd(state.factory, (IUnknown*)state.cmd_queue, state.hwnd, &swap_chain_desc, NULL, NULL, &swap_chain1);
    assert(SUCCEEDED(hr) && swap_chain1);
    hr = IDXGISwapChain1_QueryInterface(swap_chain1, _d3d12entry_refiid(_d3d12entry_IID_IDXGISwapChain3), (void**)&state.swap_chain);
    SAFE_RELEASE(IDXGISwapChain1, swap_chain1);
    assert(SUCCEEDED(hr) && state.swap_chain);

    // disable Alt+Enter fullscreen
    IDXGIFactory4_MakeWindowAssociation(state.factory, state.hwnd, DXGI_MWA_NO_ALT_ENTER);

    state.frame_index = IDXGISwapChain3_GetCurrentBackBufferIndex(state.swap_chain);

    // create RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = NUM_FRAMES + 1,  // +1 for MSAA render target
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    };
    hr = ID3D12Device_CreateDescriptorHeap(state.device, &rtv_heap_desc, _d3d12entry_refiid(_d3d12entry_IID_ID3D12DescriptorHeap), (void**)&state.rtv_heap);
    assert(SUCCEEDED(hr) && state.rtv_heap);
    state.rtv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(state.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // create DSV descriptor heap
    if (!state.no_depth_buffer) {
        D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };
        hr = ID3D12Device_CreateDescriptorHeap(state.device, &dsv_heap_desc, _d3d12entry_refiid(_d3d12entry_IID_ID3D12DescriptorHeap), (void**)&state.dsv_heap);
        assert(SUCCEEDED(hr) && state.dsv_heap);
    }

    // create command allocators for each frame
    for (UINT i = 0; i < NUM_FRAMES; i++) {
        hr = ID3D12Device_CreateCommandAllocator(state.device, D3D12_COMMAND_LIST_TYPE_DIRECT, _d3d12entry_refiid(_d3d12entry_IID_ID3D12CommandAllocator), (void**)&state.cmd_allocators[i]);
        assert(SUCCEEDED(hr) && state.cmd_allocators[i]);
    }

    // create command list
    hr = ID3D12Device_CreateCommandList(state.device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, state.cmd_allocators[state.frame_index], NULL, _d3d12entry_refiid(_d3d12entry_IID_ID3D12GraphicsCommandList), (void**)&state.cmd_list);
    assert(SUCCEEDED(hr) && state.cmd_list);
    ID3D12GraphicsCommandList_Close(state.cmd_list);

    // create fence
    hr = ID3D12Device_CreateFence(state.device, 0, D3D12_FENCE_FLAG_NONE, _d3d12entry_refiid(_d3d12entry_IID_ID3D12Fence), (void**)&state.fence);
    assert(SUCCEEDED(hr) && state.fence);
    state.fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    assert(state.fence_event);

    // create render targets
    d3d12_create_default_render_targets();
}

void d3d12_shutdown(void) {
    d3d12_wait_for_gpu();
    d3d12_destroy_default_render_targets();
    if (state.fence_event) { CloseHandle(state.fence_event); state.fence_event = NULL; }
    SAFE_RELEASE(ID3D12Fence, state.fence);
    SAFE_RELEASE(ID3D12GraphicsCommandList, state.cmd_list);
    for (UINT i = 0; i < NUM_FRAMES; i++) {
        SAFE_RELEASE(ID3D12CommandAllocator, state.cmd_allocators[i]);
    }
    SAFE_RELEASE(ID3D12DescriptorHeap, state.dsv_heap);
    SAFE_RELEASE(ID3D12DescriptorHeap, state.rtv_heap);
    SAFE_RELEASE(IDXGISwapChain3, state.swap_chain);
    SAFE_RELEASE(ID3D12CommandQueue, state.cmd_queue);
    SAFE_RELEASE(ID3D12Device, state.device);
    SAFE_RELEASE(IDXGIFactory4, state.factory);
    DestroyWindow(state.hwnd); state.hwnd = 0;
    UnregisterClassW(L"SOKOLD3D12", GetModuleHandleW(NULL));
}

void d3d12_create_default_render_targets(void) {
    HRESULT hr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(state.rtv_heap, &rtv_handle);

    // create RTVs for swap chain buffers
    for (UINT i = 0; i < NUM_FRAMES; i++) {
        hr = IDXGISwapChain3_GetBuffer(state.swap_chain, i, _d3d12entry_refiid(_d3d12entry_IID_ID3D12Resource), (void**)&state.render_targets[i]);
        assert(SUCCEEDED(hr) && state.render_targets[i]);
        ID3D12Device_CreateRenderTargetView(state.device, state.render_targets[i], NULL, rtv_handle);
        rtv_handle.ptr += state.rtv_descriptor_size;
    }

    // create MSAA render target if needed
    if (state.sample_count > 1) {
        D3D12_RESOURCE_DESC msaa_desc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = (UINT64)state.width,
            .Height = (UINT)state.height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .SampleDesc = { .Count = (UINT)state.sample_count, .Quality = 0 },
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        };
        D3D12_HEAP_PROPERTIES heap_props = { .Type = D3D12_HEAP_TYPE_DEFAULT };
        D3D12_CLEAR_VALUE clear_value = { .Format = DXGI_FORMAT_B8G8R8A8_UNORM };
        hr = ID3D12Device_CreateCommittedResource(state.device, &heap_props, D3D12_HEAP_FLAG_NONE, &msaa_desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clear_value, _d3d12entry_refiid(_d3d12entry_IID_ID3D12Resource), (void**)&state.msaa_render_target);
        assert(SUCCEEDED(hr) && state.msaa_render_target);
        ID3D12Device_CreateRenderTargetView(state.device, state.msaa_render_target, NULL, rtv_handle);
    }

    // create depth stencil
    if (!state.no_depth_buffer) {
        D3D12_RESOURCE_DESC ds_desc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = (UINT64)state.width,
            .Height = (UINT)state.height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
            .SampleDesc = { .Count = (UINT)state.sample_count, .Quality = 0 },
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        };
        D3D12_HEAP_PROPERTIES heap_props = { .Type = D3D12_HEAP_TYPE_DEFAULT };
        D3D12_CLEAR_VALUE clear_value = { .Format = DXGI_FORMAT_D24_UNORM_S8_UINT, .DepthStencil = { .Depth = 1.0f, .Stencil = 0 } };
        hr = ID3D12Device_CreateCommittedResource(state.device, &heap_props, D3D12_HEAP_FLAG_NONE, &ds_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, _d3d12entry_refiid(_d3d12entry_IID_ID3D12Resource), (void**)&state.depth_stencil);
        assert(SUCCEEDED(hr) && state.depth_stencil);
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle;
        ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(state.dsv_heap, &dsv_handle);
        ID3D12Device_CreateDepthStencilView(state.device, state.depth_stencil, NULL, dsv_handle);
    }
}

void d3d12_destroy_default_render_targets(void) {
    SAFE_RELEASE(ID3D12Resource, state.depth_stencil);
    SAFE_RELEASE(ID3D12Resource, state.msaa_render_target);
    for (UINT i = 0; i < NUM_FRAMES; i++) {
        SAFE_RELEASE(ID3D12Resource, state.render_targets[i]);
    }
}

void d3d12_update_default_render_targets(void) {
    if (state.swap_chain) {
        d3d12_wait_for_gpu();
        d3d12_destroy_default_render_targets();
        HRESULT hr = IDXGISwapChain3_ResizeBuffers(state.swap_chain, NUM_FRAMES, state.width, state.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        assert(SUCCEEDED(hr));
        state.frame_index = IDXGISwapChain3_GetCurrentBackBufferIndex(state.swap_chain);
        d3d12_create_default_render_targets();
    }
}

void d3d12_wait_for_gpu(void) {
    if (state.cmd_queue && state.fence && state.fence_event) {
        if (ID3D12Fence_GetCompletedValue(state.fence) < state.fence_value) {
            ID3D12Fence_SetEventOnCompletion(state.fence, state.fence_value, state.fence_event);
            WaitForSingleObject(state.fence_event, INFINITE);
        }
    }
}

bool d3d12_process_events(void) {
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

void d3d12_present(void) {
    IDXGISwapChain3_Present(state.swap_chain, 1, 0);
    state.frame_index = IDXGISwapChain3_GetCurrentBackBufferIndex(state.swap_chain);

    // handle window resizing
    RECT r;
    if (GetClientRect(state.hwnd, &r)) {
        const int cur_width = r.right - r.left;
        const int cur_height = r.bottom - r.top;
        if (((cur_width > 0) && (cur_width != state.width)) ||
            ((cur_height > 0) && (cur_height != state.height)))
        {
            state.width = cur_width;
            state.height = cur_height;
            d3d12_update_default_render_targets();
        }
    }
}

LRESULT CALLBACK d3d12_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

sg_environment d3d12_environment(void) {
    return (sg_environment){
        .defaults = {
            .color_format = SG_PIXELFORMAT_BGRA8,
            .depth_format = state.no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = state.sample_count,
        },
        .d3d12 = {
            .device = (const void*)state.device,
            .command_queue = (const void*)state.cmd_queue,
            .fence = (const void*)state.fence,
            .fence_event = (void*)state.fence_event,
            .fence_value = &state.fence_value,
        }
    };
}

sg_swapchain d3d12_swapchain(void) {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(state.rtv_heap, &rtv_handle);

    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = { 0 };
    if (state.dsv_heap) {
        ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(state.dsv_heap, &dsv_handle);
    }

    // for MSAA, use the MSAA render target (at index NUM_FRAMES in the heap)
    D3D12_CPU_DESCRIPTOR_HANDLE render_view = rtv_handle;
    D3D12_CPU_DESCRIPTOR_HANDLE resolve_view = { 0 };
    if (state.sample_count > 1) {
        render_view.ptr = rtv_handle.ptr + NUM_FRAMES * state.rtv_descriptor_size;
        resolve_view.ptr = rtv_handle.ptr + state.frame_index * state.rtv_descriptor_size;
    } else {
        render_view.ptr = rtv_handle.ptr + state.frame_index * state.rtv_descriptor_size;
    }

    return (sg_swapchain){
        .width = state.width,
        .height = state.height,
        .sample_count = state.sample_count,
        .color_format = SG_PIXELFORMAT_BGRA8,
        .depth_format = state.no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
        .d3d12 = {
            .render_view = (const void*)render_view.ptr,
            .resolve_view = (const void*)resolve_view.ptr,
            .depth_stencil_view = (const void*)dsv_handle.ptr,
            .render_target = (state.sample_count > 1) ? (const void*)state.msaa_render_target : (const void*)state.render_targets[state.frame_index],
            .resolve_target = (state.sample_count > 1) ? (const void*)state.render_targets[state.frame_index] : 0,
            .depth_stencil = (const void*)state.depth_stencil,
        }
    };
}

int d3d12_width(void) {
    return state.width;
}

int d3d12_height(void) {
    return state.height;
}

const void* d3d12_get_device(void) {
    return (const void*)state.device;
}

// register input callbacks
void d3d12_key_down(d3d12_key_func f) {
    state.key_down_func = f;
}

void d3d12_key_up(d3d12_key_func f) {
    state.key_up_func = f;
}

void d3d12_char(d3d12_char_func f) {
    state.char_func = f;
}

void d3d12_mouse_btn_down(d3d12_mouse_btn_func f) {
    state.mouse_btn_down_func = f;
}

void d3d12_mouse_btn_up(d3d12_mouse_btn_func f) {
    state.mouse_btn_up_func = f;
}

void d3d12_mouse_pos(d3d12_mouse_pos_func f) {
    state.mouse_pos_func = f;
}

void d3d12_mouse_wheel(d3d12_mouse_wheel_func f) {
    state.mouse_wheel_func = f;
}
