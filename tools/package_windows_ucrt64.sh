#!/usr/bin/env bash
# Build and package the native Win32 AVP executable using MSYS2 UCRT64.
# This is a release script, not a substitute for verifying the Windows binary.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build-win}"
out="${2:-$root/dist/windows}"
: "${MINGW_PREFIX:?Run from the MSYS2 UCRT64 shell (MINGW_PREFIX must be set)}"
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
  echo 'ERROR: select the MSYS2 UCRT64 shell, not MSYS or MINGW64.' >&2
  exit 2
fi

cmake -S "$root" -B "$build" -G Ninja -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j4
exe="$build/AVP_Jaguar_PC.exe"
if [[ ! -s "$exe" ]]; then echo "ERROR: missing executable $exe" >&2; exit 1; fi
"$exe" --help >/dev/null

mkdir -p "$out"
cp -f "$exe" "$out/AVP_Jaguar_PC.exe"
# The executable links libgcc/libstdc++/winpthread/zlib statically; this loop
# only copies a UCRT64 runtime DLL if one is still referenced.  System DLLs are
# provided by Windows and must not be redistributed.
queue=("$exe")
declare -A visited=()
while ((${#queue[@]})); do
  candidate="${queue[0]}"
  queue=("${queue[@]:1}")
  while IFS= read -r name; do
    key="${name,,}"
    [[ -n "$name" && ! ${visited[$key]+x} ]] || continue
    visited[$key]=1
    dll="$(find "$MINGW_PREFIX/bin" -maxdepth 1 -type f -iname "$name" -print -quit)"
    [[ -n "$dll" ]] || continue
    cp -f "$dll" "$out/$(basename "$dll")"
    queue+=("$dll")
  done < <(objdump -p "$candidate" | awk '/^[[:space:]]*DLL Name:/ {print $3}')
done
cat > "$out/README.txt" <<'README'
Alien vs Predator (Atari Jaguar) - native Windows PC port
Place your own verified AVP.jag in this folder alongside AVP_Jaguar_PC.exe,
then launch the executable. Or use: AVP_Jaguar_PC.exe --rom "C:\path\to\AVP.jag"
The retail ROM, ROM-derived assets, and personal saves are not included.
This bundle requires gameplay testing on a real Windows machine.
README
printf 'Created Windows build bundle: %s\n' "$out"
