# R241 — Original CROSS VBlank callback guards and ACK-without-copy proof

**27 October 2026 · R240 baseline · offline original-instruction research, no production changes.**

Reference: user-provided authenticated retail Atari Jaguar `AVP.jag`, 4,194,304 bytes, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`. The ROM is neither reproduced nor distributed.

## New finding: ACK is not proof of list publication

The original 68000 **six-record** callback at `$01E34A` first checks whether the source Object Processor list head at `$4FC00` is zero. Its `BEQ.W` at `$01E352` branches directly to `$01E3BE`, the **`MOVE.L #1,$7FA8; RTS`** exit. When the head is zero it skips the entire conditional bitmap-pointer patch **and all list-copy stores**, but still acknowledges the GPU. When nonzero, a second conditional uses `$4BE20`: if zero, `BEQ.W $01E362` lands at `$01E388`, skipping only the GPU-picture-pointer patch but not the copy. Otherwise the callback reads `$7FA4`, updates the picture object's first source longword at `$4FC40`, copies six records to `$50C00`, and writes ACK.

The **ten-record** callback at `$01E804` also tests the source head. Its `BEQ.W $01E80C` targets the shared `$01E85E` **ACK+RTS**, likewise bypassing all list-copy stores when empty. The **eight-record** callback at `$01E4D0` has no comparable source-head guard and ordinarily copies its eight records plus tail before ACK. All three ACK stores are on paths also reached when the guarded copy was skipped; describe these as **callback completion**, not unconditionally as **copy completion**.

| Callback | Source head | `$4BE20` | Pointer patch | Staged-list copy | `$7FA8=1` |
|---|---:|---:|---|---|---|
| 6-record `$1E34A` | zero | either | No | **No** | **Yes** |
| 6-record `$1E34A` | nonzero | zero | No | Yes | Yes |
| 6-record `$1E34A` | nonzero | nonzero | Yes, `$7FA4 → $4FC40` | Yes | Yes |
| 8-record `$1E4D0` | either | either | No | Yes | Yes |
| 10-record `$1E804` | zero | either | No | **No** | **Yes** |
| 10-record `$1E804` | nonzero | either | Yes, `$7FA4 → $4FC00` | Yes | Yes |

The CROSS CPU setup at `$1E016–$1E022` writes `$4BE20=1` *before* initiating the GPU at `$1E02E/$1E038`, explicitly enabling the six-record pointer-patch path when the callback executes while that state remains in force. This does not establish that the source-head guard is always true or that the installed callback is never temporarily replaced. Frontend callback selection is checked separately by the R226/R228 work.

### Why this changes the integration requirements

- GPU stage progression and list publication are **not in one-to-one correspondence**. A callback can release the waiting GPU without replacing the staged Object Processor records.
- The previously recovered `$7FA8` clear-and-poll rules from R240 still apply, including the early-ACK-overwritten contingency. This new result adds a **content-side** condition: a successful ACK does not imply fresh display-list contents even if its timing is accepted.
- The six-record callback may re-read `$7FA4` and overwrite the pending picture object's first source longword *after* the CPU's last `Make_New`. Thus the CPU's busy-poll object record is not the sole possible source of the bitmap pointer staged for TOM.
- An integrated scheduler must track at minimum separate **GPU stage work**, **$7FA4 publication**, **CPU pending-list writes**, **callback guard and list-copy outcome**, **$7FA8 ACK**, and **TOM scanline object-list fetch**. A model that increments a displayed-frame counter for every ACK is incorrect.

## Audit and verification limits

New `tools/audit_retail_cross_irq_guard_ack_r241.py` verifies **43 distinct original instruction groups** across retail CPU callback setup, the three writer bodies, all three relevant `BEQ.W` offsets and their actual targets, plus all three ACK exits. It executes **12 synthetic guard combinations** (3 callback types × 2 source-head states × 2 patch-flag states). Of these, four synthetic combinations ACK without copying: zero-head 6-record and 10-record paths for both flag values. These are true branches in the original source, **not proof that the original game ever executed an empty-head branch during CROSS**.

R241 has **no cycle-accurate TOM/68000/GPU/Blitter scheduler**, no observed original IRQ timestamps, no scanline-accurate capture, and no verified count of displayed frames. Do not substitute the synthetic guard cases for observed retail behavior. The production PC renderer remains unchanged until a verified coupled hardware/event trace supports a correct narrow patch. Preserve original licensing, all working features and data formats, and the privately owned original ROM boundary.

Run locally with a legally owned retail ROM:

```bash
PYTHONPATH=tools python3 tools/audit_retail_cross_irq_guard_ack_r241.py --rom /path/to/AVP.jag
```
