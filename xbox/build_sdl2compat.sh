#!/bin/bash
# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Builds sdl2-compat (the SDL2 API on top of the Xbox build of SDL3) with MSYS2 UCRT64 into $WORK/sdl2-compat.
# WORK: the folder that holds this repo (default: the repo's parent); SDL3_UWP: the Xbox SDL3 source+build.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$(dirname "$REPO")}
SDL3_UWP=${SDL3_UWP:-$(dirname "$WORK")/sdl3-uwp}
cd "$WORK"
[ -d sdl2-compat ] || git clone --depth 1 --branch release-2.32.74 https://github.com/libsdl-org/sdl2-compat.git
cd sdl2-compat
cmake -S . -B build-uwp -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL2COMPAT_TESTS=OFF \
  -DSDL3_INCLUDE_DIRS="$SDL3_UWP/include" -DCMAKE_C_FLAGS="-ffile-prefix-map=$WORK/="
cmake --build build-uwp
cmake --install build-uwp --prefix "$WORK/sdl2-compat/install"
