# R153 verification — October 21, 2026

| Check | Result |
| --- | --- |
| Strict GCC and Clang Release builds | PASS |
| GCC and Clang complete tests/run_tests.sh | PASS (both exit normally, final pass marker) |
| Jaguar architecture audit | PASS |
| Public source/no-ROM audit | PASS |
| All original src/game files compared with R152 | BYTE-IDENTICAL (33 files) |
| ELF executables have Linux execute bits | PASS |
| Zip CRC integrity and no earlier ZIPs nested | PASS |
| Live BigPEmu-vs-PC startup/music parity | NOT VERIFIED (no retail ROM/wave ROM available in this build) |

R153 changed only the PC host's cold-boot black presentation, event polling,
and externally supplied authentic JERRY wave-ROM path search. The actual
hardware waveform content is neither present nor invented; the exact SHA-1
validator, original score and synthesizer remain unchanged.
