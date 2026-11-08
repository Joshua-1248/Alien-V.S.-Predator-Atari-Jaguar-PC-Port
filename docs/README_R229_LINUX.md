# Alien vs Predator Atari Jaguar PC port — R229

Research-only follow-up to R228. Verifies all 13 absolute CPU references to `$F1BB40` (JERRY DSP RAM) and the original increasing/decreasing `$FA`-step fade callbacks. **The original PC-port gameplay and audio/render code has not been changed.** The remaining original DSP mixer response and GPU/Object Processor pixel timing are not claimed 1:1.

The complete release includes source, original licensing/credits, the cumulative R001–R229 addendum and strict GCC and Clang x86-64 Linux executables with execute permission. Extract the release archive to an executable filesystem on Linux Mint. Put your legally owned `AVP.jag` alongside the chosen `avp_pc` or launch:

```bash
./avp_pc --rom /path/to/AVP.jag
```

Rebuild from `SOURCE_R229`, without exhausting all CPU cores:

```bash
cmake -S . -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
```

Run the new read-only retail cartridge verification:

```bash
python3 tools/audit_retail_frontend_dsp_fade_word_r229.py --rom /path/to/AVP.jag
```

Read `R229_RETAIL_DSP_RAM_FADE_WORD_AUDIT.md` and `R229_VERIFICATION.md` for documented evidence and remaining limitations.
