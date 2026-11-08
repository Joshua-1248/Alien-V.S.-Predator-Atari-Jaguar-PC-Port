# RE #33 R54 — Inline Activate For selector

Base: RE #33 R53 (including ambient hissing R52 and Jaguar-era cheats R53).

F12 -> Cheats now contains:
- Original >
- New >
- Activate For : < Player 1 >

**Left / Right** (keyboard or gamepad menu direction) immediately cycles Player 1 -> Player 2 -> Player 3 -> Player 4 -> Player 1, and reverses with Left.
**Enter** on Activate For does not open a submenu or change the player.
The selected player affects the same Original/New cheat actions as in R53.
The selector remains session-only and starts at Player 1.

Only host menu code and its focused F12 regression test have changed since R53.
All original Jaguar `src/game/` files, R51 bot AI, R52 audio, R53 cheat handling, and settings remain unchanged.
Neither the retail ROM nor prototypes are redistributed.

Linux builds:
- `LINUX/avp_pc` (GCC)
- `LINUX/avp_pc_clang` (Clang)
Both executables are multiplayer-enabled. Unix execution permissions are 0755 in the ZIP and TAR packaging.

Verification: GCC and Clang strict Release builds; F12 menu tests (Enter no-op, Left/Right, reverse, wrap, selected player cheat isolation); native bot/level/awareness/Deathmatch tests; audio; Map Maker 841; AMP playtest 79; native door 42. All pass.
Windows cross-build and live in-game visual confirmation were not performed here.
