# 1:1 fidelity audit brief (read fully before starting)

Goal: find every place where the PC port's reconstruction of the ORIGINAL Atari Jaguar
Alien vs Predator (1994) 68000 game code does not behave 1:1 with the original. You are
READ-ONLY: do not edit, create or delete files anywhere except your own report file.

## Sources
- ORIGINAL 68000 assembly (authoritative): `/home/claude/avp/orig/src/Source Code/` (MAIN/, MAZE/, AMP/, CONTROL/).
  Ignore SAFE/, OLD/, TEST/ subfolders (stale copies). Include files (*.INC) define equates/macros.
  This source tree is a 8-Sep-1994 build that is byte-identical to the RETAIL cartridge code except:
  `initpad` (source sets allow_reset/allow_lcs = $FF; retail CLEARS them), one constant in `Check_EE`,
  and two longword values inside `amp_store` data (source $0A2990, retail $0927C0 at the same offsets).
  Where those differ, retail wins.
- Retail symbol map (address NAME, 8-char truncated names): `/home/claude/avp/lockstep/private/syms.txt`.
- PC PORT C under audit: `/home/claude/avp/work/src/game/*.c`, headers `/home/claude/avp/work/include/`.
- Clean earlier C decompilation for comparison (NOT authoritative, may itself be wrong):
  `/home/claude/avp/cdecomp/Alien-V.S.-Predator-Atari-Jaguar-C-Decompilation-main/src/game/`.
  `diff` between it and the port shows what later revisions changed.
- Project history/audits (context only): `/home/claude/avp/work/*.md`, `/home/claude/avp/work/docs/`.

## What is OUT of scope (do not report as deviations)
- Deliberate PC-only enhancements that are gated by a host setting and default OFF, e.g.
  `marine_pulse_los_hits`, `marine_fire_interrupt`, quick turn, enemies frozen / attacks disabled /
  facehug attach disabled (dev cheats), analog/mouse/enhanced aim (`avp_player_host_*`), noclip,
  infinite health/ammo, pain-sound toggles. Only report one if its OFF path is NOT exactly original.
- Host seams that replace Jaguar hardware (GPU/OP/blitter/DSP calls through runtime ops, VBlank waits,
  file loading) as long as the CPU-visible game state and ordering match the original.
- Pure presentation that does not change game state.

## What IS in scope — report anything that makes CPU game state/ordering differ from the original
- Missing/extra/reordered statements, wrong branch conditions (signed vs unsigned Bcc: BGT/BLT/BGE/BLE
  are signed, BHI/BLS/BCC(BHS)/BCS(BLO) unsigned), wrong operand size (.B/.W/.L), missing sign or zero
  extension, ASR vs LSR, ROL/ROR, MULS/MULU (16x16->32), DIVS/DIVU (32/16, quotient low word,
  remainder high word, overflow leaves operand unchanged), ADDX/SUBX, carry/extend usage, 16/32-bit
  wraparound, DBRA loop counts (runs count+1 times, word counter), table index scaling, off-by-one,
  wrong constants/table values, wrong global, a routine falling through into the next label, a value
  left in a register and used by the caller, condition codes from MOVE/TST used later, random number
  call count/order (any extra or missing `random` call desynchronises everything after it).
- C undefined behaviour that changes results vs the 68000 (signed overflow, shifts by >= width, etc.).

## Method
Go routine by routine through your assigned .S file(s). For each routine find the C counterpart
(names usually match, sometimes merged/renamed; grep). Compare semantics instruction-by-instruction.
Be skeptical of comments in the C claiming fidelity — verify against the asm. Prefer depth over breadth:
a few proven findings beat many guesses. Check the existing tests in `/home/claude/avp/work/tests/`
when unsure what behavior was intentionally chosen.

## Report (write to the report path given in your task, markdown)
For EACH finding:
1. ID, title, severity (gameplay-state / ordering / cosmetic-state), confidence (PROVEN / LIKELY).
2. Original evidence: file:line(s) with the exact asm instructions quoted.
3. Port evidence: file:line(s) with the exact C quoted.
4. Concrete input/state where results differ and what each side produces.
5. Minimal C fix (exact replacement), noting any knock-on.
Then list routines you fully verified as matching (one line each), and routines you could NOT check
and why. Do not pad. If you find nothing in a routine, just list it as verified.
Your final message back should be a short summary: number of PROVEN/LIKELY findings with one-line
titles, and the report path.
