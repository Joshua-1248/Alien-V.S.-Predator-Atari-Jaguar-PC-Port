# Original source and PC compatibility overlay (RE49 R397)

The 70 original reconstructed C, header and table files under `src/` and `include/` are an exact match for the archived authoritative C decompilation reference (archive SHA-256 in `manifest.json`). Do not edit them. The narrow existing compatibility forwarding files are independently checked. All first-party production extensions are under `new/`.

`deltas/` preserves every inherited difference between the original reference and the verified R396 PC-port sources. The 41 JSON files contain 570 ordered line-range edits (zero-based, end-exclusive), with exact base and result SHA-256. They are **not** new gameplay changes; they replay previously shipped port adaptations, reconstruction corrections, and historical deviations. The unchanged port algorithms, including the entire R396 `mazescrn.c` pause/Banana Stops implementation, are compiled in a disposable generated tree, not modified in the reference tree. This is physical source separation, not evidence of 100% retail runtime parity. The earlier Banana Stops laugh-dispatch difference remains in the PC compatibility delta, as before; the original reference file retains the original dispatch.

## Build

Requires Python 3.8+ and the existing CMake/compiler dependencies. Use an out-of-source build directory. For example:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAVP_STRICT=ON -DAVP_BUILD_JAG_REFERENCE=ON
cmake --build build --parallel 4 --target avp_pc
ctest --test-dir build --output-on-failure
```

The standard CMake entry point verifies all originals and forwarding paths, then generates `build/avp_port_source/` and configures it under `build/avp_port_build/`. The standard executable paths (`build/avp_pc`, `build/new/multiplayer/...`) and Vulkan shader directory are preserved. `tests/run_tests.sh` projects a private temporary view before invoking legacy source-relative tests. Existing CMake source, test and shader paths inside the generated view continue to work. Do not edit the generated view; it is regenerated from the authoritative inputs. The legacy root CMake file is now an entry point; actual port build graph is `new/port_overlay/port_build.cmake`.

To verify without building:

```
python3 new/source_integrity/check_game_source.py
python3 new/source_integrity/check_game_source.py --reference-zip /path/to/original-C-decompilation.zip
```

New features belong in `new/` as normal C/C++ modules or existing feature-specific includes. Only when an inherited integration seam must be adjusted should an explicit reviewed compatibility delta be changed. Update its result hash deliberately, retain the reference hash, document the reason, and test. Never change the reference file or its reference hash to bypass the check. A future fidelity audit must review each of the 570 historical changes against the original assembly and ROM before claiming behavioral restoration.
