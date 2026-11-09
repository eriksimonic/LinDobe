#define COBJMACROS
#include <windows.h>
#include <dxgi.h>
#include <stdio.h>

int main(void)
{
    IDXGIFactory *factory;
    IDXGIAdapter *adapter;
    IDXGIOutput *output;
    DXGI_OUTPUT_DESC desc;
    LARGE_INTEGER frequency, start, end;
    HRESULT hr;
    UINT adapter_index, output_index;
    unsigned int count = 0, failures = 0, i;

    hr = CreateDXGIFactory(&IID_IDXGIFactory, (void **)&factory);
    if (FAILED(hr))
    {
        printf("CreateDXGIFactory: %#x\n", (unsigned int)hr);
        return 1;
    }

    QueryPerformanceFrequency(&frequency);
    for (adapter_index = 0; ; ++adapter_index)
    {
        hr = IDXGIFactory_EnumAdapters(factory, adapter_index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) { ++failures; break; }

        for (output_index = 0; ; ++output_index)
        {
            hr = IDXGIAdapter_EnumOutputs(adapter, output_index, &output);
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) { ++failures; break; }

            hr = IDXGIOutput_GetDesc(output, &desc);
            if (SUCCEEDED(hr) && desc.AttachedToDesktop)
            {
                ++count;
                QueryPerformanceCounter(&start);
                for (i = 0; i < 3; ++i)
                {
                    hr = IDXGIOutput_WaitForVBlank(output);
                    if (hr != S_OK) { ++failures; break; }
                }
                QueryPerformanceCounter(&end);
                printf("adapter %u output %u: hr=%#x, %u waits, %.2f ms, %s\n",
                        adapter_index, output_index, (unsigned int)hr, i,
                        (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart,
                        hr == S_OK ? "PASS" : "FAIL");
            }
            else if (FAILED(hr)) ++failures;
            IDXGIOutput_Release(output);
        }
        IDXGIAdapter_Release(adapter);
    }
    IDXGIFactory_Release(factory);
    if (!count) puts("No attached outputs: FAIL");
    return failures || !count;
}
