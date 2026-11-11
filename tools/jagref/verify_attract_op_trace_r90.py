#!/usr/bin/env python3
"""RE35 R90: repeatable checks of the approximate JagMachine trace, NOT hardware timing.

Inputs must come from the retail-ROM JagMachine trace tool. Numeric witness
fields are for the current refmachine only and must never drive the PC game.
"""
import argparse
import csv
from pathlib import Path


def rows(path):
    with Path(path).open(newline='', encoding='utf-8') as f:
        data = {int(r['field']): r for r in csv.DictReader(f)}
    assert data
    return data


def verify(first, second):
    a, b = rows(first), rows(second)
    for f in (1723, 1724, 1725, 1726, 1739):
        assert f in a, f
    for f in (2156, 2157, 2158, 2159, 2163, 2164, 2172):
        assert f in b, f
    # JagMachine's *post-field* CPU/GPU snapshot is not scanout.
    assert a[1724]['stage_7fb4'] == '1'
    assert a[1724]['pending_data_4fc40'] != a[1724]['staged_data_50c40']
    assert a[1724]['frame_hash_fnv64'] == a[1723]['frame_hash_fnv64']
    assert a[1725]['staged_data_50c40'] == a[1725]['image_7fa4']
    assert a[1725]['frame_hash_fnv64'] == a[1724]['frame_hash_fnv64']
    assert a[1726]['frame_hash_fnv64'] != a[1725]['frame_hash_fnv64']
    assert a[1739]['stage_7fb4'] == '16'
    assert b[2157]['stage_7fb4'] == '1'
    assert b[2157]['frame_hash_fnv64'] == b[2156]['frame_hash_fnv64']
    assert b[2158]['frame_hash_fnv64'] == b[2157]['frame_hash_fnv64']
    assert b[2159]['frame_hash_fnv64'] != b[2158]['frame_hash_fnv64']
    assert b[2163]['pending_x'] == '54'
    assert b[2163]['staged_x'] == '58'
    assert b[2164]['pending_x'] == '50'
    assert b[2164]['staged_x'] == '54'
    assert b[2172]['stage_7fb4'] == '16'
    print('PASS R90 two live ROM-run reference-machine witnesses: GPU stage != posted OP list != field pixels')
    print('PASS second transition pending-vs-staged X publishes one modeled field apart')
    print('LIMIT JagMachine is NOT cycle/bus-accurate; do not transfer measured field numbers into runtime.')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('initial_trace')
    p.add_argument('second_trace')
    args = p.parse_args()
    verify(args.initial_trace, args.second_trace)
