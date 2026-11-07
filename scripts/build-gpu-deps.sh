#!/bin/bash
# build-gpu-deps.sh [DEPS_DIR]: build the patched vkd3d-proton (d3d12core.dll) and nvcuda (nvcuda.dll) that
# After Effects' viewers need, and DXVK's d3d11.dll with Intel's device information counter (Premiere's OpenCL
# path on Intel GPUs), patches in patches-deps, into DEPS_DIR/adobewine-gpu/x64.
# Needs git, python3 (meson and ninja go into a venv), mingw-w64, glslang and winegcc (Wine >= 10 headers).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
DEPS=${1:-${ADOBEWINE_HOME:-${XDG_DATA_HOME:-$HOME/.local/share}/adobewine}/deps}
SRC=$DEPS/src
OUT=$DEPS/adobewine-gpu/x64
mkdir -p "$SRC" "$OUT"
# meson and ninja from the system when it has them (the Flatpak SDK does), else a venv
if ! command -v meson >/dev/null || ! command -v ninja >/dev/null; then
  [ -x "$DEPS/venv/bin/meson" ] || { python3 -m venv "$DEPS/venv"; "$DEPS/venv/bin/pip" -q install meson ninja; }
  export PATH="$DEPS/venv/bin:$PATH"
fi

# vkd3d-proton 3.0.1
V=$SRC/vkd3d-proton-adobewine
if [ ! -d "$V" ]; then
  git clone -q --branch v3.0.1 --depth 1 https://github.com/HansKristian-Work/vkd3d-proton.git "$V"
  git -C "$V" submodule update -q --init --recursive --depth 1
  git -C "$V" apply "$HERE"/patches-deps/vkd3d-proton-3.0.1-*.patch
fi
meson setup --reconfigure --cross-file "$V/build-win64.txt" --buildtype release --strip -Denable_tests=false \
  "$V/build" "$V" >/dev/null 2>&1 || meson setup --cross-file "$V/build-win64.txt" --buildtype release --strip \
  -Denable_tests=false "$V/build" "$V" >/dev/null
ninja -C "$V/build" >/dev/null
cp "$V/build/libs/d3d12core/d3d12core.dll" "$OUT/"
x86_64-w64-mingw32-strip "$OUT/d3d12core.dll"

# DXVK 3.1.1 d3d11.dll: Intel's "Intel Device Information" counter (Premiere reads the GPU generation from it and
# otherwise rejects Intel GPUs as "Hardware out of date"); idle presenters give up their Vulkan swapchain past 192 live
# ones, as the NVIDIA driver refuses the 253rd, and at once when their window was destroyed, as such a swapchain can
# make the NVIDIA driver lose the device (Audition's long-session hang; tests/misc/swapchurn.c, vkswapcap.c)
D=$SRC/dxvk-adobewine
if [ ! -d "$D" ]; then
  git clone -q --branch v3.1.1 --depth 1 https://github.com/doitsujin/dxvk.git "$D"
  git -C "$D" submodule update -q --init --recursive --depth 1
  git -C "$D" apply "$HERE"/patches-deps/dxvk-3.1.1-*.patch
fi
[ -d "$D/build.w64" ] || meson setup --cross-file "$D/build-win64.txt" --buildtype release --strip "$D/build.w64" "$D" >/dev/null
ninja -C "$D/build.w64" src/d3d11/d3d11.dll >/dev/null
x86_64-w64-mingw32-strip -o "$OUT/d3d11.dll" "$D/build.w64/src/d3d11/d3d11.dll"

# nvcuda (SveSop/nvcuda, the CUDA bridge nvidia-libs ships)
N=$SRC/nvcuda-adobewine
if [ ! -d "$N" ]; then
  git clone -q https://github.com/SveSop/nvcuda.git "$N"
  git -C "$N" checkout -q d4c0fca9c570fa47fcf16a527e292d1e64772af6
  git -C "$N" apply "$HERE"/patches-deps/nvcuda-d4c0fca-*.patch
fi
( cd "$N" && winegcc -m64 --no-gnu-unique -D__WINESRC__ -mwindows -shared -Iinclude -Idlls/nvcuda -O2 \
    -o "$OUT/nvcuda.dll" dlls/nvcuda/internal.c dlls/nvcuda/nvcuda.c dlls/nvcuda/function_mappings.c \
    dlls/nvcuda/encryption.c dlls/nvcuda/d3d.c dlls/nvcuda/nvcuda.spec -ldl -lpthread -lgdi32 2>/dev/null )
mv "$OUT/nvcuda.dll.so" "$OUT/nvcuda.dll" 2>/dev/null || true
strip "$OUT/nvcuda.dll"
ls -la "$OUT"
