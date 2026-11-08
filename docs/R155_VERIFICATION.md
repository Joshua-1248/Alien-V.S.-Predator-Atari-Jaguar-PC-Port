# R155 verification — October 21–22, 2026

| Check | Result |
| --- | --- |
| Strict GCC and Clang Release builds | PASS |
| Complete GCC and Clang `tests/run_tests.sh` | PASS; exit code 0 and final success marker |
| Fractional TOM scaling synthetic framebuffer tests (GCC and Clang) | PASS |
| Source/architecture and no-bundled-ROM audits | PASS |
| Original `src/game/` files compared with R154 | BYTE-IDENTICAL (33 files) |
| Release ZIP integrity and ELF execute permissions | PASS |
| Additional hardware wave ROM required | NO; AVP.jag only |
| Full Jaguar OP scanline/DSP exactness | NOT VERIFIED |

The changed SELECT OP sampling uses source pixel `floor(n*32/scale)` and
`ceil(N*scale/32)` output extent, with the original 239-line portrait source
height. No game logic, timing, or JERRY synth changes. Live ROM-based visual
and audio validation is still required.
