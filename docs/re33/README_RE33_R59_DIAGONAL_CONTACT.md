# RE #33 R59 — Normalize Diagonal Contact Range

Base: exact **RE #33 R58** complete source.

## Enhancements menu

F12 → Enhancements → **Normalize Diagonal Contact Range**: `Off (Jaguar)` / `On`.

- Off **by default**, preserving the source's original square X/Y proximity
  comparisons and original contact/attack behavior. Persisted as
  `enhancement.normalize_diagonal_contact_range=true/false` in `settings.cfg`.
- On changes *harmful enemy contact* range to a circular/Euclidean 2D check
  with the same straight-line reach as the Jaguar's original square comparator.
  The circle removes the square's longer diagonal-corner contact reach.
- Implemented using unsigned 64-bit fixed-point squared distances: no floating
  point, square-root, angle approximation, or gameplay-rate change.

## What the enhancement changes

- Enemy Alien / Predator / crawling Alien / Facehugger proximity for starting
  a close attack, and Alien / Predator melee **hit-frame contact**.
- The Alien Queen's close-attack selection and damaging event range.
- Facehugger latch *acquisition* distance (not its animation or damage once
  an already-started latch finishes).
- Acid puddle proximity damage and cocoon close-contact checks.
- Enemy disc, bolt, and flame projectile contact proximity to players.
- Enabled in single player and original native multiplayer actor scopes, so
  enemy contact geometry applies to whichever local human/bot is targeted.

## What does not change

- Rotating the player does **not** resize the original collision geometry.
  The original square was always aligned to world X/Y axes.
- Original actor long-range awareness, seeking, wayfinding, line of sight,
  solid walls, doors, player movement and world collision are unchanged.
- Original attacks, health/damage amounts, animation sequences, random choices,
  tick rates, and weapon mechanics remain unchanged.
- The original game contact comparators execute unchanged when the setting is
  Off. Only the opt-in PC host condition selects circular distance.
- R51 Guerilla bot tuning and all prior RE33 releases' existing features are
  preserved.

## Technical notes

`src/game/amp.c` is the PC decompilation's authoritative enemy-side contact
unit, so this opt-in requires a small guarded change **inside that C file**.
This release does *not* claim that all Jaguar-derived source bytes remain
unchanged. The Off branch preserves the previous calculation and behavior.

The closest straight-line contact reach remains `pixels << 9` fixed-point
units. For range 60, the original diagonal corner is about 84.85 units from
centre; normalization limits contact to 60 units in every direction.

## Verification

- Strict optimized GCC and Clang multiplayer-enabled executables build.
- `test_amp.c` verifies diagonal-branch Off/On parity, exact axial boundary,
  Pythagorean boundary, and Facehugger pre-latch acquisition.
- `test_normalize_diagonal_contact_menu_r59.c` checks default Off, menu row,
  toggle and saved settings reload under GCC and Clang.
- Core `test_amp.c` passes Clang AddressSanitizer + UndefinedBehaviorSanitizer.
- Existing GCC/Clang native AMP, local multiplayer bot/navigation/awareness,
  Deathmatch, Marine sprite, audio, Map Maker 841, AMP Map Maker 79, and door-flow
  42 regressions pass.
- Interactive retail ROM verification and Windows/MinGW compilation remain
  outstanding. A retail ROM is not part of this release.
