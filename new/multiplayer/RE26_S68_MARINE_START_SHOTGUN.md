# RE26 / Sweep 68 — optional Marine shotgun at new-game start

## User-facing behavior

F12 -> Miscellaneous -> **Marine Start: Shotgun + Full Ammo**. Default **Disabled**; persisted as `misc.marine_start_shotgun=true|false` in the executable's existing `settings.cfg` (not added as a cheat). Option is shared by single-player and opt-in local multiplayer. Takes effect on **new Marine initialization**, not retroactively in an already-running session.

- Applies **after** `InitScrOverlays()` -> `Init1stOvers()` has initialized Jaguar weapon ownership/ammo: adds original bit `1<<1` (shotgun) and sets `ammo_info[0].cur=ammo_info[0].max` (44 shells). Leaves slot 1 pending for the *original* weapon draw/animation path; doesn't synthesize weapon frames, damage, sounds, or new ammunition types.
- Not applied when a game is loaded from an existing Jaguar save (`savegame` is nonzero at the post-initialization hook). Doesn't alter the save/load representation, other weapon slots, or the other species.
- No-ROM diagnostic Marine initialization and the developer *new actor* species switch honor the same setting.
- Current S67 two-player walktest captures original initialized P1 and copies the resulting actor state to independent P1/P2 snapshots; each has its own `ammo_info[0].cur`, so firing one does not consume the other's ammunition. Mixed-species / 3–4-player playable co-op is still unfinished.

## Source edits

- `src/platform/pc/pc_video_x11.c`: F12 Misc item 160, persistence and getter. Prior IDs 0..159 untouched. `AVP_MENU_ITEMS` is now 161. Compiles with the local multiplayer feature ON or OFF.
- `src/platform/pc/pc_main.c`: new Marine initialization hooks after original weapon setup; original game routines remain unchanged.
- `include/pc/pc_marine_start.h`: small, explicit host-only loadout application. Does not depend on global actor state; can be unit tested directly.
- `include/pc/pc_video.h`: preference getter.
- `new/multiplayer/tests/test_pc_marine_start_shotgun.c`: default off, saved game unchanged, Alien/Predator excluded, max ammo, P1/P2 independent copy.
- `new/multiplayer/tests/re26_retail_two_view_smoke.py`: optional `--marine-shotgun` (requires `--combat`) injects the preference into a disposable settings file and checks real ROM weapon ownership and ammunition consumption. Existing smoke's defaults unchanged.

## Verified

- Strict GCC and Clang executable builds; strict local multiplayer disabled build.
- 19 multiplayer test executables PASS (18 existing + one new), including original Jaguar retail ROM data/collision tests, native AMP/actor/world tests, and one-window X11 pixel-readback.
- Live original retail ROM (`AVP.jag`, user-owned and **not included** in deliverables), level 1: P1 and P2 independently display `weapon=1 ammo0=44` at tick 1; during held P2 fire tick 40 `P1 ammo0=44`, `P2 ammo0=43`; tick 60 `P1 ammo0=44`, `P2 ammo0=41`. P2 weapon scripts fired, shared AMP modes advanced; no enemy hit asserted.
- Linux/Xvfb visual smoke `--combat --marine-shotgun`: PASS, two independent retail views in one window.
- The option does **not** enable unfinished cooperative campaigns; Start Co-op remains disabled. Windows cross-build/live runtime not exercised here.

## Linux builds

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build --target avp_pc test_pc_marine_start_shotgun
./build/new/multiplayer/test_pc_marine_start_shotgun
python3 new/multiplayer/tests/re26_retail_two_view_smoke.py --exe build/avp_pc --rom /path/to/AVP.jag --combat --marine-shotgun
```

For manual new-game testing: launch the PC executable, press F12, select **Miscellaneous**, set **Marine Start: Shotgun + Full Ammo** to **Enabled**, then begin a new Marine game. The toggle survives restart. It does not grant weapons mid-game (which would contradict the requested *starting* loadout).
