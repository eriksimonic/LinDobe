/* Stand-in for Microsoft.WindowsAppRuntime.Bootstrap.dll under Wine.
 * The real loader looks up the Windows App Runtime MSIX package through
 * PackageManager, which Wine does not implement. This one reports success,
 * so the app uses the runtime DLLs it ships next to its exe. */
#include <windows.h>

HRESULT WINAPI MddBootstrapInitialize(UINT32 ver, const WCHAR *tag, UINT64 minver)
{
    OutputDebugStringA("mddbootstrap-shim: MddBootstrapInitialize\n");
    return S_OK;
}

HRESULT WINAPI MddBootstrapInitialize2(UINT32 ver, const WCHAR *tag, UINT64 minver, UINT32 options)
{
    OutputDebugStringA("mddbootstrap-shim: MddBootstrapInitialize2\n");
    return S_OK;
}

void WINAPI MddBootstrapShutdown(void)
{
}

HRESULT WINAPI MddBootstrapTestInitialize(const WCHAR *prefix, const WCHAR *publisher)
{
    return S_OK;
}
