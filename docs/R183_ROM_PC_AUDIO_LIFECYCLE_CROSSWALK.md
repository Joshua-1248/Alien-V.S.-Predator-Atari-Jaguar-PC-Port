# R183 — ROM-to-PC audio lifecycle crosswalk

| Function or domain | Reconstructed original `MAZE.S` | R182 PC host | R183 PC host |
| --- | --- | --- | --- |
| Alarm shutdown | `ALRM` game-owned alarm release before other sound teardown | `KillAll` only, leaves CPU `alarm_id` intact | `KillAlar()` clears `alarm_id`, restores protected voice range, releases alarm loop |
| Active SFX | `kill_sounds` after alarm release | `KillAll()` calls backend stop-all | Same `KillAll()`, after `KillAlar` |
| Ambient teardown | `kill_ambient` after SFX stop | Implicit backend stop-all, without CPU `ambient_id` reset | `KillAmbi()` clears `ambient_id` and invokes the dedicated ambient stop callback |
| Game-over ending, high-score frontend | `PlayAvP` completes cleanup before `MAIN.S` switches to MJP | Audio-state residues could survive `KillAll` | All three cleanups before existing `avp_post_game_frontend()` |

**What is verified:** original retail ROM image and gzip regions match recovered original bytes, and the readable-C original contains three distinct ordered game-exit operations. Current portable `KillAll` leaves alarm/ambient identifiers; `KillAlar` and `KillAmbi` explicitly clear them. Host exit now invokes these exact existing routines. All game-owned translation units are byte-identical to R182.

**What remains open:** original assembly instruction-level proof for MAZE's final sound calls; hardware DSP voice-release latencies; real-machine listening/trace comparison. Do not describe R183 as closing all original sound fidelity.

**Next ROM-to-PC crosswalk:** confirm level-transitions' audio/ambient handoff, screen/page transitions and Jaguar GPU OP blitter semantics without altering the working behavior until a reproducible mismatch is proven.
