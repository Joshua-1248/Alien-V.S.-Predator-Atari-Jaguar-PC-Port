# AVP (Atari Jaguar) PC port — RE #33 R56

Base: **RE #33 R55**. This is a focused local multiplayer F2 menu reorganization and third-person Alien animation cadence adjustment.

## Multiplayer setup menu

The F2 **Local Multiplayer** root contains, in order:

1. Number of Players
2. **Number of Bots** (formerly Standalone Bot Count)
3. Campaign; visible Player 1–4 characters; split-screen layout; campaign-start respawn in co-op
4. Game Mode; Deathmatch Score Limit, Time Limit and Level
5. **Bots** — press Enter
6. **Modifiers** — press Enter
7. **Options** — press Enter
8. Start Game

**Bots** contains the four human-slot AI-controlled toggles and, for each configured standalone bot, its Character and Behavior. **Number of Bots** stays on the main page directly below Number of Players as requested.

**Modifiers** contains:
- Modifier: Character Change on Death For Random Character Players (wrapped over two lines for readability)
- Modifier: Fast Movement (Deathmatch)
- Modifier: Fast Equip (Deathmatch)
- Cloaked Predator Takes 4x Damage (Deathmatch)

**Options** contains:
- Show Other Players On Automap
- Show Whole Map (was Modifier: Show Whole Map)
- Players Can Move Between Levels (Deathmatch)
- Enemies: On/Off (was Enemy AI; presence, not brain logic; Deathmatch)

Use Up/Down to select, Left/Right to change a value, Enter to enter a submenu or start the game, and Escape to go back. Escape from the root closes F2. F2 closes or opens the menu directly. Configured controller accept/back bindings also enter/go back.

The **existing row IDs 0–49, settings serialization, gameplay flags, bot behavior, bot counts, and all underlying adjustment functions remain unchanged**. Only UI-only submenu IDs (50–52) and ordered menu display navigation are new. Existing Deathmatch-only options remain Deathmatch-only.

## Alien third-person attack animation

Original enemy source: `src/game/amp.c`, `fight_bite`, `fight_swipe`, `fight_tail`, `amp_alien_clfight`, and `amp_handalien`.

Each of the Alien player's original bite, swipe/claw, and tail sprite playlists ALREADY matched the enemy source frame-by-frame at one frame per native 10-Hz tick (6, 6, and 8 ticks). The observed difference was in **consecutive held-button attack animations**: the player presentation could begin a new loop immediately after a sequence ended, whereas enemy Aliens wait five native ticks in `amp_handalien` before attacking again.

R56 applies the original five-tick recovery **only to repeated held-fire visual animations**. Explicit new button presses still animate without delay. There are no changes to `PLAYER.S`/`amp.c` native attack commands, damage, hitboxes, physics, audio or game ticks. The change is per actor and presentation-only, and applies to both human-controlled and bot-controlled Alien sprites.

## Verification / scope

- Strict GCC and Clang Release builds with local multiplayer enabled.
- `test_local_multiplayer_setup`: root order, visibility, submenu rows, Deathmatch-only filtering, stable settings and 12 bots.
- `test_local_mp_sprite_pose`: exact original weapon frames and Alien held-fire recovery.
- R51 bot tactics, bot awareness/levels, Deathmatch, native actor/player/doors, PC audio, F12, Map Maker (841 cases), AMP playtest (79 cases), door flow (42 cases) pass GCC and Clang.
- Original 33 `src/game/` files remain byte-for-byte identical to R55.
- Retail ROM loads to title under Xvfb (audio device unavailable in container). Actual interactive visual judgment at high refresh and Windows cross-compilation are not verified.

Distribution retains both Linux executables (`LINUX/avp_pc` from GCC and `LINUX/avp_pc_clang` from Clang) with 0755 executable permissions in both ZIP and TAR.GZ. No ROM is redistributed.
