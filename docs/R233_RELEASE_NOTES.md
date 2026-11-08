# R233 — accelerated full ROM-to-PC sweep (research-only)

- Full authenticated retail-ROM audit matrix: 25 passing original instruction/data checkers, including new JERRY Timer 1 score-descriptor proof.
- Entire sequential GCC and Clang C-decomp regression suites pass; strict native x86-64 GCC and Clang Linux builds pass; running executables byte-identical to R232.
- Original source, gameplay, graphics, audio, pause, controls and saves untouched. One pre-existing architecture-rule failure is preserved and disclosed.
- Identified exact remaining host substitutions: video-derived 40-VBlank transition fades, 188-VBlank select black hold, 10-VBlank high-score hold, approximated default DSP waveform ROM. The corresponding original control flow is partly recovered but exact displayed frame and DSP sample timing are not yet proven; **no arbitrary new fix**.
- See `R233_ACCELERATED_ROM_TO_PC_FULL_SWEEP.md`, `R233_VERIFICATION.md`, and `AVP_PC_PORT_PROJECT_ADDENDUM_R001_TO_R233.md` for exact evidence, priorities and limits.
- Release includes full current source, both Linux ELF executables, cumulative development notes, original creator credits and licenses. No commercial Jaguar ROM, asset dump, previous release ZIPs or generated caches.
