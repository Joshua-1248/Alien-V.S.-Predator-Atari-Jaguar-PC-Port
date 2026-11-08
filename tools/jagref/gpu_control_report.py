#!/usr/bin/env python3
"""RE24/S15: summarize *observed* GPU starts/stops from a jagref CSV trace.

Run locally with the user's retail cartridge (never distribute trace contents):
  JAGM_GPU_CONTROL_CSV=/private/gpu.csv build-gcc/jagref AVP.jag FIELDS EVERY OUTDIR
  python3 tools/jagref/gpu_control_report.py /private/gpu.csv

Shows who wrote G_PC and G_CTRL and whether a GO rising edge followed.
An observed GPU entry is not assigned to HUD, 3D, or another function without
independent original-assembly evidence. This is research tooling, not a renderer.
"""
import argparse
import csv
from collections import Counter

HEADER = ('field', 'cycles', 'vc', 'master', 'addr', 'value',
          'pc_before', 'pc_after', 'go_before', 'go_after', 'pc68')
NAMES = ('68000', 'GPU', 'DSP', 'Blitter', 'OP')

def decode(stream):
    reader = csv.DictReader(stream)
    if tuple(reader.fieldnames or ()) != HEADER:
        raise ValueError('wrong GPU control trace CSV header')
    for number, row in enumerate(reader, 2):
        if any(v is None for v in row.values()) or None in row:
            raise ValueError(f'malformed CSV line {number}')
        try:
            e = {k: int(row[k], 16 if k in {'addr','value','pc_before','pc_after','pc68'} else 10)
                 for k in HEADER}
        except (TypeError, ValueError) as exc:
            raise ValueError(f'invalid GPU control trace line {number}') from exc
        if e['addr'] not in (0xF02110, 0xF02114) or e['master'] not in range(5):
            raise ValueError(f'impossible GPU control register/master at line {number}')
        if e['go_before'] not in (0,1) or e['go_after'] not in (0,1):
            raise ValueError(f'invalid GO state at line {number}')
        yield e

def summarize(rows):
    starts=[];stops=[];pc_writes=[]
    for e in rows:
        if e['addr']==0xF02110:
            pc_writes.append(e)
        else:
            if not e['go_before'] and e['go_after']:
                starts.append(e)
            if e['go_before'] and not e['go_after']:
                stops.append(e)
    return starts,stops,pc_writes

def report(rows):
    starts,stops,writes=summarize(rows)
    print(f'GPU GO rising edges: {len(starts)}; falling edges: {len(stops)}; G_PC writes: {len(writes)}')
    counts=Counter((e['pc_after'],e['master']) for e in starts)
    for (pc,writer),n in sorted(counts.items()):
        print(f'  start at ${pc:06X}, writer={NAMES[writer]}: {n} time(s)')
    print('Individual GO rising edges:')
    for e in starts:
        site=f' 68K=${e["pc68"]:06X}' if e['master']==0 else ''
        print(f'  field={e["field"]} vc={e["vc"]} GPU_PC=${e["pc_after"]:06X} '
              f'writer={NAMES[e["master"]]}{site}')
    if not starts:
        print('  (no rising edge observed: no function is inferred)')
    return len(starts)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('csv')
    a=ap.parse_args()
    try:
        with open(a.csv,newline='',encoding='ascii') as f:
            rows=list(decode(f))
        report(rows)
    except (OSError,ValueError) as exc:
        ap.exit(2,f'GPU control report: {exc}\n')
if __name__=='__main__':main()
