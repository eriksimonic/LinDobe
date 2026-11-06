#define COBJMACROS
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <stdio.h>
#define W 600
#define H 400
/* Premiere's "Don't Save" button outline, as traced: arcs end 2e-6 off the side lines (12.0000019, 11.999999) */
static ID2D1PathGeometry *path(ID2D1Factory *f, float eps)
{
    ID2D1PathGeometry *g; ID2D1GeometrySink *sk; D2D1_POINT_2F pt;
    D2D1_ARC_SEGMENT a = {{0}, {11, 11}, 90.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL};
    ID2D1Factory_CreatePathGeometry(f, &g); ID2D1PathGeometry_Open(g, &sk);
    pt.x = 1; pt.y = 12; ID2D1GeometrySink_BeginFigure(sk, pt, D2D1_FIGURE_BEGIN_FILLED);
    ID2D1GeometrySink_AddLine(sk, pt); ID2D1GeometrySink_AddLines(sk, &pt, 1);
    a.point.x = 12; a.point.y = 1; ID2D1GeometrySink_AddArc(sk, &a);
    pt.x = 66; pt.y = 1; ID2D1GeometrySink_AddLine(sk, pt); ID2D1GeometrySink_AddLines(sk, &pt, 1);
    a.point.x = 77; a.point.y = 12 + eps * 1.9f; ID2D1GeometrySink_AddArc(sk, &a);
    pt.x = 77; pt.y = 12; ID2D1GeometrySink_AddLine(sk, pt); ID2D1GeometrySink_AddLines(sk, &pt, 1);
    a.point.x = 66; a.point.y = 23; ID2D1GeometrySink_AddArc(sk, &a);
    pt.x = 12; pt.y = 23; ID2D1GeometrySink_AddLine(sk, pt); ID2D1GeometrySink_AddLines(sk, &pt, 1);
    a.point.x = 1; a.point.y = 12 - eps; ID2D1GeometrySink_AddArc(sk, &a);
    ID2D1GeometrySink_EndFigure(sk, D2D1_FIGURE_END_CLOSED);
    ID2D1GeometrySink_Close(sk); ID2D1GeometrySink_Release(sk);
    return g;
}
int main(void)
{
    ID2D1Factory *f; ID2D1RenderTarget *rt; ID2D1SolidColorBrush *br;
    IWICImagingFactory *wic; IWICBitmap *bmp; IWICBitmapLock *lock; UINT size, stride; BYTE *data;
    WICRect wr = {0, 0, W, H}; D2D1_RENDER_TARGET_PROPERTIES rp = {0};
    D2D1_COLOR_F white = {1, 1, 1, 1}, black = {0, 0, 0, 1};
    D2D1_MATRIX_3X2_F m = {1.39583337f, 0, 0, 1.39583337f, 173.333344f, 202.833344f};
    int e, grp, x, y, fails = 0;
    CoInitialize(NULL);
    CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&f);
    IWICImagingFactory_CreateBitmap(wic, W, H, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmp);
    rp.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM; rp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    ID2D1Factory_CreateWicBitmapRenderTarget(f, bmp, &rp, &rt);
    ID2D1RenderTarget_CreateSolidColorBrush(rt, &black, NULL, &br);
    for (e = 0; e < 2; e++) for (grp = 0; grp < 2; grp++)
    {
        ID2D1PathGeometry *g = path(f, e ? 1e-6f : 0); ID2D1Geometry *d = (ID2D1Geometry *)g; ID2D1GeometryGroup *gg = NULL;
        int out = 0, minx = W, maxx = -1;
        if (grp) { ID2D1Factory_CreateGeometryGroup(f, D2D1_FILL_MODE_WINDING, &d, 1, &gg); d = (ID2D1Geometry *)gg; }
        ID2D1RenderTarget_BeginDraw(rt); ID2D1RenderTarget_Clear(rt, &white); ID2D1RenderTarget_SetTransform(rt, &m);
        ID2D1RenderTarget_DrawGeometry(rt, d, (ID2D1Brush *)br, 2.0f, NULL);
        ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
        IWICBitmap_Lock(bmp, &wr, WICBitmapLockRead, &lock); IWICBitmapLock_GetStride(lock, &stride); IWICBitmapLock_GetDataPointer(lock, &size, &data);
        for (y = 0; y < H; y++) for (x = 0; x < W; x++)
            if (data[y * stride + x * 4] < 200 && (y < 195 || y > 245)) { out++; if (x < minx) minx = x; if (x > maxx) maxx = x; }
        IWICBitmapLock_Release(lock);
        printf("epsilon %s, %s: %d px outside the button (x %d-%d) %s\n", e ? "as traced" : "none", grp ? "geometry group" : "path", out, minx, maxx, out ? "FAIL" : "ok");
        fails += !!out;
        if (gg) ID2D1GeometryGroup_Release(gg);
        ID2D1PathGeometry_Release(g);
    }
    printf("fails %d\n", fails);
    return fails;
}
