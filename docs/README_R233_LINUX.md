# Atari Jaguar Alien VS Predator — RE21 R233 Linux

The provided full release contains native 64-bit Linux executables compiled independently with GCC and Clang under `linux_x86_64/{gcc,clang}/avp_pc`; complete current source under `SOURCE_R233/`; the cumulative addendum, audits, credits, and licensing. Preserve the extracted directory structure and run the binary with a separately owned, **verified** retail `AVP.jag`. It is **not** bundled.

Example (from the unzipped full release folder):
```sh
cd linux_x86_64/gcc
./avp_pc --rom /absolute/path/to/AVP.jag
```

Source build from the included `SOURCE_R233` directory (use at most four compiler jobs):
```sh
cmake -S SOURCE_R233 -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S SOURCE_R233 -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
```

R233 is a **research/verification revision**: executable behavior is byte-identical to R232. Title/select/high-score visual timing, exact GPU/OP presentation, audio waveform-ROM parity and full campaign hardware comparison remain open. Read `R233_ACCELERATED_ROM_TO_PC_FULL_SWEEP.md` before making 1:1-fidelity claims. Never redistribute a cartridge ROM or proprietary audio assets. Retain original author/maintainer credits and applicable licenses.
