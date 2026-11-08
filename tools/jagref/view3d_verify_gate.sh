#!/usr/bin/env bash
# S28 one-command GPU-view verification gate. Run after private fixtures have
# been captured. Tests are sequential because run_tests.sh shares /tmp paths.
# Report directory contains logs/check results only, never proprietary RAM.
# Usage: view3d_verify_gate.sh AVP.jag PRIVATE_FIXTURES REPORT_DIR [FUZZ_SEEDS]
set -euo pipefail
rom=${1:?original AVP.jag required}; fixtures=${2:?private capture directory required}; reports=${3:?report directory required}; seeds=${4:-20}
root=$(cd "$(dirname "$0")/../.." && pwd)
[[ $seeds =~ ^[0-9]+$ ]] || { echo 'fuzz seeds must be an unsigned integer' >&2;exit 2; }
if [[ $(sha256sum -- "$rom" | awk '{print $1}') != b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b ]]; then
    echo 'retail ROM hash mismatch' >&2;exit 2
fi
rom=$(realpath "$rom"); fixtures=$(realpath "$fixtures")
if [[ ! -f "$fixtures/manifest.tsv" ]]; then echo 'capture fixtures first' >&2;exit 2;fi
umask 077
mkdir -p -- "$reports";reports=$(realpath "$reports")
if [[ "$reports" == "$fixtures" ]]; then echo 'keep private snapshots and public reports separate' >&2;exit 2; fi
printf 'check\tstatus\tlog\n' > "$reports/checks.tsv"
failures=0
check(){
    local label=$1 log="$reports/$1.log";shift
    printf 'Checking %s... ' "$label"
    if "$@" >"$log" 2>&1;then
        printf 'PASS\n';printf '%s\tPASS\t%s\n' "$label" "${label}.log" >> "$reports/checks.tsv"
    else
        local status=$?;printf 'FAIL (exit %s)\n' "$status"
        printf '%s\tFAIL(%s)\t%s\n' "$label" "$status" "${label}.log" >> "$reports/checks.tsv"
        failures=$((failures+1))
    fi
}
# Build and test the exact current source independently under both compilers.
check build_gcc cmake -S "$root" -B "$reports/build-gcc" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DAVP_BUILD_JAG_REFERENCE=ON
check compile_gcc cmake --build "$reports/build-gcc" --parallel 4
check regression_gcc env CC=gcc bash "$root/tests/run_tests.sh"
check replay_gcc env CC=gcc bash "$root/tools/jagref/view3d_fixture_suite.sh" replay "$rom" "$fixtures" "$reports/build-gcc" "$seeds"
check build_clang cmake -S "$root" -B "$reports/build-clang" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVP_BUILD_JAG_REFERENCE=ON
check compile_clang cmake --build "$reports/build-clang" --parallel 4
check regression_clang env CC=clang bash "$root/tests/run_tests.sh"
check replay_clang env CC=clang bash "$root/tools/jagref/view3d_fixture_suite.sh" replay "$rom" "$fixtures" "$reports/build-clang" "$seeds"
printf 'S28 verify gate: %s failed checks (see %s/checks.tsv)\n' "$failures" "$reports"
[[ $failures -eq 0 ]]
