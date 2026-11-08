## R48: 2D object sprites and clearer preview panels

Zooming into 2D Objects mode now displays the original first-frame Jaguar
sprite directly on the overhead map for verified AMP/AAF mappings. Objects
without a reliable sprite remain colored placement crosses; at overview zoom,
all objects use crosses to avoid clutter. The new sprite previews respect
pan/zoom clipping, the exact stored subcell object position, original aspect
ratio, and selected-cell overlays. This is visual only; no new map bytes or
runtime object visuals are introduced. Texture/Objects HUD previews no
longer overlap the script and status lines. See `README_RE32_R48_MAP_MAKER.md`.

## R38: 2D Textures mode and object thumbnails

`M` cycles Cells, Walls, Objects, Textures. In Textures: `C` cycles Floor,
Ceiling and Walls; `T` chooses a raw Jaguar texture brush; `Enter` paints
an entire 2D selected region or continues painting while held; `Delete`
clears just that texture layer. Other fields are untouched and scripted
panels are guarded atomically. Walls use the clicked side of the cell.
An original-Jaguar texture swatch displays the selected region's artwork,
or **Mix** if the region contains more than one texture ID.
The Objects mode HUD can show native first-frame AAF sprites for known
retail definition mappings; unknown/unloaded definitions remain ID-only.
Everything is editor-only and does not change original game code.

# Alien vs Predator (Atari Jaguar) — Map Maker (RE32 R29)

## R29: Undo and redo without rebuilding mistakes by hand

- **Ctrl+Z** undoes the most recent authored edit; **Ctrl+Y** or
  **Ctrl+Shift+Z** reapplies it. These shortcuts work in 2D and 3D without
  adding another tool mode or changing the existing key-binding indexes.
- A continuous 2D wall draw/erase stroke or 2D floor-texture paint drag is
  **one undo step**, even if the pointer crosses many cells. Ordinary single
  placements, wall rotations, 3D texture-wheel changes, multi-face texture
  edits, native mirror toggles, and object edits are independently undoable.
- The snapshots include authored cell bytes, door metadata overlay, and
  staged object records together. Scripted-door rotation can be undone
  without leaving its door access/interlock links at the moved position.
- Failed/no-op edits do not use history or erase redo. New successful edits
  clear redo. The last 24 completed edit actions are retained in PC memory.
- Switching levels, starting a blank map, loading a saved map, or rebinding
  a source clears the history, preventing accidental edits on another map.
  Undo/redo after Save conservatively marks the active draft unsaved again.
- Focus loss and F12 interruption commit an unfinished stroke; no drag can
  silently merge into a later edit. Editor camera, UI selection, key bindings,
  original Jaguar gameplay and ROM data are unaffected.
- The history exists **only in PC RAM**, never in AVPMAP01/02 files.

## R28: 2D wall drawing without pixel hunting

**No new editing mode or binding required.** Press `W` in 2D, use `T` to
choose the Jaguar wall/door/strip panel, and draw with left-click/drag.
Right-click/drag erases ordinary unprotected panels. The existing `,` and
`.` rotate the selected wall or choose the next empty-wall direction.

- A wall **snaps across the outer quarter of each tile**, not just a one-
  or two-pixel edge. Click a tile's center to use the current chosen side.
- **Green preview** shows exactly which edge would receive the next brush
  click; **red preview** warns that placement cannot safely proceed (existing
  incompatible panel, neighboring face ownership, retail script, or invalid
  door state). Preview is PC-only; it never edits map bytes.
- **Drag fills skipped cells** in a continuous stroke even when the pointer
  jumps across multiple tiles between X11 motion events. The first edge's
  orientation stays locked throughout the stroke: brushing across a corner
  cannot unpredictably turn the wall. Start a new stroke to change direction.
- The zoomed overview stays fixed during a stroke; focus loss ends the
  stroke instead of leaving the brush stuck in a held state.
- All original R27 3D face selection, Shift multi-selection, mirrored textures,
  mouse wheel texturing, arrow-key movement, and original script safeguards
  remain unchanged. No retail game code or ROM data are modified.

## R27: straightforward wall drawing, correct 3D face targeting, and multi-select

**2D mode:** Press **W** for the single wall-drawing tool (and W again to
return to ordinary selection). Left-click or drag places the selected wall
panel; right-click or drag erases an unprotected one. **T** selects the wall
brush from the existing native panel browser. **Comma** and **period** rotate
an existing unscripted wall 90 degrees counterclockwise/clockwise; on an
empty selected edge in Wall mode they change the orientation of the next
wall. Hold **Shift** with those keys for R26's explicitly guarded scripted
relocation. The physical Jaguar panel bytes are still used, not new meshes.

**2D selection:** Hold either Shift key and drag with the **left mouse
button** to highlight a rectangle of cells. The first cell stays anchored;
reversing direction shrinks or expands the selection. Releasing the button
keeps the highlighted area, including when switching Cells/Walls/Objects or
switching to 3D. Shift+drag NEVER places, erases or paints, even in Walls
mode. At close zoom, keeping the cursor near the visible canvas edge while
still holding Shift+left mouse **automatically pans and extends the selection**
beyond the current viewport. There is no new tool or keybinding. Panning stops
on release, when the pointer moves away from the edge, or when a menu opens;
dragging over the left information panel cannot select hidden cells.
Space deselects. Shift+Arrow keys keep their existing behavior.

**3D mode:** Left-click **selects a surface; it does not paint**. Hold either
Shift key while clicking to add another face (or remove one already selected).
Without Shift a click starts a new selection. A highlighted outline is on
by default; **H** toggles it. **Mouse wheel** cycles texture IDs on the selected
surface(s). **T** chooses an exact ID; **Tab** copies the last selected texture;
**Enter** pastes to the selected surfaces. **M** mirrors the artwork of
selected wall faces by toggling only the original Jaguar panel reflection
bit (0x80); this cannot mirror floors/ceilings or empty panels. A selection
can include different visible wall faces, and batch edits are atomic: one
protected destination prevents the whole edit rather than half-editing it.
The interface explicitly labels the **west, north, east, or south wall face**.

**Face correction:** Click/raycast now chooses the boundary face *toward*
the camera rather than silently selecting the neighbor's opposing backside
byte. An existing backside panel may still be visible through the native
renderer; applying a front-face texture never overwrites that backside.
The editor ray tests actual perspective floor/ceiling intersections rather
than selecting floors in a fixed screen band in front of nearby walls.

**Crosshair:** Window-relative pointer coordinates are converted into the
centered presentation image's coordinates before 3D picking, and the X11
crosshair is drawn with the corresponding image offset. It no longer
assumes the window and presentation image have identical dimensions.

**Movement:** In 3D, hold Up/Down to move forward/back and Left/Right to
turn smoothly through the same delta-time camera path as WASD/Q/E. The
old discrete KeyPress and 350/75 ms menu-repeat movement paths are disabled
for 3D. In 2D, arrow keys still move the selected tile discretely.

**Safety:** The user interface additions are transient and are not serialized
into AVPMAP01/02; existing saved key binding indices are preserved. Retail
scripted doors remain protected, and relocated scripts are still blocked
from Play Test pending isolated runtime metadata rebinding. No original
Jaguar game files, live campaign maps, or game renderer semantics change.

## R26: explicit editor-only scripted-edge relocation and validated AVPMAP02

**This is not a playable scripted-door relocation yet.** Retail campaign
scripts are still bound to the original game tables. R26 allows **private
editor drafts** to move an original scripted wall or door with its matching
map byte offsets, and prevents launching those modified scripts in Play Test.

- In 2D, select an original scripted wall edge. **Shift+,** moves it one side
  counterclockwise; **Shift+.** moves it clockwise. The ordinary unmodified
  `,` and `.` keys remain protected and still rotate ordinary walls.
- If the source has an EXIT panel, its transition destination values travel
  with that panel. If it has a door-state access or interlock rule, all records
  anchored at the source state byte move to the new canonical state owner.
  Required-interlock entries referring to the relocated interlock are updated.
- **All-or-nothing**: rotation refuses an occupied destination, opposite-cell
  panel conflict, existing destination script, shared paired-door state,
  missing canonical door-state byte, or any metadata index failing validation.
  The original access codes and transition destinations are never rewritten.
- Additional protection blocks painting over a scripted panel on the opposite
  face of the same physical edge (even when the shared door-state byte itself
  has no access rule).
- Press `K` to inspect original or relocated metadata. The HUD distinguishes
  `RETAIL SCRIPT` from `DRAFT SCRIPT` after a private relocation.
- Ordinary drafts continue to save in **AVPMAP01** with byte-compatible
  headers and checksums. When scripts are moved, the editor saves **AVPMAP02**,
  containing all original per-level rows with relocated offsets, an exact
  fingerprint of the source retail map, and a separate CRC32. The loader
  verifies both checksums, preserved access/exit values, unique and grouped
  offsets, required-interlock links, and the correct original level/map.
  Invalid, truncated, wrong-ROM, or corrupted files leave the working editor
  unchanged.
- **Fail closed**: R26 does not rebind the original Jaguar or native PC game
  metadata, so `P` Play Test refuses AVPMAP02 drafts. This is deliberate;
  playing a moved door with unrelocated runtime metadata would break card,
  elevator, airlock and exit behavior. Normal drafts still playtest.
- No original `src/game/` files, ROM bytes, multiplayer AI, F12 bindings,
  or simulation/rendering semantics are changed. Shift+rotation is a new
  explicit keyboard shortcut; existing 23 Map Maker saved binding indices
  are unchanged. It is not a game key binding.

**Remaining step for fully playable moved scripted doors:** bind an isolated
per-playtest level metadata copy that can reconstruct the game's original
`AvpDoorLevelMeta` access, exit, interlock, and required-entry tables from the
validated AVPMAP02 index. Preserve transitions into and out of other levels,
including scenarios with multiple edited maps; do not mutate global retail
metadata until all level dependency and gameplay checks pass.

## R25: script-aware retail door and exit protection

The editor now displays a **RETAIL ANCHOR** indicator if the selected
64×64 retail-derived map edge has LEVDOOR.S access, exit, interlock or
required-interlock metadata. Press **K** in 2D or 3D to inspect that link.
For raw left/top door-state fields (6/7), K looks up that exact byte.
An airlock access check refers to a **door-state byte**, whereas an
exit/duct/transition record refers to a **panel byte**. A required interlock
also references a door-state byte. For right/bottom wall sides the canonical
door-state byte belongs to the eastern/southern neighbor respectively.

When a retail-derived script anchor is involved, **wall/panel rotation,
cell deletion and panel-ID overwrite are rejected**. This avoids silently
leaving a card lock, jammed door, airlock or level-transition record pointing
at the old location. Ordinary unscripted panel rotation and blank custom
maps are unchanged. Door-state value edits are still available; those change
only opening state, not the script's address. An imported old draft that
already moved a scripted panel is not automatically repaired.

The editor's index contains 144 entries (72 access, 64 exit, 4 interlock,
4 required-state anchors) generated from the original unmodified
`src/game/door_retail_meta.c` and cross-checked against the R16 reference
JSON. To regenerate after a game-source change run:

    python3 new/map_maker/tools_gen_script_catalog.py

This is deliberately a **read-only prerequisite**, not a working structured
metadata relocation/export system. No table is rebound in the live game or
playtest, and `.avpm` has not acquired script relocation data. The game
still uses its own original script tables. Scripted-door movement stays
blocked until an independently validated, persistent overlay can preserve
all dependent offsets and level-transition semantics.


## R24: native panel identification during placement

The 2D selected-edge HUD, 3D targeted-surface HUD and T palette now show the
**native wall type** (0..15), the ID's reflection/mirror flag, and the native
handler family. The classification follows the exact `InitDoors` table and
`MAZE2` renderer: types 0..2 are solid, type 3 is shaped solid, types 4..11
are doors, type 12 is a non-rendered/unused handler, and types 13..15 are
partial/strip panels. Zero and 128 are both empty. This is a classification of
the Jaguar **handler**, not a guarantee of any particular artwork; the actual
archway, strip and door textures are level-specific retail panel resources.
In the T browser while a wall edge is selected, **W** jumps to the first
solid wall type, **S** to shaped-solid panels, **D** to doors, and **A** to
partial/strip types (potential archways). Arrow keys still choose individual
variants and PgUp/PgDn pages. Shortcuts never assign a panel by themselves.
The browser remains a raw 0..255 ID selector; panel types are **not**
converted into independent objects or meshes. All old controls, keyboard and
joystick bindings and `.avpm` serialization remain unchanged.

## R23: 2D wall, arch/strip and door orientation

The native Jaguar maze keeps **four panel bytes per cell** in this order:
left (0), top (1), right (2), bottom (3). A panel ID controls both texture
lookup and collision classification. The `InitDoors` wall-type table groups
IDs (reflection bit 7 masked off) into solid types 0..3, door types 4..11,
and non-solid/partial pass-through types 12..15; actual archway and strip
*artwork* is level-specific and must be selected from the retail panel table.
The flat floor and ceiling bytes (5 and 4) are independent of wall slots.

In 2D, click an edge of the highlighted cell to select that directional
panel. `F` continues to cycle raw field selection. `T` opens the existing
texture/panel browser to place its panel ID. `,` rotates a selected panel
90 degrees counter-clockwise; `.` rotates it 90 degrees clockwise. The
selected panel edge is highlighted amber. Rotating moves the ID to the next
side rather than spinning its texture pixels or changing reflection bit 7.
The rotation actions are appended to the editor's keyboard, mouse and
joystick binding lists, preserving all existing action indices.

A move is **rejected** rather than overwriting an occupied side or its
opposite neighbor. For door panel groups, the editor also moves the native
open-state byte into the correct left/top owner cell, rejecting invalid
perimeter references, shared paired-door bytes, and occupied state slots.
This does NOT move original security-card access rules, elevators, airlock
transitions, interlocks or triggers: those remain authored in the separate
retail tables and need a structured metadata editor before moved scripted
doors can be considered gameplay-correct. The source ROM and active game
remain entirely untouched; these operations alter `.avpm` drafts only.

## R22: 3D crosshair surface highlight

- **H** toggles an optional bright yellow, thin four-sided outline around the
  wall, floor or ceiling *currently selected by the 3D crosshair*. It starts
  **Off**, draws no filled polygons, and is an editor-only visual overlay.
- The outline follows the detached editor camera, pitch and perspective.
  Visible clipped edges remain highlighted even when a nearby wall fills the
  preview. Selected zero-ID wall slots can also be highlighted with **G**
  (Ghost Walls) enabled.
- The action is rebindable as **Toggle Surface Highlight** in the editor's
  keyboard, mouse and joystick pages (B). Binding rows scroll at small window
  sizes. **H in 2D mode still selects the original left/top door-state field**.
- **Keypad Enter** switches 2D/3D. **Tab** copies a targeted texture; main
  **Enter** pastes it. Those controls remain unchanged. The outline is not
  serialized to `.avpm`, and cannot affect gameplay or the original renderer.

The notes below describe the earlier R18 editor milestone; later Map Maker
features (blank-map startup, textured overhead, texture browser, Escape
Save/Save As/Load, objects and playtesting) are documented in R19–R21 release
notes.

---

# AvP Jaguar — PC Map Maker (RE31 R18)

The editor is accessible from **F12 > Map Maker : PRESS ENTER**. It operates
on a *private* decoded copy of one of the 15 retail maps. It never changes the
running simulation's maze, the source ROM, or authored object/exit tables.

## 2D / 3D

- **Tab or F3** toggles between a 2D tile overview and a 3D editor camera.
- **2D**: arrows move the tile cursor; left-click selects a tile; `F` or
  Shift+Tab cycles one of the original eight cell fields.
- **3D**: the PC port invokes the *same Jaguar maze wall/flat rasterizer*
  used during gameplay with a separate renderer state and framebuffer. WASD
  moves, Q/E turns, arrows also move/turn, and the mouse points at a wall,
  floor or ceiling. Left click, +/- or assigned edit buttons change the
  selected field of the *draft* map.
- **T** opens the 0–255 texture/property ID palette. Arrows navigate,
  PgUp/PgDn change palette page, Enter assigns the selected ID, and Esc
  closes the palette. These are numeric raw field IDs, not texture thumbnails.
- **PgUp/PgDn** outside the palette selects a level; **S** saves and **L** loads
  `map_maker_level_XX.avpm`; **R** restores the retail source and **N** creates
  a blank draft. The editor prevents level replacement when unsaved changes
  exist. **Backspace/Esc** returns to F12.

## Bindings

**B** from Map Maker opens its keyboard, mouse and joystick/gamepad binding
pages. Each page uses the ordinary F12 Enter-to-capture/Delete-to-clear
semantics. The joystick page accepts buttons or axis directions. Analog camera
movement reuses the existing player-1 joystick axis/deadzone configuration.
Bindings are saved as `map_maker.*` keys in `settings.cfg`.

## Limitations and safety

- This is the *first 3D surface-editor milestone*, not yet a full BUILD editor.
  Cursor picking uses an independent bounded maze ray and may not line up at
  every wall edge with the native rasterizer. Texture selection is numeric;
  no material thumbnails, sloped planes or object/door/exit editors yet.
- The 3D editor preview needs a correctly loaded retail `AVP.jag` for the
  original textures. Without the ROM, diagnostic geometry is only a smoke
  test. `AVP.jag` is not distributed.
- Draft `.avpm` files are intentionally not gameplay-ready and cannot be
  injected into the original campaign; metadata for doors, AMP entities,
  exits, security access and transitions must be supported first.
- `src/game/` original source is preserved byte-for-byte. Normal rendering,
  simulation timing, multiplayer, micro-optimizations and F12 settings remain
  independent of the editor.

## RE32 R30: Play Test with relocated scripted doors (supersedes R26-R29 restriction)

`P` can now launch AVPMAP02 edited scripts **in a separate game process**.
The child validates the offset-only relocation against the original LEVDOOR.S
index, constructs a private typed copy of all 15 retail level tables, then
replaces only the selected edited level's script offsets. The existing original
DoorKeys/access, exit, elevator, and interlock state machines consume the
private typed tables without any changes to `src/game/`. The edited maze bytes
are independently overlaid into the same child, both at first boot and when
returning to the edited level via a level transition.

Unedited levels still use identical retail records. An invalid relocated
script aborts the child playtest rather than partially installing it. The
original ROM, editor, campaign files and other processes are unaffected.
The browser recognizes both AVPMAP01 and AVPMAP02 drafts.

**Verification boundary:** automated parity checks compare all rebuilt
access, exit and interlock records against `door_retail_meta.c` (all 15
levels), and check relocated reference coherence. Playable behavior still
requires direct retail-ROM/manual end-to-end testing; do not treat metadata
parity tests as proof every moved physical door or scripted effect works.
