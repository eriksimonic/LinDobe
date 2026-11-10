/* dragpos.c: during an OLE drag-and-drop from the X desktop (XDND), log the point IDropTarget gets next to
 * GetCursorPos(), for public #39 (After Effects hit-tests GetCursorPos during a drop; XWayland keeps the X pointer
 * where a Wayland-source drag entered, so it read a stale point). Prints one line per DragEnter/DragOver/Drop and,
 * at the drop, "max diff N px" (0..2 expected with the fix, or with an X11 source where the pointer moves).
 *   x86_64-w64-mingw32-gcc -O1 -o dragpos.exe dragpos.c -lole32 -luser32 -lgdi32 -luuid
 * Window at (100,100) 500x400; exits after the first drop or 60 s. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <stdlib.h>

static int maxdiff = 0, events = 0;
static void note(const char *what, POINTL pt)
{
    POINT c; int d;
    GetCursorPos(&c);
    d = abs(c.x - pt.x) + abs(c.y - pt.y); if (d > maxdiff) maxdiff = d; events++;
    printf("%-9s pt %5ld,%5ld  GetCursorPos %5ld,%5ld  diff %d\n", what, pt.x, pt.y, c.x, c.y, d); fflush(stdout);
}
static HRESULT WINAPI qi(IDropTarget *t, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDropTarget)) { *o = t; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI addref(IDropTarget *t) { return 2; }
static ULONG WINAPI release(IDropTarget *t) { return 1; }
static HRESULT WINAPI enter(IDropTarget *t, IDataObject *d, DWORD k, POINTL pt, DWORD *e) { note("DragEnter", pt); *e = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI over(IDropTarget *t, DWORD k, POINTL pt, DWORD *e) { note("DragOver", pt); *e = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI leave(IDropTarget *t) { printf("DragLeave\n"); return S_OK; }
static HRESULT WINAPI drop(IDropTarget *t, IDataObject *d, DWORD k, POINTL pt, DWORD *e)
{
    note("Drop", pt); *e = DROPEFFECT_COPY;
    printf("events %d, max diff %d px\n", events, maxdiff); fflush(stdout);
    PostQuitMessage(0); return S_OK;
}
static IDropTargetVtbl vt = { qi, addref, release, enter, over, leave, drop };
static IDropTarget target = { &vt };

int main(void)
{
    WNDCLASSA wc = {0}; HWND w; MSG m;
    SetProcessDPIAware(); OleInitialize(NULL);
    wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "dragpos"; wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    RegisterClassA(&wc);
    w = CreateWindowA("dragpos", "dragpos", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 500, 400, 0, 0, 0, 0);
    printf("RegisterDragDrop %#lx\n", RegisterDragDrop(w, &target)); fflush(stdout);
    SetTimer(w, 1, 60000, NULL);
    while (GetMessageA(&m, 0, 0, 0)) { if (m.message == WM_TIMER) { printf("timeout, events %d\n", events); break; } DispatchMessageA(&m); }
    RevokeDragDrop(w); OleUninitialize();
    return 0;
}
