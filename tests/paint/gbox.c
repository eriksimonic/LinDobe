/* gbox.c: a themed group box must not paint its interior black (patch 0137, PR #36; 0131 left it
 * unwritten in the paint buffer). Build like radiodc.c, with radiodc.rc for the comctl32 v6 manifest:
 *   x86_64-w64-mingw32-windres radiodc.rc -O coff -o gbox.res
 *   x86_64-w64-mingw32-gcc -O1 -o gbox.exe gbox.c gbox.res -luser32 -lgdi32 -lcomctl32 -luxtheme
 * Prints "fails 0" when the interior shows the dialog colour. Before 0137: fails 3. */
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
/* A themed group box must not paint its interior black (PR #36). */
int main(void)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    WNDCLASSA wc = {0}; HWND top, gb; HDC dc; COLORREF c, want; int fails = 0, i; MSG msg;
    InitCommonControlsEx(&icc);
    wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.lpszClassName = "gboxtop";
    RegisterClassA(&wc);
    top = CreateWindowA("gboxtop", "gbox", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 400, 300, 0, 0, 0, 0);
    gb = CreateWindowA("BUTTON", "Group", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 20, 300, 200, top, 0, 0, 0);
    for (i = 0; i < 20; i++) { while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg); Sleep(50); }
    RedrawWindow(top, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    for (i = 0; i < 10; i++) { while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg); Sleep(50); }
    dc = GetDC(top); want = GetSysColor(COLOR_BTNFACE);
    for (i = 0; i < 3; i++) {
        int x = 80 + i * 70, y = 120;
        c = GetPixel(dc, x, y);
        printf("interior %d,%d: %06lx (face %06lx)\n", x, y, c, want);
        if (c == RGB(0,0,0)) fails++;
    }
    ReleaseDC(top, dc);
    printf("themed: %s\nfails %d\n", GetWindowTheme(gb) ? "yes" : "no", fails);
    return fails;
}
