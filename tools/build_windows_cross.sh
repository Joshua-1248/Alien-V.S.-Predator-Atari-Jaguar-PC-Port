#!/usr/bin/env bash
# Cross-compile the native Win32 AVP executable from Linux with MinGW-w64,
# verify the PE image, and package it.  Requires: x86_64-w64-mingw32-gcc/g++,
# MinGW zlib (static libz.a), cmake.  Wine is optional (startup and quickstate
# checks run only when it is installed).  The ROM is never packaged.
#   tools/build_windows_cross.sh [BUILD_DIR] [OUT_DIR]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build-win}"
out="${2:-$root/dist/windows}"
triple=x86_64-w64-mingw32

# FindZLIB can select libz.dll.a (DLL import library) even when libz.a exists.
# Resolve the real static archive explicitly and override any stale CMake cache.
static_zlib="$("$triple-gcc" -print-file-name=libz.a)"
if [[ "$static_zlib" == libz.a || ! -s "$static_zlib" ]]; then
  static_zlib="/usr/$triple/lib/libz.a"
fi
if [[ ! -s "$static_zlib" ]]; then
  echo "ERROR: static MinGW zlib (libz.a) not found. Install libz-mingw-w64-dev." >&2
  exit 1
fi
printf 'Using static MinGW zlib: %s\n' "$static_zlib"

cmake -S "$root" -B "$build" \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64-x86_64.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON \
  -DZLIB_LIBRARY_RELEASE:FILEPATH="$static_zlib" \
  -DZLIB_LIBRARY_DEBUG:FILEPATH="$static_zlib" \
  -DZLIB_LIBRARY:FILEPATH="$static_zlib"
cmake --build "$build" -j"$(nproc 2>/dev/null || echo 4)" --target avp_pc
exe="$build/AVP_Jaguar_PC.exe"
[[ -s "$exe" ]] || { echo "ERROR: missing $exe" >&2; exit 1; }

# 1. Only operating-system DLLs may be imported (static libgcc/libstdc++/
#    winpthread/zlib), so the executable runs without companion runtime DLLs.
allowed='^(kernel32|user32|gdi32|winmm|xinput9_1_0|bcrypt|msvcrt|ucrtbase|api-ms-win-crt-[a-z0-9-]+)\.dll$'
bad=0
while IFS= read -r dll; do
  if [[ ! "${dll,,}" =~ $allowed ]]; then echo "ERROR: non-system DLL import: $dll" >&2; bad=1; fi
done < <("$triple-objdump" -p "$exe" | awk '/^[[:space:]]*DLL Name:/ {print $3}')
((bad==0)) || exit 1

# 2. Every loaded section must start on SectionAlignment (the Windows loader
#    rejects the image otherwise), and the game-state section must exist.
"$triple-objdump" -x "$exe" | python3 -c '
import re,sys
align=None; seen=False; bad=False; lines=sys.stdin.read().splitlines()
for i,l in enumerate(lines):
    m=re.match(r"SectionAlignment\s+([0-9a-fA-F]+)",l)
    if m: align=int(m.group(1),16)
    m=re.match(r"\s+\d+ (\.\S+)\s+[0-9a-f]+\s+([0-9a-f]+)",l)
    if m and i+1<len(lines) and "ALLOC" in lines[i+1]:
        if m.group(1)==".avpgs": seen=True
        if align and int(m.group(2),16)%align:
            print("ERROR: section",m.group(1),"VMA misaligned"); bad=True
if not align: print("ERROR: no SectionAlignment"); bad=True
if not seen: print("ERROR: .avpgs game-state section missing"); bad=True
sys.exit(1 if bad else 0)'

# 3. Optional runtime checks under Wine.
if command -v wine >/dev/null 2>&1; then
  export WINEDEBUG=-all
  wine "$exe" --help >/dev/null 2>&1 || { echo "ERROR: --help failed under Wine" >&2; exit 1; }
  qs="$build/test_quickstate_pe_r284.exe"
  "$triple-gcc" -O2 -I"$root/include" "$root/tests/test_quickstate_pe_r284.c" \
    "$root/src/platform/pc/pc_quickstate.c" "$root/src/platform/pc/pc_time.c" \
    -Wl,--start-group "$build/libavp_pc_core.a" "$build/libavp_jaguar_compat.a" -Wl,--end-group \
    -Wl,-T,"$root/cmake/avp_game_state_pe.ld" -static -o "$qs"
  wine "$qs"
else
  echo "NOTE: wine not installed; skipped Windows startup/quickstate checks."
fi

mkdir -p "$out"
"$triple-strip" -o "$out/AVP_Jaguar_PC.exe" "$exe"
cat > "$out/README.txt" <<'README'
Alien vs Predator (Atari Jaguar) - native Windows PC port

1. Put your own retail AVP.jag in this folder, next to AVP_Jaguar_PC.exe.
   (Or run: AVP_Jaguar_PC.exe --rom "C:\path\to\AVP.jag")
2. Double-click AVP_Jaguar_PC.exe.

The executable is self-contained (64-bit Windows; no extra DLLs needed).
Settings (settings.cfg), Jaguar EEPROM saves (avp_eeprom.bin), the
ROM-derived cache folder and avp_pc.log are written beside the executable, so
keep the folder somewhere writable (not Program Files).
Controllers: XInput gamepads (Xbox-compatible).
F5/F9 quicksave works across restarts on compatible executable revisions.
Durable quicksaves are stored under %LOCALAPPDATA%/AVP_Jaguar (falling back
to %APPDATA%).  Original Jaguar save slots also work.
The retail ROM, ROM-derived assets and personal saves are not included.
README
printf 'Created Windows bundle: %s\n' "$out"
