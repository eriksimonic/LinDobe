#!/usr/bin/env bash
# Maintainers: build the prebuilt release tarball for the adobewine-bin AUR package.
#   cd packaging/aur/adobewine && makepkg -f && ../../../scripts/make-release.sh
set -euo pipefail
cd "$(dirname "$0")/../packaging/aur/adobewine"
ver="$(grep -m1 '^pkgver=' PKGBUILD | cut -d= -f2)"
out="$PWD/adobewine-$ver-x86_64.tar.zst"
# The release must run on any x86-64 CPU: refuse host-only instructions (GFNI, AVX-512) in the native modules.
bad=0
for so in pkg/adobewine/opt/adobewine/runtime/lib/wine/x86_64-unix/*.so; do
  n=$(objdump -d "$so" | grep -cE 'gf2p8|%zmm' || true)
  [ "$n" -eq 0 ] || { echo "$so: $n GFNI/AVX-512 instructions (built with -march=native?)" >&2; bad=1; }
done
[ "$bad" -eq 0 ] || exit 1
tar -C pkg/adobewine -I 'zstd -19 -T0' -cf "$out" opt
sha256sum "$out"
echo "upload $out to the v$ver GitHub release, then put its sha256 in packaging/aur/adobewine-bin/PKGBUILD"
