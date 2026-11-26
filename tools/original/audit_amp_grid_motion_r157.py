#!/usr/bin/env python3
"""Compare actual compiled original-C and PC AMP occupancy/movement helpers.

Requires the separately supplied original readable-C reconstruction (not bundled),
plus this game's source tree. Uses appended test-only wrappers, leaving both
original source files byte-for-byte untouched. Output is source-level evidence,
NOT a retail 68000 execution trace.
"""
import argparse
import csv
import io
import pathlib
import subprocess
import tempfile
import json
import hashlib

SUFFIX = """
/* R157 TEST-ONLY EXPORTS, never incorporated into the native game. */
int avp_r157_cell(int x,int y){return amp_grid_nonzero(x,y);}
int avp_r157_neighbor(const AvpAmp *a,int dx,int dy){return amp_neighbor_occupied(a,dx,dy);}
"""

def build(cc, source, includes, harness, tmp, tag, sanitize=False):
    material = source.read_bytes()
    copy = tmp / (tag+'_amp.c')
    copy.write_bytes(material + b'\n' + SUFFIX.encode())
    exe = tmp / (tag+'_audit')
    cmd = [cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
           '-ffunction-sections','-fdata-sections','-fno-strict-aliasing',
           '-Wl,--gc-sections']
    if sanitize:
        cmd += ['-fsanitize=undefined','-fno-omit-frame-pointer']
    for inc in includes:
        cmd += ['-I',str(inc)]
    cmd += [str(copy), str(harness), '-o', str(exe)]
    compile_result=subprocess.run(cmd, capture_output=True, text=True)
    if compile_result.returncode:
        raise RuntimeError(f"Compile failed ({tag}): {compile_result.stderr}")
    p = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
    rows=list(csv.DictReader(io.StringIO(p.stdout)))
    return rows,hashlib.sha256(material).hexdigest()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--original',type=pathlib.Path,required=True,help='Original readable-C src/game/amp.c')
    p.add_argument('--original-include',type=pathlib.Path,required=True)
    p.add_argument('--port-root',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[2])
    p.add_argument('--cc',default='cc')
    p.add_argument('--sanitize',action='store_true',help='Use UBSan in both original and port probes')
    p.add_argument('--output',type=pathlib.Path,required=True)
    args=p.parse_args()
    port=args.port_root.resolve()
    harness=port/'tools/original/amp_grid_motion_probe_r157.c'
    with tempfile.TemporaryDirectory(prefix='avp_r157_') as dirname:
        tmp=pathlib.Path(dirname)
        originals, oh=build(args.cc,args.original,[args.original_include,port/'include'],harness,tmp,'original',args.sanitize)
        updated, ph=build(args.cc,port/'src/game/amp.c',[port/'include'],harness,tmp,'port',args.sanitize)
    if len(originals)!=len(updated):
        raise RuntimeError('Different number of probe rows')
    records=[]
    for a,b in zip(originals,updated):
        if (a['scenario'],a['check']) != (b['scenario'],b['check']):
            raise RuntimeError('Probe iteration mismatch')
        records.append({'scenario':a['scenario'],'check':a['check'],
                        'original':a['occupied'],'port':b['occupied'],
                        'difference':int(a['occupied']!=b['occupied'])})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('w',newline='') as out:
        writer=csv.DictWriter(out,fieldnames=list(records[0]))
        writer.writeheader();writer.writerows(records)
    diff=[x for x in records if x['difference']]
    summary={'compiler':args.cc,'sanitizers':args.sanitize,'cases':len(records),'differences':len(diff),
             'original_sha256':oh,'port_sha256':ph,
             'different_scenarios':sorted(set(x['scenario'] for x in diff)),
             'output':str(args.output)}
    print(json.dumps(summary,indent=2))

if __name__=='__main__':
    main()
