# Mister Horse on LinDobe

The end-to-end workflow passed in an isolated development prefix: Product
Manager opens, accepts browser login, installs Animation Composer and free
content, and retains the session and installation after restarting. In After
Effects, the user applied a free preset, confirmed playback and normal panel
interaction, and verified stable composition display. Saving, reopening, and
a short render passed with the final development modules.

This is experimental support for the versions below. A production runtime
build/install, Windows reference comparison, other plugin vendors, Premiere,
and broad Adobe regression testing were not performed.

## Tested setup

| Component | Version or configuration |
|---|---|
| Date | 2026-11-08 |
| Runtime | Wine 11.20, `/opt/adobewine/runtime` |
| Product Manager | 3.7.7, JUCE 8.0.10 |
| Animation Composer | 4.2.6 |
| After Effects | 26.5.0.89 |
| Desktop | CachyOS, Hyprland through XWayland |
| Hardware | Ryzen 5 5600X, NVIDIA RTX 3060 |
| Repository base | `3aaea375bc011bc23a51ccf4edc8711cb9b98eee` |

The official MSI came from
<https://misterhorse.com/downloads/product-manager/win> and has SHA-256
`bbc4b34ad4ce30cfc4b06e970bd6c851cd6125aced36d17bea35910045d69b1d`.
Installation used Product Manager, following
[Mister Horse's documented workflow](https://help.misterhorse.com/hc/en-us/articles/4405555986321-How-to-install-Animation-Composer).

## Isolation and installation

The test prefix is a separate copy of the working Adobe prefix. Personal-folder
symlinks in the copy were replaced with local directories. Every Wine
execution uses an explicit test `WINEPREFIX` and
`PATH=/opt/adobewine/runtime/bin:/usr/bin`. The working prefix, installed
runtime, Adobe settings, portal file-dialog settings, and window decorations
were preserved. Do not refresh the entire prefix to install this plugin.

The official MSI was installed in that copy. The user entered credentials in
the browser/application, then installed Animation Composer and free content
through Product Manager. The installed files include:

- `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\MisterHorse\AnimationComposer4\AnimationComposer4.aex`
- `C:\Program Files\Common Files\Adobe\CEP\extensions\com.misterhorse.AnimationComposer`

The browser returns through `misterhorsepm:`. The MSI registers the Windows
protocol handler, but Linux also needs an association that forwards the URI
as one argument to Product Manager in the same prefix through `adobewine run`.
The test uses `lindobe-misterhorse-test.desktop` for
`x-scheme-handler/misterhorsepm`. No generic plugin-manager command was added.
Do not log or share callback URIs, which may contain authentication data.

## Required corrections

### Product Manager startup and interaction

DXVK returns `E_NOTIMPL` for the composition swapchain used by JUCE. Wine's
DXGI supports that path through patches 0050/0051, but its `WaitForVBlank`
stub prevents JUCE's VBlankThread from completing shutdown during window
construction. Patch **0134** connects the wait to the existing D3DKMT
implementation from 0047. The standalone test changes from immediate
`E_NOTIMPL` on both attached outputs to successful waits.

Product Manager uses builtin D3D11/D3D10Core with the development Wine DXGI
and `WINE_D3D_CONFIG=renderer=vulkan`. The OpenGL comparison had incorrect
presentation and an invisible cursor. A native debugger also caught a GFNI
instruction in the installed `winex11.so`, unsupported by this test CPU.
Setting only
`HKCU\Software\Wine\AppDefaults\ProductManager.exe\X11 Driver\ClientSideWithRender`
to string `N` avoids that path. This is a runtime/CPU-specific workaround,
not a global XRender setting.

The final cold launch retains the account session and installed package. The
user confirms that Product Manager now draws correctly. Earlier hover
artifacts were not isolated to one specific correction; ignoring DXGI dirty
rectangles remains a diagnostic observation, not a demonstrated cause. The
single-buffer experiment produced a blank window and was removed.

Patch 0134 inherits 0047's refresh-based pacing, not hardware vblank
synchronization. That implementation uses the primary display's refresh
rate rather than the requested output's rate.

### Animation Composer loading

Animation Composer imports `KERNEL32.ReadDirectoryChangesExW`, missing in the
original runtime. It requests extended notifications and has an existing
fallback to `ReadDirectoryChangesW` when the request fails. Patch **0135**
implements the basic information class and rejects the unsupported extended
format with `ERROR_INVALID_FUNCTION`, allowing that fallback without returning
incorrectly formatted records. It does not claim full extended-notification
support. The asynchronous notification test passes, and After Effects starts
and loads the plugin instead of aborting on the missing import.

### After Effects viewer presentation

The viewer flicker also reproduces without Mister Horse, with the original
core DLLs, and with hardware-accelerated viewers disabled before a cold start.
Traces identify D3D11/DXVK and D3D12/vkd3d surfaces targeting the same viewer
HWND. Temporarily suppressing either source distinguishes the black covering
surface from the actual composition. No source-ID filter is retained.

Patch **0136** recognizes vkd3d's Vulkan engine name. After a successful
presentation, a private window property retains that client surface as the
HWND's presentation owner; competing client-surface copies are skipped until
the owning swapchain/surface is destroyed. The graphics-driver ABI is
unchanged. The mixed D2D/D3D12 probe confirms both retained display and return
to ordinary D2D drawing after release.

This is a targeted vkd3d compatibility rule. Microsoft discourages mixing
flip presentation with other APIs in one HWND, so it is not presented as a
general multi-API composition implementation or a Windows conformance fix.
Other Adobe applications still need regression coverage before general
deployment.

## Acceptance evidence

| Check | Result |
|---|---|
| Product Manager content, input and browser login | User confirmed |
| Session and installed-package persistence after cold restart | User confirmed |
| Final Product Manager redraw | User confirmed correct |
| After Effects startup and Animation Composer panel | User confirmed |
| Free preset application and playback | User confirmed |
| Final viewer stability and panel mouse resizing | User confirmed normal |
| Save and reopen | `saveAndReopen=PASS`, three enabled expressions |
| Final short render | `render=PASS`, 1280x720, 24 fps, 3 seconds, 72 frames |
| Render content | 21 distinct frame hashes; all 72 hashes match the earlier render |
| Mixed D2D/D3D12 probe | Original: black then green; patched: red then green |

The final application checks use patch 0136 with the original X11 driver and
without the diagnostic source filter. Viewer acceleration, plugin files, and
other temporary comparison settings were restored. No screenshots or visual
automation tools were used; visible behavior was checked by the user. Render
validation uses ExtendScript, media metadata, and frame hashes.

Reproducers and acceptance scripts:

- `tests/vblank/dxgi.c`
- `tests/directory-notify/ex.c`
- `tests/d3d12child/d3d12child.c -underlay`
- `tests/misterhorse/setup.jsx` and `verify.jsx`

## Prepared development-prefix commands

These commands require the already-prepared development prefix and its matching
module files. They are not an installation recipe for the unpatched installed
runtime. Set both paths explicitly:

```sh
test_root=/absolute/path/to/prepared-test-root
launcher=/absolute/path/to/adobe-wine/bin/adobewine

env PATH=/opt/adobewine/runtime/bin:/usr/bin \
    WINEPREFIX="$test_root/prefix" ADOBEWINE_HOME="$test_root" \
    WINEDLLOVERRIDES='kernel32,kernelbase=n,b;win32u=n,b;winex11.drv=b;dxgi=n;d3d11,d3d10core=b;winemenubuilder.exe=d' \
    WINE_D3D_CONFIG=renderer=vulkan \
    "$launcher" run "$test_root/prefix/drive_c/Program Files/Mister Horse Product Manager/ProductManager.exe"

env PATH=/opt/adobewine/runtime/bin:/usr/bin \
    WINEPREFIX="$test_root/prefix" ADOBEWINE_HOME="$test_root" \
    WINEDLLOVERRIDES='kernel32,kernelbase=n,b;win32u=n,b;winex11.drv=b;winemenubuilder.exe=d' \
    "$launcher" aftereffects
```

The development core DLLs are in the test prefix's System32; development DXGI
is only beside Product Manager, while System32 retains the original DXVK.
Native-first loading uses a DOS-stub marker change from `Wine builtin DLL` to
`Test builtin DLL`. Development `win32u.dll` additionally loads its matching
Unix module by an explicit development path. That loader modification is not
part of the submitted patch. Process maps confirm one development `win32u.so`
with the installed `/opt` ntdll and X11 modules. Only development module targets
were built; no production build or runtime installation was performed.

## Rollback

Close the test applications normally before replacing core DLLs. Stop only
the test prefix's Wine server. Restore `kernel32-prefix-original.dll`,
`kernelbase-prefix-original.dll`, and `win32u-prefix-original.dll` from the
investigation backup directory to their corresponding System32 names, and
omit their native overrides. The diagnostic X11 driver was already restored
from `x11-prefix-original.drv`.

Remove only the development `dxgi.dll` beside Product Manager and omit its
DXGI/D3D overrides and renderer environment variable. Leave System32's DXVK
unchanged. Delete only the test prefix's process-specific
`ClientSideWithRender` value to undo that workaround; preserve the rest of
the key and all global file-dialog/decoration settings.

The test protocol handler is installed in
`~/.local/share/applications/lindobe-misterhorse-test.desktop` and points to
the isolated prefix. There was no previous `misterhorsepm:` association.
When removing the test setup, remove that desktop entry and only its MIME
association, preserving the user's other MIME configuration. Authentication,
package data, development binaries and private logs remain outside the repo.
