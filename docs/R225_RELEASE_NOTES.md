# R225 release notes — VBlank callback no-op and wait semantics

Research-only successor to R224. The retail ROM establishes that the callback installed at `$01BE00` points to `$01BEC2`, which consists solely of `RTS`. Unlike null callbacks, this no-op callback does **not** publish fallback STOP; unlike 6/8/10-record callbacks, it does **not** copy object lists or acknowledge CROSS GPU. The normal VBlank handler and `One_Tick` synchronization still operate independently. Added a read-only ROM opcode/branch audit and complete R1–R225 addendum. **No production source or behavior changed.**

See `R225_RETAIL_VBLANK_CALLBACK_AUDIT.md` and `R225_VERIFICATION.md` for tested facts and limitations. No commercial ROM, proprietary assets, prior release ZIPs or production diagnostic patches are included.
