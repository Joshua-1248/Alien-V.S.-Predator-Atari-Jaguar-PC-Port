# R225 — Retail MJP VBlank callback classification and One_Tick gate

**Research only; R224 production source and game behavior unchanged.** 2026-10-27.

## Primary cartridge evidence

- Private unmodified 4 MiB Jaguar `AVP.jag`, SHA-256 `b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b`, embeds a GZIP 68000 program at physical ROM `$3804`, decompressing to 99,072 bytes mapped at CPU `$8000`.
- `$01BE00` contains `MOVE.L #$01BEC2,($0002EE80)`; `$01BE0A` immediately calls VBlank wait routine `$0091F6`. The callback at **`$01BEC2` is one instruction, `RTS`**. The next original instruction at `$01BEC4` belongs to the *following routine*, and must not be mistakenly considered part of the callback.
- Original `$0091B6` IRQ handler increments `$2EE84`, clears `$2EE88`, and dispatches the installed callback via `MOVE.L ($2EE80),D0` + `JSR (A0)`. Even with the no-op callback, the interrupt performs its own counter/wake actions and IRQ acknowledgment.
- The separate `$0091F6` `One_Tick` routine sets `$2EE88` to -1, executes `STOP #$2000`, and tests `$2EE88`; its `BNE` returns to `STOP` until the VBlank interrupt clears the word. This is **not** a forced OP list commit operation.
- Six original frontend callback installer sites exist in the examined code. In source/CPU-address order they select `$01BEC2` (immediate-return), `$01E804` (10-record), `$01E4D0` (8-record), `$01E34A` (6-record), `$01E4D0` (8-record), `$01E34A` (6-record). Other code paths may override the global callback: static installer enumeration does *not* establish runtime duration or execution order for all game modes.
- The three list-copy callback bodies acknowledge the CROSS GPU at `$7FA8` **after** their list staging. The no-op callback has no such acknowledgment and no writes to `$4FC00` / `$50C00`; null callback is a third distinct case that builds fallback STOP `(0,4)` at `$7560`.

## Verification method

`python3 tools/audit_retail_frontend_vblank_callback_lifetimes.py --rom /path/to/AVP.jag` checks the exact verified ROM hash and GZIP code size, authenticates **37 unique nonoverlapping opcode groups**, verifies four branch destinations, parses all six original callback setter immediate operands and destination pointers, and asserts distinct no-op / null / staging behaviors. It contains no live production instrumentation or commercial ROM output.

## Implications and limitations

The VBlank callback slot alone is **not** proof of a published OP list on each interrupt. A configured `$1BEC2` is an intentional inert callback, whereas a null callback produces a fallback STOP and list-writer callbacks stage images before `$7FA8` notification. Frame-exact GPU/OP bus arbitration, current callback at every display scanline, interval lengths, hardware pixel comparison, and whether a particular callback supersedes another are **not established** by this static audit.

No original GPU crossfade or renderer change is authorized by this finding. Preserve working weapon HUD, Marine/Alien transitions, controls (including analog elevators), save format, runtime source, audio and 10 Hz gameplay. Do not ship the original ROM, extracted art, reference video or prior release ZIPs. All original credits/licensing remain intact.
