/* relpaintdc.c: a window releases, with ReleaseDC(itself, hdc), a DC that another window got from BeginPaint.
 * Lightroom Classic does this: its "Edit Photo with Adobe Photoshop" dialog subclasses a group box whose
 * WM_ERASEBKGND handler calls ReleaseDC(groupbox, hdc) on the DC that a child radio button passes through
 * DrawThemeParentBackground. On Windows the radios draw; on Wine the release disabled the radio's paint DC and the
 * radios stayed blank. Checked: the child's paint DC still works after the parent's ReleaseDC (clip box, FillRect,
 * pixel read back). Also printed, not checked: what ReleaseDC returns for (b) a paint DC with its own window,
 * (c) a GetDC DC with another window.
 * Build: x86_64-w64-mingw32-gcc -O2 -o relpaintdc.exe relpaintdc.c -luser32 -lgdi32 */
#include <windows.h>
#include <stdio.h>

static HWND parent_wnd, child_wnd;
static int fails, release_ret = -1, clip_after = -1, fill_ok = -1;

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND && wp && GetProp(hwnd, "probe"))
    {
        release_ret = ReleaseDC(hwnd, (HDC)wp);     /* what Lightroom's group box does */
        return 1;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK child_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps; RECT r, cb; HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
        HDC hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &r);
        SetProp(parent_wnd, "probe", (HANDLE)1);
        SendMessageA(parent_wnd, WM_ERASEBKGND, (WPARAM)hdc, 0);   /* as DrawThemeParentBackground does */
        RemoveProp(parent_wnd, "probe");
        clip_after = GetClipBox(hdc, &cb);
        fill_ok = FillRect(hdc, &r, red);
        EndPaint(hwnd, &ps);
        DeleteObject(red);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(void)
{
    WNDCLASSA wc = {0};
    HDC hdc, sdc;
    COLORREF px;
    MSG m;
    int i, r;

    wc.hInstance = GetModuleHandleA(NULL); wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpfnWndProc = parent_proc; wc.lpszClassName = "relpaintdc_parent"; RegisterClassA(&wc);
    wc.lpfnWndProc = child_proc; wc.lpszClassName = "relpaintdc_child"; RegisterClassA(&wc);
    parent_wnd = CreateWindowA("relpaintdc_parent", "relpaintdc", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
                               100, 100, 300, 200, 0, 0, 0, 0);
    child_wnd = CreateWindowA("relpaintdc_child", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 20, 20, 120, 40,
                              parent_wnd, 0, 0, 0);
    for (i = 0; i < 50; i++) { while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&m); Sleep(20); }
    RedrawWindow(child_wnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    for (i = 0; i < 25; i++) { while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&m); Sleep(20); }

    sdc = GetDC(child_wnd); px = GetPixel(sdc, 20, 10); ReleaseDC(child_wnd, sdc);
    printf("(a) parent ReleaseDC(parent, child's paint DC) returned %d\n", release_ret);
    printf("    after it: clip box type %d (0 = DC no longer usable), FillRect %d, child pixel %06lx\n", clip_after, fill_ok,
           (unsigned long)px);
    if (clip_after == 0) { printf("FAIL: the child's paint DC was released by another window's ReleaseDC\n"); fails++; }
    if (!fill_ok) { printf("FAIL: FillRect into the child's paint DC failed\n"); fails++; }
    if (px != RGB(255, 0, 0)) { printf("FAIL: the child is not red\n"); fails++; }

    /* (b) a paint DC released by its own window, before EndPaint, then (c) a GetDC DC released with another window */
    {
        PAINTSTRUCT ps; RECT cb;
        InvalidateRect(parent_wnd, NULL, FALSE);
        hdc = BeginPaint(parent_wnd, &ps);
        r = ReleaseDC(parent_wnd, hdc);
        printf("(b) ReleaseDC(own window, own paint DC) returned %d, clip box after %d\n", r, GetClipBox(hdc, &cb));
        EndPaint(parent_wnd, &ps);
        hdc = GetDC(child_wnd);
        r = ReleaseDC(parent_wnd, hdc);
        printf("(c) ReleaseDC(other window, GetDC DC) returned %d, clip box after %d\n", r, GetClipBox(hdc, &cb));
        ReleaseDC(child_wnd, hdc);
    }
    DestroyWindow(parent_wnd);
    printf("fails %d\n", fails);
    return fails;
}
