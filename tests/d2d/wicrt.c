/* Two WIC bitmap render targets, one after the other, on the same IWICBitmap: the first draws a red square on the
 * left, the second a blue square on the right. On Windows the second draws onto what the first left in the bitmap,
 * so both squares remain. Wine's WIC render target started from an empty texture and wrote all of it back, so only
 * the last drawing survived (Premiere's time rulers: one marker, one time number, no ticks; issue #28).
 *   wine wicrt.exe      prints the two squares' pixels and "fails N" */
#define COBJMACROS
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <stdio.h>

static void draw(ID2D1Factory *f, IWICBitmap *bmp, float x, D2D1_COLOR_F c, int clear)
{
    D2D1_RENDER_TARGET_PROPERTIES p = {0}; ID2D1RenderTarget *rt; ID2D1SolidColorBrush *br;
    D2D1_RECT_F r = {x, 10, x + 20, 30}; D2D1_COLOR_F none = {0, 0, 0, 0};
    p.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM; p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    ID2D1Factory_CreateWicBitmapRenderTarget(f, bmp, &p, &rt);
    ID2D1RenderTarget_CreateSolidColorBrush(rt, &c, NULL, &br);
    ID2D1RenderTarget_BeginDraw(rt);
    if (clear) ID2D1RenderTarget_Clear(rt, &none);
    ID2D1RenderTarget_FillRectangle(rt, &r, (ID2D1Brush *)br);
    ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
    ID2D1SolidColorBrush_Release(br); ID2D1RenderTarget_Release(rt);
}

int main(void)
{
    IWICImagingFactory *wic; IWICBitmap *bmp; ID2D1Factory *f; IWICBitmapLock *lock;
    UINT size, stride; BYTE *d; WICRect wr = {0, 0, 64, 40};
    D2D1_COLOR_F red = {1, 0, 0, 1}, blue = {0, 0, 1, 1};
    int fails = 0;
    CoInitialize(NULL);
    CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wic);
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&f);
    IWICImagingFactory_CreateBitmap(wic, 64, 40, &GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmp);
    draw(f, bmp, 5, red, 1);
    draw(f, bmp, 35, blue, 0);
    IWICBitmap_Lock(bmp, &wr, WICBitmapLockRead, &lock);
    IWICBitmapLock_GetStride(lock, &stride); IWICBitmapLock_GetDataPointer(lock, &size, &d);
    {
        BYTE *a = d + 20 * stride + 15 * 4, *b = d + 20 * stride + 45 * 4;
        printf("first square (red) BGRA %u %u %u %u %s\n", a[0], a[1], a[2], a[3], a[2] > 200 && a[3] > 200 ? "ok" : "FAIL");
        printf("second square (blue) BGRA %u %u %u %u %s\n", b[0], b[1], b[2], b[3], b[0] > 200 && b[3] > 200 ? "ok" : "FAIL");
        fails = !(a[2] > 200 && a[3] > 200) + !(b[0] > 200 && b[3] > 200);
    }
    IWICBitmapLock_Release(lock);
    printf("fails %d\n", fails);
    return fails;
}
