# shellcheck shell=bash
# LinDobe environment. Source this file; it never touches a system-wide Wine.
#   ADOBEWINE_HOME   where the runtime, dependencies and prefix live
#                    (default: ${XDG_DATA_HOME:-~/.local/share}/adobewine)
#   WINEPREFIX       the Windows prefix (default: $ADOBEWINE_HOME/prefix)
# Path of this file: BASH_SOURCE in bash, %x in zsh (eval keeps bash from parsing zsh syntax).
if [ -n "${BASH_SOURCE[0]:-}" ]; then _aw_env="${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION:-}" ]; then eval '_aw_env="${(%):-%x}"'
else _aw_env="$0"; fi
ADOBEWINE_SRC="$(cd "$(dirname "$_aw_env")/.." && pwd)"; unset _aw_env
ADOBEWINE_HOME="${ADOBEWINE_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/adobewine}"
export ADOBEWINE_SRC ADOBEWINE_HOME
export WINEPREFIX="${WINEPREFIX:-$ADOBEWINE_HOME/prefix}"
# Runtime: the packaged one (e.g. /opt/adobewine/runtime from the AUR package) or the one
# scripts/build-wine.sh built into $ADOBEWINE_HOME/runtime.
if [ -x "$ADOBEWINE_SRC/runtime/bin/wine" ]; then ADOBEWINE_RUNTIME="$ADOBEWINE_SRC/runtime"
else ADOBEWINE_RUNTIME="$ADOBEWINE_HOME/runtime"; fi
export ADOBEWINE_RUNTIME
export PATH="$ADOBEWINE_RUNTIME/bin:$PATH"
export WINEARCH=win64
export WINEDEBUG="${WINEDEBUG:--all,+err}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-winemenubuilder.exe=d}"
export FONTCONFIG_FILE="${FONTCONFIG_FILE:-$ADOBEWINE_SRC/config/fonts.conf}"
mkdir -p "$ADOBEWINE_HOME/logs"
export DXVK_LOG_PATH="$ADOBEWINE_HOME/logs"
# Shader caches in one place: vkd3d-proton otherwise writes vkd3d-proton.cache into whatever folder an app was
# started from (the home folder, a project folder), where the next start may not find it again.
mkdir -p "$ADOBEWINE_HOME/cache/shaders"
export VKD3D_SHADER_CACHE_PATH="${VKD3D_SHADER_CACHE_PATH:-$ADOBEWINE_HOME/cache/shaders}"
export DXVK_STATE_CACHE_PATH="${DXVK_STATE_CACHE_PATH:-$ADOBEWINE_HOME/cache/shaders}"

# Hybrid laptops (Intel/AMD iGPU + NVIDIA dGPU): render on the NVIDIA GPU.
# Set ADOBEWINE_NVIDIA_OFFLOAD=0 to turn this off, =1 to force it.
if [ "${ADOBEWINE_NVIDIA_OFFLOAD:-auto}" = 1 ] || { [ "${ADOBEWINE_NVIDIA_OFFLOAD:-auto}" = auto ] &&
     [ -e /proc/driver/nvidia/version ] && [ "$(ls -d /sys/class/drm/card[0-9] 2>/dev/null | wc -l)" -gt 1 ]; }; then
  export __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia __VK_LAYER_NV_optimus=NVIDIA_only
fi
[ -e /proc/driver/nvidia/version ] && export DXVK_ENABLE_NVAPI=1
# Hide Mesa's CPU Vulkan driver (lavapipe) when a real GPU exists. Wine registers it as a second GPU (VEN_0005) and
# dxcore reports it as hardware, so Windows ML (DxO PureRAW 6) picks it; DXVK hides it, D3D12 device creation fails
# ("Adapter with luid ... not found") and inference falls back to the CPU.
if ls /dev/dri/renderD* >/dev/null 2>&1; then export VK_LOADER_DRIVERS_DISABLE="${VK_LOADER_DRIVERS_DISABLE-*lvp*}"; fi
# AMD's OpenCL rejects some of Premiere's kernels without this, and those clips scrub black (#15). Only AMD's driver reads it.
export AMD_OCL_BUILD_OPTIONS_APPEND="${AMD_OCL_BUILD_OPTIONS_APPEND--Wf,-Wno-error=incompatible-pointer-types}"

# DxO PureRAW 6 (Qt 6): Qt Quick's D3D11 swapchain fails under DXVK (white window), so Qt draws with OpenGL.
# Qt WebEngine with OpenGL then needs WGL_NV_DX_interop, which Wine lacks (crash), so WebEngine renders on the CPU.
# Lightroom starts PureRAW from its plugin, so these must be in every app's environment. No Adobe app uses Qt Quick.
export QSG_RHI_BACKEND="${QSG_RHI_BACKEND-opengl}"
export QTWEBENGINE_CHROMIUM_FLAGS="${QTWEBENGINE_CHROMIUM_FLAGS---disable-gpu --disable-gpu-compositing}"

# Creative Cloud and its helpers write to heap blocks after freeing them; patch 0032 keeps
# freed blocks out of reuse for a while in these processes only.
export WINE_HEAP_QUARANTINE="${WINE_HEAP_QUARANTINE-Creative Cloud.exe;Creative Cloud Helper.exe;Creative Cloud UI Helper.exe;Adobe Desktop Service.exe;node.exe;CCXProcess.exe;CoreSync.exe;CRLogTransport.exe;HDHelper.exe;AdobeIPCBroker.exe;AdobeUpdateService.exe}"
