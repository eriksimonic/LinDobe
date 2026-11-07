/* swapchurn.c: Audition's long-session hang. In one session it created ~230 D3D swapchains for a 451x410 window (a file
 * dialog part), DXVK kept up to 82 presenter threads alive, and Vulkan then refused a swapchain (DEVICE_LOST /
 * INITIALIZATION_FAILED; the UI thread then waited forever in Map). This repeats: create a 451x410 window, a flip-model
 * swapchain for it on one shared D3D11 device, present a few frames, release the swapchain (optionally keep it:
 * KEEP=1; KEEP=2 also keeps the window; LATE=1 destroys the window before the release; DELAY=ms waits before the destroy), destroy the window. Prints failures and the process's live thread count every 25 rounds.
 * Build: x86_64-w64-mingw32-gcc -O2 -o swapchurn.exe swapchurn.c -ld3d11 -ldxgi -luser32 -lgdi32 -ldxguid
 * Usage: swapchurn.exe [rounds, default 300] */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

static int threads(void)
{
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); THREADENTRY32 t = {sizeof(t)}; int n = 0;
    DWORD pid = GetCurrentProcessId();
    if (Thread32First(s, &t)) do { if (t.th32OwnerProcessID == pid) n++; } while (Thread32Next(s, &t));
    CloseHandle(s);
    return n;
}

int main(int argc, char **argv)
{
    int rounds = argc > 1 ? atoi(argv[1]) : 300, fails = 0, keep = getenv("KEEP") ? atoi(getenv("KEEP")) : 0,
        late = getenv("LATE") && atoi(getenv("LATE"));
    ID3D11Device *dev; ID3D11DeviceContext *ctx; IDXGIDevice *dxdev; IDXGIAdapter *ad; IDXGIFactory2 *fac;
    IDXGISwapChain1 **kept = calloc(rounds, sizeof(*kept));
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                   D3D11_SDK_VERSION, &dev, NULL, &ctx);
    if (FAILED(hr)) { printf("device: %#lx\n", hr); return 1; }
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&dxdev);
    IDXGIDevice_GetAdapter(dxdev, &ad);
    IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void **)&fac);
    printf("start: %d threads\n", threads()); fflush(stdout);
    for (int i = 0; i < rounds; i++)
    {
        HWND w = CreateWindowExA(0, "STATIC", "swapchurn", WS_POPUP | WS_VISIBLE, 40, 40, 451, 410, 0, 0, 0, 0);
        DXGI_SWAP_CHAIN_DESC1 d = {451, 410, DXGI_FORMAT_B8G8R8A8_UNORM, FALSE, {1, 0}, DXGI_USAGE_RENDER_TARGET_OUTPUT, 2,
                                   DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_ALPHA_MODE_IGNORE, 0};
        IDXGISwapChain1 *sc = NULL;
        hr = IDXGIFactory2_CreateSwapChainForHwnd(fac, (IUnknown *)dev, w, &d, NULL, NULL, &sc);
        if (FAILED(hr)) { printf("round %d: CreateSwapChainForHwnd %#lx\n", i, hr); fails++; DestroyWindow(w); continue; }
        for (int f = 0; f < 3; f++)
        {
            ID3D11Texture2D *bb; ID3D11RenderTargetView *rtv; float c[4] = {f / 3.0f, 0.2f, 0.4f, 1};
            IDXGISwapChain1_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&bb);
            ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
            ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, c);
            ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(bb);
            hr = IDXGISwapChain1_Present(sc, 0, 0);
            if (FAILED(hr)) { printf("round %d: Present %#lx\n", i, hr); fails++; break; }
        }
        MSG m; while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&m);
        /* KEEP=1 keeps the swapchain, KEEP=2 keeps the swapchain and its window, LATE=1 destroys the window first and
         * then releases the swapchain */
        if (getenv("DELAY")) { ID3D11DeviceContext_Flush(ctx); Sleep(atoi(getenv("DELAY"))); }   /* DELAY=ms: nothing in flight at the destroy */
        if (late) { DestroyWindow(w); IDXGISwapChain1_Release(sc); }
        else if (keep) { kept[i] = sc; if (keep < 2) DestroyWindow(w); }
        else { IDXGISwapChain1_Release(sc); DestroyWindow(w); }
        if ((i + 1) % 25 == 0) { printf("round %d: %d threads, %d fails\n", i + 1, threads(), fails); fflush(stdout); }
        if (fails > 5) break;
    }
    /* KEEP=2: every kept swapchain presents once more (one the presenter gave up must come back) */
    if (keep == 2)
    {
        int again = 0;
        for (int i = 0; i < rounds; i++)
        {
            ID3D11Texture2D *bb; ID3D11RenderTargetView *rtv; float c[4] = {0.9f, 0.5f, 0.1f, 1};
            if (!kept[i]) continue;
            IDXGISwapChain1_GetBuffer(kept[i], 0, &IID_ID3D11Texture2D, (void **)&bb);
            ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)bb, NULL, &rtv);
            ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, c);
            ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(bb);
            hr = IDXGISwapChain1_Present(kept[i], 0, 0);
            if (FAILED(hr)) { printf("again %d: Present %#lx\n", i, hr); fails++; }
            else again++;
        }
        printf("presented again: %d\n", again);
    }
    printf("fails %d\n", fails);
    return fails;
}
