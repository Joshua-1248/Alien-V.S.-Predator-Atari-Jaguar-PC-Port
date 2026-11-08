#!/usr/bin/env bash
# No retail ROM or game assets needed. Runs save/load in separate processes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
CC="${CC:-gcc}"
"$CC" -std=gnu99 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/tests/test_pc_quickstate_disk_restart_s44.c" \
  "$ROOT/src/platform/pc/pc_quickstate.c" \
  "$ROOT/src/platform/pc/pc_quickstate_disk.c" \
  "$ROOT/src/platform/pc/pc_sha256.c" -lz -lcrypto -o "$TMP/restart"
export XDG_DATA_HOME="$TMP/userdata"
mkdir -p "$XDG_DATA_HOME"
"$TMP/restart" save
"$TMP/restart" load
SAVE="$XDG_DATA_HOME/avp-jaguar/avp_quickstate.bin"
cp "$SAVE" "$TMP/backup"
python3 - "$SAVE" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);b=bytearray(p.read_bytes());b[-2]^=0x40;p.write_bytes(b)
PY
"$TMP/restart" reject
cp "$TMP/backup" "$SAVE"
python3 - "$SAVE" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);p.write_bytes(p.read_bytes()[:100])
PY
"$TMP/restart" reject
printf 'S44 durable Linux quicksave restart and corruption checks: PASS (%s)\n' "$CC"
