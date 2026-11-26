#!/usr/bin/env python3
"""Compiled original-C vs port AMP actor-grid lifecycle differential.

The original reconstruction is supplied separately; neither source is modified.
Outputs evidence from synthetic scenarios, not original Jaguar runtime traces.
"""
import argparse
import csv
import hashlib
import io
import json
import pathlib
import subprocess
import tempfile

SUFFIX = """
/* R158 test-only accessors, appended to temporary copies of the source. */
void avp_r158_reset_free_list(void){reset_free_list();}
unsigned avp_r158_free_count(void){return free_count;}
unsigned avp_r158_number(const AvpAmp *a){return amp_number(a);}
void avp_r158_clear(AvpAmp *a){amp_cleargrid_c(a);}
void avp_r158_release(AvpAmp *a){release_amp(a);}
int avp_r158_occupied(int x,int y){return amp_grid_nonzero(x,y);}
"""


def build(cc, original_file, includes, harness, tmp, tag, sanitize):
    src = original_file.read_bytes()
    modified = tmp / (tag + '_amp.c')
    modified.write_bytes(src + b'\n' + SUFFIX.encode())
    exe = tmp / (tag + '_probe')
    cmd = [cc, '-std=c11', '-O1' if sanitize else '-O2', '-Wall', '-Wextra', '-Werror',
           '-ffunction-sections', '-fdata-sections', '-fno-strict-aliasing',
           '-Wl,--gc-sections']
    if sanitize:
        cmd += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined',
                '-fno-omit-frame-pointer']
    for inc in includes:
        cmd += ['-I', str(inc)]
    cmd += [str(modified), str(harness), '-o', str(exe)]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError(f'{tag} compile failed: {p.stderr}')
    p = subprocess.run([str(exe)], capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError(f'{tag} run failed: {p.stderr}')
    return list(csv.DictReader(io.StringIO(p.stdout))), hashlib.sha256(src).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--original', required=True, type=pathlib.Path)
    ap.add_argument('--original-include', required=True, type=pathlib.Path)
    ap.add_argument('--port-root', default=pathlib.Path(__file__).resolve().parents[2], type=pathlib.Path)
    ap.add_argument('--cc', default='cc')
    ap.add_argument('--sanitize', action='store_true')
    ap.add_argument('--assert-r158-baseline', action='store_true',
                    help='Fail if the verified 22-event / 198-field / 18-difference baseline changes')
    ap.add_argument('--output', required=True, type=pathlib.Path)
    args = ap.parse_args()
    port = args.port_root.resolve()
    harness = port / 'tools/original/amp_lifecycle_probe_r158.c'
    with tempfile.TemporaryDirectory(prefix='avp_r158_') as d:
        tmp = pathlib.Path(d)
        a, original_hash = build(args.cc, args.original,
            [args.original_include, port / 'include'], harness, tmp, 'original', args.sanitize)
        b, port_hash = build(args.cc, port / 'src/game/amp.c',
            [port / 'include'], harness, tmp, 'port', args.sanitize)
    if len(a) != len(b):
        raise RuntimeError('Probe count differs')
    columns = ['grid_a', 'grid_b', 'grid_c', 'block_a', 'block_b', 'block_c',
               'free_slots', 'allocation', 'second_allocation']
    rows = []
    for orig, host in zip(a, b):
        if (orig['scenario'], orig['step']) != (host['scenario'], host['step']):
            raise RuntimeError('Event sequence differs')
        for col in columns:
            rows.append({'scenario': orig['scenario'], 'step': orig['step'],
                         'field': col, 'original': orig[col], 'port': host[col],
                         'difference': int(orig[col] != host[col])})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    diff = [r for r in rows if r['difference']]
    summary = {
        'compiler': args.cc, 'sanitizer': args.sanitize,
        'events': len(a), 'comparisons': len(rows), 'differences': len(diff),
        'scenarios': sorted(set(x['scenario'] for x in a)),
        'mismatch_scenarios': sorted(set(r['scenario'] for r in diff)),
        'mismatch_events': sorted(set((r['scenario'], r['step']) for r in diff)),
        'original_sha256': original_hash, 'port_sha256': port_hash,
        'output': str(args.output),
    }
    if args.assert_r158_baseline:
        assert len(a) == 22 and len(rows) == 198 and len(diff) == 18, summary
        expected = {'clear_move_release_reuse', 'duplicate_release', 'move_without_clear',
                    'sentinel', 'shared_cell'}
        assert set(summary['mismatch_scenarios']) == expected, summary
    print(json.dumps(summary, indent=2))

if __name__ == '__main__':
    main()
