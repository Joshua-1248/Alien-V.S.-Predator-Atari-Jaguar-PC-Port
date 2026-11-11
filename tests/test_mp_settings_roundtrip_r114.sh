#!/usr/bin/env bash
# R114: actual X11 startup + settings.cfg load/write regression.
# Usage: tests/test_mp_settings_roundtrip_r114.sh <avp_pc executable> <owned AVP.jag>
set -euo pipefail
[[ $# == 2 ]] || { echo "Usage: $0 <avp_pc> <AVP.jag>" >&2; exit 2; }
exe=$(realpath "$1")
rom=$(realpath "$2")
command -v xvfb-run >/dev/null || { echo "Requires xvfb-run" >&2; exit 2; }
command -v timeout >/dev/null || { echo "Requires timeout" >&2; exit 2; }
base=$(mktemp -d)
trap 'rm -rf "$base"' EXIT
cp "$exe" "$base/avp_pc"
cat > "$base/settings.cfg" <<'CFG'
local_multiplayer.enabled=true
local_multiplayer.players=3
local_multiplayer.player_collisions=false
local_multiplayer.show_other_players_on_automap=false
local_multiplayer.three_layout=1
local_multiplayer.bot_count=5
local_multiplayer.dm_cloak_damage=true
local_multiplayer.input.p2.cheat_no_facehug=1
local_multiplayer.input.p3.cheat_no_enemy_attack=1
local_multiplayer.input.p4.cheat_reveal_map=1
CFG
( cd "$base" && timeout 5s xvfb-run -a ./avp_pc --rom "$rom" > run.log 2>&1 ) || rc=$?
[[ ${rc:-0} == 0 || ${rc:-0} == 124 ]] || { cat "$base/run.log"; exit 1; }
for line in \
 'local_multiplayer.player_collisions=false' \
 'local_multiplayer.show_other_players_on_automap=false' \
 'local_multiplayer.three_layout=1' \
 'local_multiplayer.bot_count=5' \
 'local_multiplayer.dm_cloak_damage=true' \
 'local_multiplayer.input.p2.cheat_no_facehug=1' \
 'local_multiplayer.input.p3.cheat_no_enemy_attack=1' \
 'local_multiplayer.input.p4.cheat_reveal_map=0'; do
 grep -qxF "$line" "$base/settings.cfg" || { echo "FAIL: $line" >&2; exit 1; }
done
printf '%s\n' 'PASS: real frontend settings roundtrip; P1-P4 reveal cheat resets at startup'
