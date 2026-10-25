#!/bin/bash
# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Packs $WORK/dist/Fallout1in2_x64.appx: the Xbox engine exe, sdl2-compat's SDL2.dll, the Xbox SDL3.dll, the
# notices and licences; marks the exe AppContainer and signs it. xbox/package.sh <version>
# Signing: PFX_FILE (default: the Game Vault certificate next to the work folder) and PFX_PASSWORD
# (or GameVault/pfx_password.ps1 on the build PC). Needs the Windows SDK and Visual Studio build tools.
set -e
V=${1:-0.1.0.0}
REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$(dirname "$REPO")}
ROOT=$(dirname "$WORK")
SDL3_UWP=${SDL3_UWP:-$ROOT/sdl3-uwp}
PFX_FILE=${PFX_FILE:-$ROOT/GameVault/cert/GameVault.pfx}
P=$WORK/pkg
rm -rf "$P"; mkdir -p "$P/licenses"
cp "$REPO/xbox/AppxManifest.xml" "$P/"
cp -r "$REPO/xbox/Assets" "$P/"
cp "$REPO/build-xbox/fallout2-ce.exe" "$P/"
/ucrt64/bin/strip.exe "$P/fallout2-ce.exe" 2>/dev/null || /c/msys64/ucrt64/bin/strip.exe "$P/fallout2-ce.exe"
cp "$WORK/sdl2-compat/build-uwp/SDL2.dll" "$P/"
/ucrt64/bin/strip.exe "$P/SDL2.dll" 2>/dev/null || /c/msys64/ucrt64/bin/strip.exe "$P/SDL2.dll"
cp "$SDL3_UWP/build-uwp/RelWithDebInfo/SDL3.dll" "$P/"
cp "$REPO/xbox/NOTICE.txt" "$P/NOTICE.txt"
cp "$REPO/LICENSE.md" "$P/licenses/fallout2-ce-LICENSE.md"
cp "$WORK/sdl2-compat/LICENSE.txt" "$P/licenses/sdl2-compat-LICENSE.txt"
cp "$SDL3_UWP/LICENSE.txt" "$P/licenses/SDL3-LICENSE.txt"
for l in zlib lodepng stb_vorbis fpattern; do cp "$REPO/third_party/$l/LICENSE" "$P/licenses/$l-LICENSE.txt"; done
# nothing from the build PC's folders may ship
if grep -a -l -i "users[\\/]" "$P/fallout2-ce.exe" "$P/SDL2.dll"; then echo "A BUILD-PC PATH IS IN THE PACKAGE"; exit 1; fi
EB=$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/"*/bin/Hostx64/x64)
"$EB/editbin.exe" //nologo //APPCONTAINER "$P/fallout2-ce.exe"
sed -i "s/ Version=\"[0-9.]*\" ProcessorArchitecture/ Version=\"$V\" ProcessorArchitecture/" "$P/AppxManifest.xml"
SDK="/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
mkdir -p "$WORK/dist"; rm -f "$WORK/dist/Fallout1in2_x64.appx"
"$SDK/makeappx.exe" pack //o //d "$(cygpath -w "$P")" //p "$(cygpath -w "$WORK/dist/Fallout1in2_x64.appx")" >/dev/null
PFX_PASSWORD=${PFX_PASSWORD:-$(/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass \
    -File "$(cygpath -w "$ROOT/GameVault/pfx_password.ps1")" | tr -d '\r\n')}
"$SDK/signtool.exe" sign //fd SHA256 //a //f "$(cygpath -w "$PFX_FILE")" //p "$PFX_PASSWORD" "$(cygpath -w "$WORK/dist/Fallout1in2_x64.appx")" >/dev/null
ls -la "$WORK/dist/Fallout1in2_x64.appx"
