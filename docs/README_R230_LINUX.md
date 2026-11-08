# AVP Atari Jaguar PC port — R230 (Linux)

This R230 research-only release retains the R229 production game unchanged and adds an original-ROM FullSynth master-gain audit. Place a personally owned original `AVP.jag` next to either Linux executable, then run it on an executable Linux filesystem.

Build manually from `SOURCE_R230`:

```bash
cmake -S . -B build-gcc -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gcc -j4
cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_STRICT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-clang -j4
python3 tools/audit_retail_dsp_master_gain_r230.py --rom /path/to/AVP.jag
```

No game assets or original ROM are included. Full technical details and limitations are in the new R230 report and cumulative addendum. Retain all existing license, credit and provenance files.
