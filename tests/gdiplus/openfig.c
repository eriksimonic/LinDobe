/* openfig.c: a path with two open figures (StartFigure, no CloseFigure) filled on a bitmap. Filling closes every
 * figure; gdiplus's span code only added the closing edge for an explicitly closed figure or the path's last one, so
 * the first figure left an odd number of edges on its scanlines and Wine stopped on the assertion
 * "edge[0].y == edge[1].y" (dlls/gdiplus/region.c; Lightroom Classic, Photo > Edit In > Open as Layers in Photoshop).
 * Checked: both triangles are filled (a pixel inside each), and the areas outside stay empty.
 * Build: x86_64-w64-mingw32-gcc -O2 -o openfig.exe openfig.c -lgdiplus */
#include <windows.h>
#include <stdio.h>

typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread, SuppressExternalCodecs; } GdiplusStartupInput;
typedef float REAL;
typedef struct { REAL X, Y; } GpPointF;
int WINAPI GdiplusStartup(ULONG_PTR *, const GdiplusStartupInput *, void *);
int WINAPI GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE *, void **);
int WINAPI GdipGetImageGraphicsContext(void *, void **);
int WINAPI GdipCreatePath(INT, void **);
int WINAPI GdipAddPathLine2(void *, const GpPointF *, INT);
int WINAPI GdipStartPathFigure(void *);
int WINAPI GdipCreateSolidFill(DWORD, void **);
int WINAPI GdipFillPath(void *, void *, void *);
int WINAPI GdipBitmapGetPixel(void *, INT, INT, DWORD *);
int WINAPI GdipGraphicsClear(void *, DWORD);
#define PixelFormat32bppARGB 0x26200a

int main(void)
{
    GdiplusStartupInput in = {1, NULL, FALSE, FALSE};
    ULONG_PTR token; void *bmp, *g, *path, *brush; DWORD px; int fails = 0, i;
    GpPointF t1[3] = {{10, 10}, {90, 10}, {10, 90}}, t2[3] = {{110, 10}, {190, 10}, {110, 90}};
    struct { int x, y; DWORD want; const char *what; } chk[] = {
        {20, 20, 0xff00ff00, "inside the first (open) figure"}, {120, 20, 0xff00ff00, "inside the second figure"},
        {80, 80, 0, "outside the first"}, {180, 80, 0, "outside the second"} };

    GdiplusStartup(&token, &in, NULL);
    GdipCreateBitmapFromScan0(200, 100, 0, PixelFormat32bppARGB, NULL, &bmp);
    GdipGetImageGraphicsContext(bmp, &g);
    GdipGraphicsClear(g, 0);
    GdipCreatePath(0 /* FillModeAlternate */, &path);
    GdipAddPathLine2(path, t1, 3);
    GdipStartPathFigure(path);                 /* the first figure stays open */
    GdipAddPathLine2(path, t2, 3);
    GdipCreateSolidFill(0xff00ff00, &brush);
    printf("GdipFillPath %d\n", GdipFillPath(g, brush, path));
    for (i = 0; i < 4; i++)
    {
        GdipBitmapGetPixel(bmp, chk[i].x, chk[i].y, &px);
        printf("%-32s (%d,%d): %08lx\n", chk[i].what, chk[i].x, chk[i].y, (unsigned long)px);
        if (px != chk[i].want) { printf("FAIL: want %08lx\n", (unsigned long)chk[i].want); fails++; }
    }
    printf("fails %d\n", fails);
    return fails;
}
