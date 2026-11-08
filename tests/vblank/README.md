# Vertical blank diagnostics

`vblank.c` tests the D3DKMT wait implemented by patch 0047. `dxgi.c` checks
`IDXGIOutput::WaitForVBlank` on every attached output. JUCE 8 uses this API
both for frame notifications and to let its VBlankThread finish shutdown.

Build the DXGI test with MinGW for Windows or Wine:

```sh
x86_64-w64-mingw32-gcc -O1 -o dxgi-vblank.exe dxgi.c -ldxgi -ldxguid
```

For a local Wine development test, winegcc also works:

```sh
/opt/adobewine/runtime/bin/winegcc -m64 -Wall -Wextra -o /tmp/dxgi-vblank.exe dxgi.c -ldxgi -ldxguid
```

Run with the same explicit prefix and runtime as the application being tested.
When testing Wine's DXGI rather than DXVK, use `WINEDLLOVERRIDES=dxgi=b`.
Use a timeout around the test in case the wait itself hangs.

Each attached output must return `S_OK` for three consecutive waits. The test
prints elapsed time for diagnosis but does not assert an exact duration:
refresh rates, scheduling, and the initial position within a refresh differ.
No attached outputs is a failure, not a pass.

On the tested LinDobe runtime, before patch 0134, both attached outputs return
`E_NOTIMPL` immediately. With the candidate patch, both complete all three
waits. Windows reference validation has not been performed.
