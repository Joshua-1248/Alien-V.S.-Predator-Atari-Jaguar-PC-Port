"""PC-only integration at the existing R399 native damage assignments."""
import re
SITES = {"src/game/amp.c": 10, "src/game/collision.c": 1, "src/game/mazescrn.c": 1}
PATTERN = re.compile(r"player_energy=(amp_word_add\(player_energy,[^;]+\)|\(s16\)\(player_energy-(?:damage|10)\));")
def apply_damage_gate(path, data):
    if path not in SITES: return data
    text = data.decode("utf-8")
    text, count = PATTERN.subn(r"player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,\1);", text)
    if count != SITES[path]: raise ValueError(f"Unexpected native damage sites: {path}: {count}")
    return ('#include "../../new/cheats/damage_gate.h"\n'+text).encode("utf-8")
