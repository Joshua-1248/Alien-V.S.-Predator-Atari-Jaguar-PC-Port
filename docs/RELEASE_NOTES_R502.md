# R502 — Predator decloak and unavoidable vent acid crossings

Development base: R501. Experimental gameplay build; retained development baseline R495.

## Changes
- Visible hostile selection requests Predator decloak regardless of the old six-tile distance cutoff. Wait for native fade, retry OPTION on proper button edges; after eight observed ticks allow normal FIRE even if cloak flags remain set. Existing aim/range/visibility and other attack restrictions still apply. No native weapon, honor, energy, or cloak changes.
- Vent levels 6–13 (1B, 2B, 3B, 3C, 4B, 4C, 5B, 5C): permit committed crossings of unavoidable acid, including necessary returns. Prefer dry routes; check actual reachability instead of assuming side branches provide bypasses. Remembered re-entry requires stable objective for 12 ticks; commitment bounded at 90 ticks, follows up to six waypoints through bends to dry ground. Do not stop on acid to reconsider combat movement; existing FIRE remains permitted. Existing first-time junction steering preserved except strictly unavoidable straight chokes. Original acid damage and collision remain in force.
- Trace cloak state, decloak wait and vent crossing commitment. Co-op spawning, Banana Stops and Facehugger modifier unchanged.

## Verification (both GCC and Clang)
- Complete software CTest: 238/238 each. All four software/Vulkan executables freshly built; --help successful.
- Native PlayerWeapons fixture: normal decloak FIRE at tick 5, stuck flag FIRE at tick 8. Focused cloak decision and safety exclusions passed.
- Native vent fixture: 96/96 (32 ordinary two-puddle round trips, 32 combat interruption scenarios, 32 dry bypass scenarios). Necessary round trips enter each puddle once per journey. R501 ordinary roundtrip baseline 0/32.
- Native retail-map corridor replay: R501 0/512, R502 512/512, all eight maps, 16 map/axis pairs, both vulnerable species, eight headings, both directions. Controlled puddle placements, original wall and native movement routines; no exact reconstruction of user footage or dynamic actors. Corner replay 64/64. Existing native junction suite 14976/14976. Initial 48/256 retail candidate and 96 junction regressions were corrected before final rebuild.
- Original-source integrity guard: 70 protected files unchanged. Final production source projections match both compilers.

## Live retail smoke tests
Original ROM authenticated SHA256 b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b. Three Deathmatch sessions (4B Predator, 5B mixed, Sub-Level 4 Predator; eight bots each), 360 simulation ticks each, intentionally terminated. 8642 observations through tick 360 inclusive. 26 visible-hostile decloak request sequences, 15 followed by visible-hostile FIRE after cloak cleared within 20 sampled ticks. 1083 visible-hostile Predator FIRE command observations; not confirmed hits. Stuck-cloak fallback and committed vent crossing were not triggered naturally in these sessions; 61 acid-contact observations. Controlled fixtures supply evidence for those specific behaviors. Initial candidate logs not used as final results.

## Limitations
No claim of overall AI improvement, exact recorded puddle loop elimination, globally optimal acid damage, or hardware Vulkan validation. Dynamic crowds and remaining weapon acquisition/zero-command problems still require testing. Historical shell-only runner not rerun. ROM and SDK not distributed.
