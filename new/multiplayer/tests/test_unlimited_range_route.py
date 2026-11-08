#!/usr/bin/env python3
"""RE31 R17 regression: F2 weapon ticks must read the live F12 range setting.

The earlier original-weapon unit test called the MAZESCRN setter directly. That
passed while F2 sessions never ran pc_main's single-player settings update.
This guard checks the actual native actor tick site in addition to the C test.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[3]
main = (root / "src/platform/pc/pc_main.c").read_text()
walk = (root / "new/multiplayer/local_mp_walktest.c").read_text()

assert main.index("multiplayer_dispatch:") < main.index(
    "avp_mazescrn_set_unlimited_shooting_range(")
start = walk.index("static int walk_post_actor(")
end = walk.index("static ", start + len("static int walk_post_actor("))
tick = walk[start:end]
assert tick.count("avp_pc_video_unlimited_shooting_range(t->video)") == 1
assert tick.index("avp_mazescrn_set_unlimited_shooting_range(") < tick.index(
    "UpdtScrOverlays();")
assert tick.index("avp_mazescrn_set_unlimited_shooting_range(") > tick.index(
    "t->dm_shooter=player;")
print("PASS: every F2 native weapon update reads F12 Unlimited Shooting Range")
