# Atari Jaguar Alien VS Predator — RE21 R234 Linux

The full release includes **native Linux x86_64 GCC + Clang executables**, complete current source under `SOURCE_R234/`, cumulative R1–R234 addendum, retail-ROM read-only audits, original rights/credits and licenses. Supply your own verified retail `AVP.jag`; **not bundled**.

From the extracted full release directory:

```bash
cd linux_x86_64/gcc
./avp_pc --rom /absolute/path/to/your/AVP.jag
```

Build from extracted source, with at most **four jobs**:

```bash
cmake -S SOURCE_R234 -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S SOURCE_R234 -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
```

R234 is a **research/verification release**. The actual game binaries and all working behavior are preserved unchanged from R233. The new exact original packed-pair CROSS arithmetic reference is located in `SOURCE_R234/tools/retail_cross_rgb16_reference_r234.c` and **is not included in production builds**. See `R234_CROSS_RGB16_ROM_VS_HOST_FULL_FRAME_PARITY.md` before attempting a production correction; scene-to-GPU caller and TOM timing fidelity are not yet established.
