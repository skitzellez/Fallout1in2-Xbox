#!/bin/bash
# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Builds the Xbox engine (FALLOUT_XBOX) with MSYS2 UCRT64 against sdl2-compat (build_sdl2compat.sh first).
# Run inside an UCRT64 shell: xbox/build_fallout.sh [fresh]
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$(dirname "$REPO")}
cd "$REPO"
if [ "${1:-}" = fresh ] || [ ! -f build-xbox/build.ninja ]; then
  rm -rf build-xbox
  # -ffile-prefix-map: no build-machine folder names end up in the exe
  cmake -S . -B build-xbox -G Ninja -DCMAKE_BUILD_TYPE=Release -DFALLOUT_XBOX=ON -DFALLOUT_VENDORED=OFF \
    -DZLIB_LIBRARY=/ucrt64/lib/libz.a -DZLIB_INCLUDE_DIR=/ucrt64/include \
    -DSDL2_DIR="$WORK/sdl2-compat/install/lib/cmake/SDL2" \
    -DCMAKE_CXX_FLAGS="-ffile-prefix-map=$REPO/=" -DCMAKE_C_FLAGS="-ffile-prefix-map=$REPO/="
fi
cmake --build build-xbox --target fallout2-ce
ls -la build-xbox/fallout2-ce.exe
