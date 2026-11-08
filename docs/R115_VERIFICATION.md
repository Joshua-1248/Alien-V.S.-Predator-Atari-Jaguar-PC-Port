# R115 verification record

- Canonical original retail ROM: `AVP.jag`, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. Inspected privately, NOT included in release.
- RDB.DTA source of `VSX20`: descriptor `$21C18`, type 32, control `$80003000` (loop enabled, original priority `$3000`).
- Focused `test_pred_cloak` PASS with GCC and Clang: original five VMODE settings and timings; matching start/unloop handles across vision changes, decloaking and recloaking. No new sound on idle uncloaked ticks.
- Strict GCC Release build: PASS.
- Strict Clang Release build: PASS.
- Complete sequential GCC C-decomp regression suite: PASS (exit code 0).
- Complete sequential Clang C-decomp regression suite: PASS (exit code 0).
- GCC/Clang retail ROM-backed startup in Xvfb: both recognize the cartridge, decode 74 original panel textures and reach the retail frontend; 7-second harness timeout intentionally terminates the running game.
- Archive CRC verification: PASS. Extracted native Linux executable permissions: `755` for both versions.
- Not yet verified: actual audible decline of the cloak loop during a full Predator campaign on real desktop audio; full Jaguar hardware aural parity. All game/weapon/pain-state logic remains otherwise unchanged.
