# RE #33 R65 — Clear Enemies From Level

Base: the exact RE #33 R64 complete source archive.

## Menu and scope

F12 -> Cheats -> New -> **Clear Enemies From Level : Press Enter**.
This is a one-shot action, **not** a toggle. Left/Right cannot execute it.
The existing Cheats -> Activate For selector determines P1/P2/P3/P4; only
that player's **current level** is cleared in local multiplayer. Other loaded
levels and the other players' worlds are untouched. If several players are
on that same level, they share the cleared world (as usual).

## Direct removal, not killing

`avp_amp_dev_clear_enemies()` immediately frees hostile actor records in
the bound Jaguar world: ordinary Aliens, Crawling Aliens, enemy Marines,
Predators, Facehuggers, their eggs, generators, generator shields, and the
**Alien Queen**. Dead enemy records belonging to the hostile types are
also removed. No weapon hit, kill routine, score award, explosion, corpse,
acid-death effect, boss-death event or campaign-completion callback runs.
Generator-related alarm sound is stopped when generators are removed.
Enemy Marine attack loops are stopped after removing the enemies.

Collision/collectable grid entries are erased only if their one-based AMP
owner is one of the removed actors; other actor cells, geometry and pickups
remain intact. In particular, **AC_GAS is flamethrower ammunition**, not an
explosive barrel, and is never removed by this cheat. Explosive DRUM props
and all player actors are also preserved. Slot bookkeeping is reused as-is.

On saving/leaving the level, original `ram_save()` omits released enemies;
they do not become death-state save records. Original campaign logic is
unchanged without the cheat.

**Queen-specific note:** Because removal deliberately bypasses her authored
death/ending script, clearing the Queen may also bypass the campaign progress
that normally requires defeating her. This is intentional for a developer
cheat and is not an alternative boss win condition.

## Production changed paths

- `src/game/amp.c`, `include/amp.h`: optional explicit removal helper.
- `include/pc/pc_video.h`, `src/platform/pc/pc_video_x11.c`: menu/action routing.
- `src/platform/pc/pc_main.c`: single-player current level.
- `new/multiplayer/local_mp_walktest.c`: action in selected player's bound world.

Tests:
- `tests/test_amp_clear_enemies_r65.c`: Queen, spawners, pickup/actor safety,
  collision/objmap ownership, no death/progression, allocator reuse.
- `tests/test_pc_show_fps_menu.c`: actual F12 routing for P1 and P3.
- CMake adds a strict-test target.

## Verification limits

GCC/Clang strict optimized Linux builds and regression tests passed. Clang
AddressSanitizer/UndefinedBehaviorSanitizer focused test passed.
Windows cross-compilation and interactive ROM gameplay were not performed.
The retail ROM is *not* redistributed.
