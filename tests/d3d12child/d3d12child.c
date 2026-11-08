/* d3d12child: a D3D12 flip-model swapchain on a child window, cleared to a colour every frame (red, then
 * green after 3 s), like After Effects' composition viewer. Shows whether D3D12 presents into child windows
 * (offscreen client surfaces) reach the screen. With "-wait" the swapchain has a frame latency waitable
 * object that is waited on (1 s timeout) before each frame, as AE does; timeouts are counted. With "-once"
 * it presents a single red frame and then only pumps messages (AE presents a static composition once).
 * Build: x86_64-w64-mingw32-gcc -O2 -o d3d12child.exe d3d12child.c -ld3d12 -ld2d1 -ldxgi -luuid -mwindows */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d2d1.h>
#include <dxgi1_4.h>
#include <stdio.h>

static LRESULT CALLBACK proc( HWND h, UINT m, WPARAM w, LPARAM l )
{
    if (m == WM_DESTROY) PostQuitMessage( 0 );
    return DefWindowProcW( h, m, w, l );
}

static HRESULT create_underlay( HWND hwnd, ID2D1Factory **factory, ID2D1HwndRenderTarget **target )
{
    D2D1_RENDER_TARGET_PROPERTIES props = {0};
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props = {0};
    HRESULT hr;

    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    hwnd_props.hwnd = hwnd;
    hwnd_props.pixelSize.width = 500;
    hwnd_props.pixelSize.height = 350;
    hwnd_props.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;
    hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)factory );
    if (FAILED(hr)) return hr;
    return ID2D1Factory_CreateHwndRenderTarget( *factory, &props, &hwnd_props, target );
}

static HRESULT draw_underlay( ID2D1HwndRenderTarget *target, BOOL released )
{
    D2D1_COLOR_F color = {0, released ? 1.0f : 0.0f, 0, 1};

    ID2D1HwndRenderTarget_BeginDraw( target );
    ID2D1HwndRenderTarget_Clear( target, &color );
    return ID2D1HwndRenderTarget_EndDraw( target, NULL, NULL );
}

int WINAPI WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASSW wc = {0};
    HWND top, child;
    ID3D12Device *dev; ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *list;
    ID3D12DescriptorHeap *rtv_heap; ID3D12Resource *buffers[2]; ID3D12Fence *fence;
    IDXGIFactory4 *factory; IDXGISwapChain1 *sc1; IDXGISwapChain3 *sc;
    D3D12_COMMAND_QUEUE_DESC qd = {0}; D3D12_DESCRIPTOR_HEAP_DESC hd = {0};
    DXGI_SWAP_CHAIN_DESC1 sd = {0}; D3D12_CPU_DESCRIPTOR_HANDLE rtv; UINT rtv_size, i, frame = 0;
    UINT64 fv = 0; HANDLE ev = CreateEventW( NULL, FALSE, FALSE, NULL ); DWORD start = GetTickCount(); MSG msg;
    BOOL once = cmd && strstr( cmd, "-once" ) != NULL;
    BOOL underlay = cmd && strstr( cmd, "-underlay" ) != NULL, released = FALSE;
    ID2D1Factory *d2d_factory = NULL;
    ID2D1HwndRenderTarget *d2d_target = NULL;
    BOOL use_wait = cmd && strstr( cmd, "-wait" ) != NULL; HANDLE latency = NULL; UINT timeouts = 0;
    FILE *log = fopen( "C:\\users\\Public\\d3d12child.log", "w" );

    if (!log) return 1;
    once = once || underlay;

    wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = L"d3d12child";
    wc.hbrBackground = (HBRUSH)GetStockObject( GRAY_BRUSH ); wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    RegisterClassW( &wc );
    top = CreateWindowW( L"d3d12child", L"d3d12child", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, 200, 200, 800, 600, 0, 0, inst, 0 );
    child = CreateWindowW( L"static", L"", WS_CHILD | WS_VISIBLE, 100, 100, 500, 350, top, 0, inst, 0 );

    if (FAILED(D3D12CreateDevice( NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev ))) { fprintf( log, "no device\n" ); return 1; }
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12Device_CreateCommandQueue( dev, &qd, &IID_ID3D12CommandQueue, (void **)&queue );
    CreateDXGIFactory2( 0, &IID_IDXGIFactory4, (void **)&factory );
    sd.Width = 500; sd.Height = 350; sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM; sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (use_wait) sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (FAILED(IDXGIFactory4_CreateSwapChainForHwnd( factory, (IUnknown *)queue, child, &sd, NULL, NULL, &sc1 ))) { fprintf( log, "no swapchain\n" ); return 1; }
    IDXGISwapChain1_QueryInterface( sc1, &IID_IDXGISwapChain3, (void **)&sc );
    if (use_wait) latency = IDXGISwapChain3_GetFrameLatencyWaitableObject( sc );
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 2;
    ID3D12Device_CreateDescriptorHeap( dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&rtv_heap );
    rtv_size = ID3D12Device_GetDescriptorHandleIncrementSize( dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
    for (i = 0; i < 2; i++)
    {
        rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart( rtv_heap, &rtv ); rtv.ptr += i * rtv_size;
        IDXGISwapChain3_GetBuffer( sc, i, &IID_ID3D12Resource, (void **)&buffers[i] );
        ID3D12Device_CreateRenderTargetView( dev, buffers[i], NULL, rtv );
    }
    ID3D12Device_CreateCommandAllocator( dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&alloc );
    ID3D12Device_CreateCommandList( dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, NULL, &IID_ID3D12GraphicsCommandList, (void **)&list );
    ID3D12GraphicsCommandList_Close( list );
    ID3D12Device_CreateFence( dev, 0, 0, &IID_ID3D12Fence, (void **)&fence );
    fprintf( log, "running\n" ); fflush( log );

    for (;;)
    {
        D3D12_RESOURCE_BARRIER b = {0}; float red[4] = {1, 0, 0, 1}, green[4] = {0, 1, 0, 1};
        UINT idx;
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) { if (msg.message == WM_QUIT) return 0; DispatchMessageW( &msg ); }
        if (GetTickCount() - start > (underlay ? 25000 : 10000)) break;
        if (once && frame)
        {
            if (underlay)
            {
                HRESULT hr;

                if (!d2d_target)
                {
                    hr = create_underlay( child, &d2d_factory, &d2d_target );
                    fprintf( log, "D2D underlay creation %#lx; D3D12 red should remain visible until release\n", (unsigned long)hr );
                    fflush( log );
                    if (FAILED(hr)) return 1;
                }
                if (!released && GetTickCount() - start >= 15000)
                {
                    for (i = 0; i < 2; i++) ID3D12Resource_Release( buffers[i] );
                    IDXGISwapChain3_Release( sc );
                    IDXGISwapChain1_Release( sc1 );
                    released = TRUE;
                    fprintf( log, "D3D12 swapchain released; D2D green should become visible\n" );
                    fflush( log );
                }
                hr = draw_underlay( d2d_target, released );
                if (FAILED(hr)) { fprintf( log, "D2D EndDraw %#lx\n", (unsigned long)hr ); return 1; }
            }
            Sleep( underlay ? 100 : 10 );
            continue;
        }
        idx = IDXGISwapChain3_GetCurrentBackBufferIndex( sc );
        if (latency && WaitForSingleObject( latency, 1000 ) == WAIT_TIMEOUT) timeouts++;
        ID3D12CommandAllocator_Reset( alloc ); ID3D12GraphicsCommandList_Reset( list, alloc, NULL );
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = buffers[idx];
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        ID3D12GraphicsCommandList_ResourceBarrier( list, 1, &b );
        rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart( rtv_heap, &rtv ); rtv.ptr += idx * rtv_size;
        ID3D12GraphicsCommandList_ClearRenderTargetView( list, rtv, underlay || GetTickCount() - start < 3000 ? red : green, 0, NULL );
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        ID3D12GraphicsCommandList_ResourceBarrier( list, 1, &b );
        ID3D12GraphicsCommandList_Close( list );
        ID3D12CommandQueue_ExecuteCommandLists( queue, 1, (ID3D12CommandList **)&list );
        IDXGISwapChain3_Present( sc, 1, 0 );
        ID3D12CommandQueue_Signal( queue, fence, ++fv );
        ID3D12Fence_SetEventOnCompletion( fence, fv, ev ); WaitForSingleObject( ev, INFINITE );
        frame++;
        if (underlay) start = GetTickCount();
    }
    if (d2d_target) ID2D1HwndRenderTarget_Release( d2d_target );
    if (d2d_factory) ID2D1Factory_Release( d2d_factory );
    fprintf( log, "frames %u latency timeouts %u\n", frame, timeouts ); fclose( log );
    return 0;
}
