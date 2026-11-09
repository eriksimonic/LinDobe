# Child-window presentation probe

`d3d12child.c` presents D3D12 into a child HWND. Existing modes are `-once`
(a retained red frame) and `-wait` (a frame-latency waitable object).

```sh
x86_64-w64-mingw32-gcc -O2 -o /tmp/d3d12child.exe d3d12child.c -ld3d12 -ld2d1 -ldxgi -luuid -mwindows
```

With the installed Wine SDK, `winegcc -m64 -b x86_64-windows` can produce the
PE executable instead. Run it from a directory without an application-local
development `dxgi.dll`, using the same DXVK/vkd3d runtime as the application.

`-underlay` reproduces the competing-surface arrangement observed in After
Effects: after presenting D3D12 red once, it creates a D2D HWND target for the
same child and repeatedly clears that target to black. After 15 seconds it
releases the D3D12 swapchain and changes the D2D clear to green. The program
exits after 25 seconds. API creation/drawing results and phase transitions go
to `C:\users\Public\d3d12child.log`.

The intended compatibility behavior is retained D3D12 red while that swapchain
owns presentation, followed by D2D green after release. Record the actual
visible colors manually; the program takes no screenshots and makes no claim
of visual success from API return values alone. This mixed-API case is a Wine
compatibility probe, not a Windows API conformance assertion: Microsoft advises
against [combining flip presentation and other graphics APIs in one HWND](https://learn.microsoft.com/windows/win32/direct3ddxgi/dxgi-flip-model).

Observed on Wine 11.20 with DXVK/vkd3d and X11/Hyprland: the original runtime
shows black then green. Patch 0136 retains red, then correctly permits green
after swapchain release. Both color sequences were checked by the user;
Windows reference behavior has not been tested.
