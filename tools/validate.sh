#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-validate"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j2
bash "$ROOT/tests/run_tests.sh"
printf 'int main(void){return 0;}\n' > /tmp/avp_c_empty_main.c
# The real game links libm (PLAYER.C uses floor/fmod); the standalone
# whole-archive check must carry that same dependency after the archive.
"${CC:-cc}" /tmp/avp_c_empty_main.c -Wl,--whole-archive "$BUILD/libavp_c.a" -Wl,--no-whole-archive -lm -o /tmp/avp_c_whole_archive
"${CC:-cc}" /tmp/avp_c_empty_main.c -Wl,--whole-archive "$BUILD/libavp_rom_util.a" "$BUILD/libavp_rom_bootstrap.a" -Wl,--no-whole-archive -o /tmp/avp_rom_whole_archive
# Shipping C/H (including PC-only modules under new/) must not contain
# explicit unfinished-implementation markers. The bare phrase "queued for"
# also describes normal event queues, so require a deferred-work qualifier.
# Research notes and third-party code are intentionally outside this scan.
if grep -RniE 'TODO|FIXME|placeholder until|stub[[:space:]]*(implementation|routine)|queued for[[:space:]]+(later|future|implementation|reimplementation|completion|follow-up)' "$ROOT/src" "$ROOT/include" "$ROOT/new"; then
  echo 'FAIL: unfinished implementation marker found in active source' >&2
  exit 1
fi
python3 "$ROOT/tools/audit_public_tree.py"
python3 "$ROOT/tools/audit_architecture.py"
echo 'PASS: strict build, tests, whole-archive link and publication audit'
