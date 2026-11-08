# R154 verification — October 21, 2026

| Check | Result |
| --- | --- |
| Strict GCC and Clang Release builds | PASS |
| Complete GCC and Clang `tests/run_tests.sh` | PASS; final success marker and exit 0 |
| Synthetic JERRY audio oscillator/reinitialize regression | PASS with both compilers |
| Architecture and source/no-ROM audits | PASS |
| Original `src/game/` files compared with R153 | BYTE-IDENTICAL (33 files) |
| `--help` contains no external hardware-wave-ROM option | PASS |
| ELF files executable and ZIP CRC | PASS |
| No older archives or commercial ROMs/audio embedded | PASS |
| Jaguar-accurate waveform timbre, startup VBL timing and OP pixel fidelity | NOT VERIFIED |

R154 drops external JERRY wave-ROM lookup/injection. Existing mathematical
waveforms, original ROM-derived FullSynth scores, and gameplay state stay intact.
See `R154_TOM_JERRY_HARDWARE_FIDELITY_AUDIT.md` for hardware-source mapping and
strict open issues. No actual user-retail-ROM audio A/B test was performed.
