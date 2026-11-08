# RE #33 R69 — F2 Backspace and Native-Width Diagonal Melee AI

Base: the exact complete RE33 R68 source. Original Atari Jaguar gameplay source files are unchanged.

## F2 menu Backspace

Physical Backspace invokes the existing F2 `mp_setup_back()` handler, in addition to Escape and the configurable Back binding. In Bots, Modifiers, and Options submenus it returns to the parent F2 menu; at the F2 root it closes F2 without starting play. Input is consumed with the existing F2 key buffer-clearing behavior. Hints include Backspace, and the root still documents Left/Right adjustment.

## Bot diagonal melee eligibility

Original `src/game/mazescrn.c` melee definitions all have `distance=0x90` and `width=20`. Original `src/game/player.c::TestSpark` builds a signed X/Y shot bounding rectangle widened by `fire_width*512`, then checks a perpendicular aim offset and forward facing. The previous PC bot code used only Euclidean distance <= `0x90<<8` (0.5625 tile) for Alien/Predator melee Fire decisions. That circular gate is narrower than the true authored shot geometry on diagonal headings, leaving bots passive at some legitimate attack positions.

The PC bot decision now mirrors the original finite shot bounding-box and perpendicular-width eligibility, without modifying `TestSpark`, original damage, collision, motion, animation timing, weapon availability, bot awareness, or A* route/navigation. Visible facing, Predator cloak and combat behavior gates are preserved. All original weapon hit results remain authoritative.

The focused `test_local_mp_bot` regression includes all four 45-degree quadrants x Alien/Predator x Balanced/Berserker/Guerilla (24 reachable diagonal encounters), rejects targets beyond the original shot box, rejects unseen targets, and retains the previous melee boundary, approach/retreat, cloak, cocoon, hazard, and target-selection tests. It intentionally fails against unmodified R68 and passes with R69.

## Verification

- GCC and Clang optimized strict Release (multiplayer enabled): full build PASS.
- `test_local_mp_bot`: PASS on each compiler; Clang AddressSanitizer/UndefinedBehaviorSanitizer PASS.
- Bot awareness, bot levels, local multiplayer actor, Deathmatch, Queen, hazard/enemy clear, R60 flame projectile, R68 Debug refresh, title attract: PASS on both.
- Map Maker 841 PASS, AMP playtest 79 PASS, native door flow 42 PASS on both.
- Retail-ROM/Xvfb real F2 keyboard/navigation test: Bots / Modifiers / Options Backspace -> root and root Backspace -> close, PASS with GCC and Clang executables.
- Patch checked against pristine R68 source, with byte comparisons of every modified/added file.
- Linux executables are GCC `LINUX/avp_pc` and Clang `LINUX/avp_pc_clang` (both 0755 in ZIP and TAR.GZ). No multiplayer-disabled binaries or copyrighted Jaguar ROM included.

### Outstanding

Actual hands-on combat in the retail-ROM levels, and a Windows/MinGW build, have not been verified. The diagonal melee test is an original-source geometry and simulated bot observation regression; it does not replace a live game playtest.
