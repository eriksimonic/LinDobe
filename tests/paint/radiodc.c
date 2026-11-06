/* radiodc.c: which DC does a themed (comctl32 v6) radio button hand to its parent's WM_ERASEBKGND, and does the radio
 * still draw when the parent calls ReleaseDC(parent, that DC)? Lightroom Classic's "Edit Photo with Adobe Photoshop"
 * dialog subclasses the group box that holds its radios and does exactly that release; on Windows the radios draw.
 * Printed per WM_ERASEBKGND: the DC's object type (3 OBJ_DC, 10 OBJ_MEMDC), the window it belongs to (radio, group box
 * or none) and what ReleaseDC returned. Checked: the parent never gets the radio's own paint DC (Windows 11: a memory
 * DC, from a paint buffer; ReleaseDC on it returns 0) and the radio's circle has dark pixels after it painted.
 * Build: x86_64-w64-mingw32-windres radiodc.rc -O coff -o radiodc.res &&
 *        x86_64-w64-mingw32-gcc -O2 -o radiodc.exe radiodc.c radiodc.res -luser32 -lgdi32 -lcomctl32 */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static HWND group, radio;
static WNDPROC group_old;
static int erase_count, release, fails, own_dc;

static LRESULT CALLBACK group_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND && wp)
    {
        HDC hdc = (HDC)wp; HWND owner = WindowFromDC(hdc); int ret = -1;
        if (owner == radio) own_dc++;                   /* Windows: never; a memory DC (type 10) comes instead */
        if (release) ret = ReleaseDC(hwnd, hdc);      /* what Lightroom's group box does */
        if (erase_count++ < 6)
            printf("  erase: dc type %lu, belongs to %s, ReleaseDC(group, dc) %d\n", GetObjectType(hdc),
                   owner == radio ? "the radio" : owner == hwnd ? "the group box" : owner ? "another window" : "no window", ret);
        FillRect(hdc, &(RECT){0, 0, 400, 400}, GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    return CallWindowProcA(group_old, hwnd, msg, wp, lp);
}

static int dark_pixels(HWND hwnd)
{
    RECT r; HDC src = GetDC(hwnd), mem = CreateCompatibleDC(src); HBITMAP bmp; int n = 0;
    GetClientRect(hwnd, &r);
    bmp = CreateCompatibleBitmap(src, 20, r.bottom); SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, 20, r.bottom, src, 0, 0, SRCCOPY);       /* the circle sits at the left */
    for (int y = 0; y < r.bottom; y++) for (int x = 0; x < 20; x++)
    {
        COLORREF c = GetPixel(mem, x, y);
        if (GetRValue(c) + GetGValue(c) + GetBValue(c) < 3 * 140) n++;
    }
    DeleteDC(mem); DeleteObject(bmp); ReleaseDC(hwnd, src);
    return n;
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG m;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        Sleep(10);
    }
}

int main(void)
{
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    HWND top; int pass;
    InitCommonControlsEx(&icc);
    top = CreateWindowExA(WS_EX_TOPMOST, "#32770", "radiodc", WS_POPUP | WS_CAPTION | WS_VISIBLE, 40, 40, 360, 200, 0, 0, 0, 0);
    group = CreateWindowA("BUTTON", "Group", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 10, 10, 330, 140, top, 0, 0, 0);
    radio = CreateWindowA("BUTTON", "Edit a Copy", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 20, 30, 200, 24, group, 0, 0, 0);
    group_old = (WNDPROC)SetWindowLongPtrA(group, GWLP_WNDPROC, (LONG_PTR)group_proc);
    printf("comctl32 v6 (themed): %s\n", GetModuleHandleA("uxtheme.dll") ? "uxtheme loaded" : "no uxtheme");
    for (pass = 0; pass < 2; pass++)
    {
        int n;
        release = pass; erase_count = 0;
        printf("%s\n", pass ? "group box calls ReleaseDC on the DC:" : "group box does not release:");
        InvalidateRect(radio, NULL, TRUE); UpdateWindow(radio); pump(400);
        n = dark_pixels(radio);
        printf("  radio circle: %d dark pixels, %d erase calls\n", n, erase_count);
        if (!erase_count) printf("  (no WM_ERASEBKGND reached the group box)\n");
        if (n < 5) { printf("FAIL: the radio did not draw (%s)\n", pass ? "with release" : "no release"); fails++; }
    }
    if (own_dc) { printf("FAIL: the radio passed its own paint DC to the parent %d times (Windows: a paint buffer)\n", own_dc); fails++; }
    DestroyWindow(top);
    printf("fails %d\n", fails);
    return fails;
}
