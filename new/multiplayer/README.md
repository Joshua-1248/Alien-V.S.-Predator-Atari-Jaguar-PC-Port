# Optional PC-only local multiplayer (foundation, Sweep 50)

**Status:** Setup menu and data-model foundation; **NOT yet a playable multiplayer mode**.

The retail Jaguar gameplay, original title graphics, original menus, frame scheduling,
AI, weapons, resources and saves are unchanged. This folder contains the new feature's
source. Only the existing PC host window/menu file and root CMake file contain small
#ifdef/build integration points; `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` compiles the legacy
PC executable without this extension. When compiled in, the runtime setting defaults OFF.

## Current user-facing controls

1. Press **F12 -> Miscellaneous -> Local Multiplayer (PC extension)** to enable.
2. On the original title, a host-only **F2: Local Multiplayer >** entry appears.
3. F2 opens the setup overlay. Choose **1-4 players**, independent **campaign**
   (Marine, Alien, Predator), species for each active player, and two-player
   **Side by Side / Top and Bottom** layout. Three and four players use quadrants.
4. **Esc/F2** returns to the original title. The UI makes no attempt to start
   co-op. It displays a clear **SETUP ONLY** message.
5. Choices persist in the existing `settings.cfg` as `local_multiplayer.*`. The
   multiplayer feature defaults disabled for new and old settings files.

The original title's ROM-derived pixels are never altered. The host UI is rendered
after the original game framebuffer; all setup-input events are consumed before
being passed to the original Jaguar input path. The title behaves normally when
multiplayer is disabled.

## Core data contracts

`local_mp.h/.c` do **not** include a game header or access an original game global.
They provide: bounded setup values, species/campaign independent selections,
viewport rectangles, and a proposed campaign+level grouping for independent levels.
Two players on the same level can share a future world instance. This is only a
**registry calculation**; no additional world is instantiated or simulated yet.

This is not a network feature. No netcode, matchmaking or sockets.

## Preserve the original source

Subsequent tasks need independently ticked game worlds and separate player/AI,
maze, level-resource, renderer, palette, sound and save state. Never use a memcpy
of the quicksave area as the runtime isolation strategy. Do not claim the presence
of the setup/UI means 2-4 simulations are already running.

Proposed next milestones:

1. Inventory the mutable game state and non-shareable renderer/audio state.
2. Implement isolated level simulation containers under `new/multiplayer/worlds/`.
3. Make separately controllable player actors, each with current level, species,
   inventory, health, weapons and campaign mission state.
4. Share a world (and enemies, doors, interactions) for players in the same level;
   keep different levels ticking simultaneously.
5. Add a viewport compositor and shared-level sprite actor renderer. Marine uses
   available directional sprites; Alien and Predator billboards face each viewport
   independently. Inventory original assets before assigning animation frames.
6. Enable **Start Co-op** only after all of the above work and regressions pass.

## Verification

```
cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=OFF
cmake --build build-gcc --target avp_pc test_local_multiplayer_setup --parallel 4
build-gcc/new/multiplayer/test_local_multiplayer_setup
```

Run an unmodified baseline with `-DAVP_BUILD_LOCAL_MULTIPLAYER=OFF`.
The `tests/smoke_x11.py` script optionally checks F12/F2 and persistence using a
user-owned retail ROM, Xvfb, and XTEST; it distributes no copyrighted assets.

## Sweep 51: independent local-player controls and physical mice

After enabling **F12 -> Miscellaneous -> Local Multiplayer**, select **F12 ->
Controls -> Local Players (2-4)**. The original Player 1 controls remain on the
normal F12 Controls pages. On the separate page:

- Change **Configure Player** to Player 2, 3 or 4. Player 1 is available
  *only for its optional dedicated multiplayer physical-mouse assignment*;
  its existing keyboard/gamepad/mouse actions remain on the original Controls
  pages.
- Select **Binding Device Type**: Keyboard, Mouse, or Gamepad. On Keyboard,
  highlight an action and press **Enter** followed by a key to bind it; Esc
  cancels. On Mouse/Gamepad, use left/right to select a button number. Delete
  clears an action's binding. Multiple gamepad axes, inversions and deadzone,
  mouse sensitivity and acceleration have independent per-player values.
- Select an individual **Physical Mouse** by its stable Linux
  `/dev/input/by-id/*-event-mouse` or `/dev/input/by-path/*-event-mouse` path.
  Two profiles may **not** be assigned the same physical device (even under
  distinct symlinks). The same principle will apply to controllers once input
  dispatch is live. Assigning a path does not enable simultaneous gameplay.
- Configuration persists as `local_multiplayer.input.p2.*` through `.p4.*`
  in `settings.cfg`. There is no hidden overwrite of P1's legacy settings.

`local_mp_input.*` provides explicit, per-player keyboard/mouse/gamepad action
maps and a hardware-independent mouse-event router. `local_mp_mice.*` discovers
physical mice separately from the merged X11 cursor. The Linux backend uses
non-blocking EV_REL/EV_KEY reads from the selected evdev node; this typically
requires read permission on `/dev/input/event*`. The device can be inaccessible
on a stock Linux desktop without appropriate udev/logind permissions. Never
recommend making the event devices globally writable. No root is required for
configuring controls; raw reader opening fails cleanly without permission.

**Not connected to live co-op yet:** The independent simulation, viewport
actors and per-player input-consumption stage are not implemented. We do not
steal or filter the current merged X11 cursor while single-player is running.
Therefore using a second physical mouse does **not yet** control Player 2.
When live input integration is added, the legacy aggregate pointer input must
be suppressed for assigned devices, so P2's mouse cannot also move P1.
On Windows, Raw Input device enumeration is scaffolded, but WM_INPUT forwarding
and simultaneous device event delivery are **not** implemented or tested.

The physical mouse router and keyboard profile serialization are tested with
synthetic separate-device events; the F12 key binding and settings persistence
were also verified in an actual Linux/X11 window with the retail ROM. No
second physical mouse was present in that test environment. Compile-out with
`-DAVP_BUILD_LOCAL_MULTIPLAYER=OFF` remains supported.

## Sweep 52: complete-frame split-screen compositor (isolated)

`local_mp_compositor.h/.c` implements `avp_local_mp_compose`. Pass **one separately
rendered, fully composited** XRGB32 frame for each active player and a destination
host framebuffer. That source MUST already contain the original single-player
HUD (health, ammunition, tracker, weapon, messages and species overlays). The
compositor scales the entire source, not separate elements, so side-by-side,
top/bottom and quadrant layouts naturally stretch/narrow the original graphics
according to viewport dimensions. It does not introduce a redesigned HUD.

The existing `avp_local_mp_viewports` computes pixel-exact regions, including
odd window dimensions; three players occupy three quadrants with a cleared
fourth. The full source image edges, notably the bottom HUD scanline, are
preserved even if the output region is smaller than the source. The scaling
is nearest-neighbor with independent horizontal/vertical factors. No
aspect-ratio letterboxing is added within a player viewport.

**Important limitation:** this compositor takes real per-player source frames;
it does **not** itself create those frames, run simulations, allocate player
actors or present a split-screen frame during gameplay. The user-facing F2
menu still explicitly says SETUP ONLY. Never feed the same single-player
frame to every viewport and claim that is playable co-op.

Isolation: No original gameplay, HUD, video, renderer or audio files were
modified by Sweep 52. The module builds with the existing optional
`AVP_BUILD_LOCAL_MULTIPLAYER` flag; `OFF` excludes it from the game. Separate
player-level rendering, per-world simulation and input delivery remain open.

Test:

```
cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON
cmake --build build-gcc --target test_local_multiplayer_compositor --parallel 4
build-gcc/new/multiplayer/test_local_multiplayer_compositor
```

The test verifies one through four players, both two-player orientations,
per-player world/HUD pixels, odd sizes, output pitch protection, blank fourth
quadrant and failure without partially altering destination pixels.

## Sweep 53: independently owned active-world session coordinator (isolated)

New `local_mp_worlds.h/.c` defines a **backend-neutral session**. It owns one
player-state context per player and one world-state context per distinct active
campaign+level. Users can enter the same level (sharing enemies/doors inside
one backend world) or visit separate levels with independent world ticks.

A host calls `avp_mp_session_tick()` at the original 10-Hz simulation cadence;
the coordinator calls the backend **once per active world**, passing the full
member mask and the four player input commands. Player contexts survive level
changes. Entering a new world and attaching the actor succeeds before leaving
the old one. If world creation or actor entry fails, the old world remains
attached. Leaving a shared world does not destroy it while other players are
present. A four-way distinct-level session can move a sole resident into a
fifth level by replacing its vacated slot. A failed backend tick faults the
session to avoid replaying only some of the worlds on the next host frame.

`avp_mp_session_compose()` requests a **different complete frame for each
player**, checks against reusing the same pixel pointer and then calls the
Sweep 52 scaler, preserving the existing HUD. Rendering failure leaves the
host destination untouched. The backend must keep all per-player frame
storage valid until the composite call returns.

**This is a tested scheduling/ownership contract, not yet retail game
simulation.** The new test backend allocates fake level and actor contexts to
verify ownership, transitions and output-frame routing; no production backend
instantiates original Jaguar game globals or produces actual gameplay frames.
The F2 setup continues to be explicitly non-playable. All original source,
menus, controls, saves and renderer implementations remain unchanged.

The original `src/game/player.c`, `amp.c`, `doors.c`, `levels.c`, `maze.c`,
`weapons.c` and the PC rendering/audio runtime still use global or static
singletons, so linking multiple copies into one process is **not** equivalent
to independent worlds. The final production backend must provide *true* state
isolation (e.g. separate isolated simulation contexts with an explicit
state model, or helper processes with carefully defined messages). It must
also implement durable per-level state on unload/revisit, preserve player
progression across levels, and render every mixed-species actor. Copying raw
quicksave data on every frame is prohibited as a substitute.

Build and test:

```
cmake -S . -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON
cmake --build build-gcc --target test_local_multiplayer_worlds --parallel 4
build-gcc/new/multiplayer/test_local_multiplayer_worlds
```

## Sweep 54: one PC window, four independently composed views

The live-session API `avp_pc_video_present_local_mp` in the existing PC host
window now connects `avp_mp_session_compose()` to the **same** X11/Win32
presentation buffer/window used by single-player; no worker process and no
extra window are involved. The original single-player presenter is unchanged
in behavior: it scales the original Jaguar frame before blitting. The new
backend must instead supply *four separate complete frames including original
HUDs*; they are scaled together, then blitted **once**, as one host image.

`tests/test_local_mp_one_window_x11.c` is a real-display, real-pixel readback
smoke test for 1-4 viewport layouts in ONE X11 window (synthetic input frames
only; not original gameplay). The production entry point remains dormant until
independent original-game actors/worlds and separately rendered frames exist.
The Windows version uses the same `pc_video_x11.c` through Win32 Xlib
compatibility; Windows runtime verification is not yet done.

See `AVP_RE25_FULL_SWEEP_54_SINGLE_WINDOW_MULTIPLAYER_PRESENTATION_AUDIT.md`.

## Sweep 55: original retail level-map contexts (no OS-specific workers)

`local_mp_retail_maps.h/.c` owns a verified copy of each of **FILES 87–101**,
all 15 original Jaguar levels. The caller supplies the *unmodified* decoded
retail maze resources and their original file indices, dimensions, and sizes.
The module requires the 8-byte retail header/dimensions and full 8-byte maze
cells; it does not generate maps or infer geometry. It owns independent
mutable copies indexed by `(campaign, level)`, so two players in one level
share the same level bytes, while players on other levels have independent
bytes. Unoccupied levels retain their mutations until the campaign is
explicitly reset. The source copies are immutable and never written to.

This is a **level-resource isolation milestone**, not actual multiplayer
simulation. The live gameplay's original door, AI, player, collision,
weapons and render globals remain singletons and are not yet bound to these
isolated contexts. Changing a cell byte in tests proves storage isolation,
not that door state machines or campaign progression execute correctly. The
host PC gameplay and original Jaguar source files were NOT modified here.

`test_local_mp_retail_maps` uses deterministic authored-shape test bytes to
check same-level sharing, cross-level/campaign isolation, bounds,
refcounting, reentry persistence, reset and failure cleanup. An additional
`test_local_mp_retail_rom /path/to/AVP.jag` uses the **existing original-ROM
resource decoder** to check all fifteen authentic cartridge map records,
byte-for-byte. The ROM is user-owned, required only at test time and never
shipped. Both tests pass GCC and Clang. The standalone original-ROM audit
is built on Linux with function-section GC to omit the game's unrelated
single-instance global bindings. The actual `avp_local_multiplayer` source
uses strictly portable ISO C99 and is included in the Windows PC target.

The next necessary task is a genuinely instanced native player/AI/door/
collision runtime consuming each independent map; until that works, the
F2 start option must remain disabled. One executable, one window and one
composed output remain the target across both Linux and Windows.

## Sweep 56: call the ORIGINAL collision engine on a selected retail map

`local_mp_collision.h/.c` exposes scoped per-world calls to the **existing**
Jaguar collision implementation (`SafePos`, `AllowedMoves`, `NMoves`,
`LineOfSight`, `avp_collision_open_door_ahead`). It checks the leased map,
passes the original decoded cell bytes unaltered, and restores the three
original collision map-binding values after each query. The original door
classification `fullbits` array is initialized through the existing
`InitDoors()` on ordinary startup (also in standalone original-code tests).

**Thread contract:** serialized calls on the original game thread only; do not
call concurrently or reentrantly. This is a small *map-binding* seam, not
player/AI/door simulation isolation or runtime global save-state swapping.
The new calls are not yet wired to multiplayer game ticks; Start Game stays
disabled. Verified GCC+Clang vs original game functions, including 60 sample
positions across all 15 privately decoded retail ROM levels; no ROM shipped.

## Sweep 57: original DOORS.S machine state, per world

`local_mp_doors_native.h/.c/.inc` provides a **PC-only**, synchronous,
original-code door-state capture/restore seam. Exactly one guarded include
was added to the **end** of `src/game/doors.c`, without altering any original
`InitDoors`, `SaveDoors`, `ResetDoors`, `DoorKeys`, or `MoveDoors` statements.
The optional hook is not compiled into an `AVP_BUILD_LOCAL_MULTIPLAYER=OFF`
PC build. The test uses original `SaveDoors`/`ResetDoors`, not a substitute.
Active original-door pointers are validated, stored as map-relative offsets,
and restored into the appropriate live world map; stored door histories for
separate levels cannot overwrite each other. Reentering a world restores its
previous per-world saved door values. A scope always restores the host's
original private door state and its bound maze. Nested entry is rejected.

**Not yet a live door tick backend:** `MoveDoors` consults original AMP and
player globals which are still single-instance. The current test verifies
capturing/restoring active door slots, not actually advancing independent
active doors under multiplayer. Authentic level metadata and player/AMP world
isolation must come first. See the root Sweep 57 audit for exact limitations.

## Sweep 58: original PLAYER.S player-state isolation and PC-link correction

`local_mp_player_native.{h,c,inc}` captures and restores 80 explicitly
listed original player-owned or player-facing globals, including movement
physics state, all PLAYER.S random seeds, health, pain, class, controls,
position/angle, first-person selection numbers and the private PC analog
heading accumulator. It reuses `InitMove`, `TidyMove`, `ResetMove`, and
`avp_random` *unchanged*. Four player slots are verified with serial scopes;
entering one never changes the retained state of another. The original host
snapshot is restored bit-for-bit on leave. Invalid snapshots and nested scopes
are rejected. No game windows, threads, workers or online interfaces are added.

**Important integration fix:** the actual PC executable links `avp_pc_core`,
not `avp_c`; both original optional `doors.c` and new `player.c` hooks are now
compiled into **both** libraries when `AVP_BUILD_LOCAL_MULTIPLAYER=ON`. The
Sweep 57 `avp_c`-only door macro did not activate the seam in the actual PC
binary. GCC/Clang `nm` inspection now proves both native capture APIs exist
in `avp_pc`. `AVP_BUILD_LOCAL_MULTIPLAYER=OFF` excludes both hooks.

**Limitations:** this snapshot is NOT a complete gameplay world. It does not
cover `mazescrn.c` weapon animation/private state, `hud.c`, inventory,
`amp.c` objects and AI, `joyed.c` pad sampler, sound handles or full
renderer/level ownership. Position/orientation and selector/input register
values are included solely because original player routines consult them.
`maze_width` must be bound from the active world separately. The `MovePlayer`
function pointer is preserved but actual `MovePlayer`/`UpdatePlayer` are
not called by the isolation test; doing so now would mix unbound AMP,
weapon and renderer state across players. This is neither live independent
movement nor playable co-op. Start multiplayer remains disabled.

One-window compositor, input controls and existing single-player paths
remain unchanged. The source-only patch is replayable against Sweep 57.

## Sweep 59 — Original first-person weapon state and combined actor transaction

The native `MAZESCRN.S` weapon-script state (including original command stream
pointers and six ammo records) and private Predator weapon unlock index can now
be captured/restored without copying host screen buffers or callbacks. Original
`player.c` and first-person weapon state must be scoped **together** through
`AvpMpActorNativeScope`: they share four original weapon-selection globals and
nested individual scopes would corrupt the captured state. The native test
executes original `new_weapon()` / `update_wep()` and validates all four slots;
this is **not** connected to playable multiplayer or safe original AMP combat.
See `AVP_RE25_FULL_SWEEP_59_ORIGINAL_WEAPON_AND_ACTOR_CONTEXT_AUDIT.md`.

### Sweep 59 AMP.S original world state

`local_mp_amp_native.h/.c/.inc` additionally preserve the original actor pool,
original AMP allocator and saved-placements state and active object/collision
grids for separate worlds in the same process. The real original allocator and
`ram_save()` path are exercised by `test_local_mp_amp_native`. A snapshot may
only be captured between *complete* AMP ticks. This does **not** make original
enemy AI safe to run with several player targets or remove non-world game
globals; it is not installed into live multiplayer runtime.

## RE26: original-game world+actor native bridge

See `RE26_NATIVE_CONTEXT_AUDIT.md`. The new `local_mp_native_context` binds
original AMP, DOORS, collision and maze dimensions around individually scoped
original player/weapon actors. A small `local_mp_native_input` converts local
per-player commands into original Jaguar pad state. Verified with actual
original engine units in `test_local_mp_native_context`. **Still not playable
co-op:** the live game loop, full world dependencies, multi-mouse Windows/Linux
polling, original per-player rendering and shared target interactions are
NOT yet connected. Keep F2 Start Co-op gated.

## RE26 S61: independent host input + native backend session

See `RE26_S61_INPUT_AND_NATIVE_SESSION_ADDENDUM.md`. Host input now has an
opt-in per-tick command collection API, independently polled P2-4 joystick
readers, and a combined original player/weapon/world `AvpMpWorldBackend`
adapter. A mandatory Player 1 legacy-input callback prevents rewriting the
normal single-player bindings. An original, live game-loop/co-op renderer is
**not yet present**, so the F2 Start Co-op gate intentionally remains closed.

### RE26 Sweep 68 — Marine starting shotgun

A shared single-player/multiplayer PC preference is available at **F12 -> Miscellaneous -> Marine Start: Shotgun + Full Ammo**. Disabled by default. On a new Marine start, shotgun weapon slot 1 is granted with original maximum 44 shells using the original weapon/HUD update path. Existing savegames and Alien/Predator starts are untouched. The two-player retail-ROM experiment uses separately owned ammo from the original initialized actor; this does **not** mean complete co-op combat or campaign progression is available. See `RE26_S68_MARINE_START_SHOTGUN.md`.
