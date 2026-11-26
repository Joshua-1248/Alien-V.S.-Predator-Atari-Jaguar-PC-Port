# Alien vs. Predator (Atari Jaguar) — RE #31 R16
## Map Maker: original format, level atlas, and engineering boundaries

**Baseline:** R15 PC source and reconstructed Jaguar game code. **Scope:** exact on-disk maze-cell format, level identifiers, passage links, and metadata prerequisites for a future *playable* map editor. This is a research/implementation handoff, not a claim that all 15 ROM binary maps were independently opened here. A verified retail `AVP.jag` is required for an exhaustive cell-by-cell audit.

### Sources and provenance

1. Primary technical references: `src/jaguar/avp_resources/retail_resources.c`, `src/game/doors.c`, `src/game/door_retail_meta.c`, `src/game/levels.c`, `src/game/amp.c`, and `src/platform/pc/pc_main.c` in the R15/R16 source. The rebuilt `MAZE/LEVDOOR.S` door/access/exit tables are particularly important.
2. Dan McNamee (original level designer), [Arcade Attack interview](https://www.arcadeattack.co.uk/dan-mcnamee/): original engine's **64×64 grid** for level design, with artwork/review and playable layouts checked in-engine.
3. Scott Bigham, [The Jag Zone cross-referenced AvP atlas](https://www.thejagzone.com/jaguar/Codes/avpmaps/index.html), updated 1997: all 15 maps, vent endpoint labels, access locks, and item/key locations. Links to individual pages are given below. Use as independent gameplay/map corroboration, not as the source of the byte format.
4. Contemporary/archival alternate reference: [Atarimania game page](https://www.atarimania.com/game-atari-jaguar-alien-vs-predator_s21118.html) and [GameFAQs complete map walkthrough](https://gamefaqs.gamespot.com/jaguar/586871-alien-vs-predator/faqs/62711/getting-started).

### Retail maze storage, indexing, and decoding

The original game has **15 logical level numbers**:

* 1–5 = Sublevels 1 through 5
* 6–13 = Airduct 1B, 2B, 3B, 3C, 4B, 4C, 5B, 5C
* 14 = Alien Ship
* 15 = Predator Ship

On the verified retail cartridge, the mazes correspond to **FILES.DAT records 87 through 101** (zero-based file indexes; logical level = file index − 86). `avp_resources_find_mazes` extracts their `NONE`/`GZIP` payloads from the ROM archive. The retail 64×64 form decodes to **0x8008 bytes = 8 header bytes + (64×64×8 cell bytes)**. Maze candidate recognition includes a special case for sparsely populated ducts/ships so these records are not rejected by a generic density heuristic.

The 8-byte maze header starts with big-endian 16-bit **width** and **height**, followed by two further big-endian 16-bit source header fields (retail full-size maps commonly use `0x0002, 0x0000`; preserve these bytes exactly rather than assuming all maps do).

The maze is an array of 8-byte cells, with each cell indexed as `offset = 8 + ((y × width + x) × 8)` within the decoded file:

| Cell byte | Purpose in reconstructed game code |
|---|---|
| 0 | Left wall/panel slot |
| 1 | Top wall/panel slot |
| 2 | Right wall/panel slot |
| 3 | Bottom wall/panel slot |
| 4 | Ceiling texture index/slot |
| 5 | Floor texture index/slot |
| 6 | Left-edge door/state flags |
| 7 | Top-edge door/state flags |

**Important:** These are raw native **bytes**, not stand-alone textured 3D meshes. Nonzero wall/panel entries need the original level-specific panel tables to decode their visuals/actions. Door and access lists often refer to *byte offsets* into these very cells. Floors, doors, and collision semantics must be implemented with the actual engine's parsing; interpreting all nonzero bytes as simply 'solid wall' is incorrect.

Separate data from the maze cell array:

* **Door/access/interlock/exit metadata:** `src/game/door_retail_meta.c`, reconstructed from `MAZE/LEVDOOR.S`, includes original per-level access requirements, exit offsets, target level IDs and target coordinates, and door interlocks.
* **Panel mappings:** retail RDB.DTA `LEVELS.S::LoadPanels` source tables interpreted by `retail_resources.c`; necessary for textured surfaces and interactive panels.
* **AMP/enemy/object placements:** decoded from four 15-entry pointer tables, with `6-byte` placement entries (`s16 x_8_8`, `s16 y_8_8`, `u16 template_index`) terminated by `x == -1`. These are species/mode-dependent.
* **Player/campaign starts:** LEVELS table and player species constraints must remain distinct from editor cells.
* **Other gameplay state:** dropped items, security cards, terminals, triggers, airlocks, elevators, player inventory, cocoon mechanics, and all native logic depend on metadata beyond wall geometry.

### The 15 maps, authored connections, and landmarks

The key/security landmarks and numbered air-vent endpoint mappings below are cross-checked against the historical atlas. "Vent 1→..." denotes a *numbered transfer endpoint*, not an editable cell coordinate. For exact grid coordinates the verified original ROM and source-authored byte offset tables must be correlated.

| # | Level / reference | Connections and numbered vents | Items and design-critical details |
|---:|---|---|---|
| 01 | [Sublevel 1](https://www.thejagzone.com/jaguar/Codes/avpmaps/lev1.html) | V1→Duct 1B/V3; V2→Duct 2B/V2; V3→Duct 1B/V2; V4→Duct 1B/V1. Also connects to Predator Ship by authored exit. | Security card **7**, Pulse Rifle, locked level-6 access and terminals. |
| 02 | [Sublevel 2](https://www.thejagzone.com/jaguar/Codes/avpmaps/lev2.html) | V1→2B/V1; V2→3B/V2; V3→3B/V3; V4→2B/V3; V5→3B/V4. | Flamethrower, security card **3**, Pulse Rifle; security access 3,4,5,7,9 and jammed doors. |
| 03 | [Sublevel 3](https://www.thejagzone.com/jaguar/Codes/avpmaps/lev3.html) | V1→3B/V1; V2→4B/V1; V3→3C/V1; V4→4B/V6; V5→3C/V2; V6→3B/V5; V7→3B/V6. | Shotgun, security cards **1** and **2**, Pulse Rifle; access 1,2,6; brig-related progression. |
| 04 | [Sublevel 4](https://www.thejagzone.com/jaguar/Codes/avpmaps/lev4.html) | V1→4B/V2; V2→4B/V3; V3→5B/V1; V4→4B/V4; V5→4B/V7; V6→5B/V4; V7→5B/V3; V8→4B/V5; V9→4C/V1; V10→4C/V2. | Security cards **4**, **6**; Pulse Rifle, Flamethrower; training maze, access 3,4,5,7,9 and multiple jammed doors. |
| 05 | [Sublevel 5](https://www.thejagzone.com/jaguar/Codes/avpmaps/lev5.html) | V1→5C/V2; V2→5C/V3; V3→5C/V1; V4→5B/V2; authored exit to Alien Ship. | Motion Tracker, security card **5**, Marine escape pod, level-6 access. |
| 06 | [Airduct 1B](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct1b.html) | V1→Sublevel 1/V4; V2→Sublevel 1/V3; V3→Sublevel 1/V1. | Triple return path within Sublevel 1. |
| 07 | [Airduct 2B](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct2b.html) | V1→Sublevel 2/V1; V2→Sublevel 1/V2; V3→Sublevel 2/V4. | Connects Sublevels 1 and 2. |
| 08 | [Airduct 3B](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct3b.html) | V1→Sublevel 3/V1; V2→Sublevel 2/V2; V3→Sublevel 2/V3; V4→Sublevel 2/V5; V5→Sublevel 3/V6; V6→Sublevel 3/V7. | Connects Sublevels 2 and 3. |
| 09 | [Airduct 3C](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct3c.html) | V1→Sublevel 3/V3; V2→Sublevel 3/V5. | Two endpoint loop within Sublevel 3. |
| 10 | [Airduct 4B](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct4b.html) | V1→Sublevel 3/V2; V2→Sublevel 4/V1; V3→Sublevel 4/V2; V4→Sublevel 4/V4; V5→Sublevel 4/V8; V6→Sublevel 3/V4; V7→Sublevel 4/V5. | Connects Sublevels 3 and 4. |
| 11 | [Airduct 4C](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct4c.html) | V1→Sublevel 4/V9; V2→Sublevel 4/V10. | Two endpoint loop within Sublevel 4. |
| 12 | [Airduct 5B](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct5b.html) | V1→Sublevel 4/V3; V2→Sublevel 5/V4; V3→Sublevel 4/V7; V4→Sublevel 4/V6. | Connects Sublevels 4 and 5. |
| 13 | [Airduct 5C](https://www.thejagzone.com/jaguar/Codes/avpmaps/duct5c.html) | V1→Sublevel 5/V3; V2→Sublevel 5/V1; V3→Sublevel 5/V2. | Triple return path within Sublevel 5. |
| 14 | [Alien Ship](https://www.thejagzone.com/jaguar/Codes/avpmaps/a_ship.html) | Authored ship airlock/exit links to Sublevel 5. | Security cards **8** and **10**; Alien Queen for Marine/Predator campaigns. |
| 15 | [Predator Ship](https://www.thejagzone.com/jaguar/Codes/avpmaps/p_ship.html) | Authored ship airlock/exit links to Sublevel 1. | Security card **9**; Smart Gun; captive Alien Queen in Alien campaign. |

### Source-authored transition inventory (machine-readable)

The unmodified reconstructed `src/game/door_retail_meta.c` contains **64 native exit/transition records**, **72 access-control records**, and **4 door interlock records** spread across these 15 maps. Their exact map-byte offsets and destination coordinates are transcribed into `new/map_maker/RETAIL_LEVEL_METADATA_RESEARCH.json`, alongside the per-level retail FILES.DAT index. **This is a research inventory only; it is not imported into gameplay or editable through the R16 UI yet.** It provides a concrete basis for the next metadata editor, rather than discarding the original door and destination rules.

### What R16 implements

**F12 first page → Map Maker : PRESS ENTER** launches a real navigable, PC-only map editing interface. The editor obtains privately-owned deep copies of all 15 decoded retail mazes from the running user's legally obtained, verified `AVP.jag`, when loaded. It preserves source header bytes. If no ROM exists (e.g., `--diagnostic`), it offers a bounded 64×64 temporary blank map rather than inventing retail geometry.

User interface: 64×64 overview (when display space permits), selected cell X/Y, selected original cell field and 8-bit value. Arrows move, Tab cycles fields, `+`/`-` adjusts a value, 0/1 sets it, PageUp/PageDown changes level, `N` new blank draft, `R` reload original, `S` save and `L` load `map_maker_level_XX.avpm` in the app settings directory. Saved `.avpm` includes versioned magic `AVPMAP01`, big-endian level/dimensions/reserved, checksum, original eight-byte header, and native eight-byte cells. It is **not** the original retail ROM file format and is not written back to `AVP.jag` or `FILES.DAT`.

**Deliberate safety boundary:** The private staging buffer is never bound to the active native collision/maze systems; choosing/saving a map does **not** yet change any playable level, AI placement, door/airlock destination, or existing campaign. Current save/load covers **maze geometry bytes only**. This restriction prevents a partial editor from corrupting the live Jaguar state while we extend data coverage.

### Next engineering stages before playable maps

1. Decode and expose source-level panel identifiers and texture lookup tables for each original map, including wall side consistency across adjacent cells.
2. Represent **doors, access/interlocks, vents, airlocks, elevators, and portal destinations** as structured editable objects anchored to original byte offsets, preserving their exact Jaguar activation semantics. Cross-check every reciprocal portal.
3. Expose the original AMP/source-template placement data for all three campaigns and common actors. Include player starts, items, terminals, and scripted triggers from their native tables.
4. Implement strict semantic validation (bounds, collision clearance, portal pair consistency, floor/ceiling presence, door references, spawn slots and legal inventory/start positions). Never automatically repair original files without an opt-in.
5. Only then implement opt-in **play-test of a staging map**, with reversible full-state snapshots and a clean restore to unmodified retail world on editor exit. Campaign saves and ROM bytes must stay protected.
6. Run automated comparisons for **all 15 original decoded maps**, every transition, object list, and species campaign against the actual user's retail ROM; verify game behavior, 10 Hz ticks, exact geometry, and corresponding original and optimized modes.

### Intellectual-property note

The retail cartridge, its map images, and level assets are **not** included in the deliverable. The editor uses the owner's `AVP.jag` at runtime. Historical maps remain linked to the original credited author rather than repackaged into the source archive.
