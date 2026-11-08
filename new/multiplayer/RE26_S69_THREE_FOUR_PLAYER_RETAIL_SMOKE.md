# RE26 / Sweep 69 — Retail-ROM 3- and 4-player smoke

## Scope

Adds the opt-in experimental command-line modes `--mp-walktest-3`, `--mp-walktest-4`, `--mp-combattest-3`, `--mp-combattest-4`, preserving the original `-2` modes and the normal single-player executable. This is **not** a cooperative campaign or an enabled Start Co-op menu entry.

The same single-PC-window session/renderer handles 2–4 same-species participants in the already loaded original retail level. All participants receive independently captured actor, weapon, HUD, source renderer, and input state. The original `SafePos` collision routine checks the *distinct* authored interior cells for the additional players; P2/P3/P4 never share the same spawn coordinate. The spatial occupancy check is an experimental multiplayer start arrangement, not a retail multiplayer spawn mechanic. The underlying world is still updated **once** per level; the optional combat mode advances each player's original weapon script separately. No extra operating-system process or second game window is used.

All species are still constrained to the currently initialized species. Mixed-species profiles configured in F12 do not override the experimental source-clone requirement. The original single-player path remains unchanged. The experimental co-op AMP target policy is unchanged from S67 and is **not** considered final.

## Run

```sh
./avp_pc --rom /path/to/your/AVP.jag --diagnostic --mp-walktest-3
./avp_pc --rom /path/to/your/AVP.jag --diagnostic --mp-walktest-4
./avp_pc --rom /path/to/your/AVP.jag --diagnostic --mp-combattest-4
```

`--diagnostic` skips the retail title, but the supplied **original ROM** still provides level data, sprites, audio, and HUD; it does **not** use diagnostic geometry when a retail ROM is provided. Configure independent inputs in F12 -> Controls -> Local Players first; secondary players have **no default keyboard bindings**. To reproduce the automated tests, a temporary `settings.cfg` assigns P2 Forward=`i`, P3 Forward=`t`, P4 Forward=`o`, P3 Fire=`g`, P4 Fire=`h`. F12 -> Miscellaneous -> Marine Start: Shotgun + Full Ammo applies to all newly spawned Marines when enabled.

## Actual retail-ROM observations

- 3-player original-world run: three independent original positions, 100+ ticks, no reported session fault.
- 4-player original-world run: four independent original positions, 100+ ticks, no reported session fault.
- Both 3- and 4-player **one-window** rasterized viewport captures contain nonblack original map and Marine HUD pixels. Both screenshots are actual captured retail-ROM runs, not synthetic dummy test frames.
- Independent P3 and P4 forward keys visibly move **their own** camera while previously untouched participant views remain pixel-identical. After releasing a key, an earlier actor can retain **original momentum**; the regression tests therefore test descending player slots and only assert still-untouched slots remain unchanged.
- Original AMP/weapon combat probes continue for 3/4 actors in the same world. At tick 20, the four-player combat test reports 80 weapon updates (4 per tick) while AMP modes advance once through the shared world.
- Optional full-shotgun start verifies Player 3 / 4 consumes their own shotgun ammo while other Marines retain 44 shells. This verifies independent weapon execution and ammo, **not** actual enemy damage or hit parity.
- Previous S68 two-player retail walktest and shotgun-fire smoke still pass.

## Validation

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build --target avp_pc
python3 new/multiplayer/tests/re26_retail_multi_view_smoke.py --exe build/avp_pc --rom /path/to/AVP.jag --players 3
python3 new/multiplayer/tests/re26_retail_multi_view_smoke.py --exe build/avp_pc --rom /path/to/AVP.jag --players 4
python3 new/multiplayer/tests/re26_retail_multi_view_smoke.py --exe build/avp_pc --rom /path/to/AVP.jag --players 4 --combat
```

Linux GCC and Clang strict builds, build with multiplayer OFF, existing original-engine tests, previous 2P retail smoke, and focused ASan/UBSan tests also pass. Windows builds and physical 3–4 controller/mouse testing remain unverified.

## Remaining limitations

This is not four-player **co-op completion**. All participants currently start as clones of the original currently initialized species; no independently mixed Marine/Alien/Predator start state. Player/player collision, target eligibility rules and comprehensive AI combat, damage/hit parity, death/respawn, computers, level transfers, checkpoints, multi-world live initialization, independent audio, and Windows physical mouse capture remain unverified or unfinished. The 3/4 viewport captures also have large dark triangular areas from the existing underlying rendering path; this may represent the scene/occlusion implementation and is **not** proof of complete rendering fidelity. Start Co-op stays disabled pending actual campaign testing and parity.
