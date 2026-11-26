#!/usr/bin/env python3
"""Reproducible original-only SOURCE crosswalk; not a behavioral-equivalence rate.

Compare readable-C function bodies from the independent Jaguar C reconstruction
against the PC port at the SAME original module path. Never include `new/`,
`new/pc_host/src/`, `new/jaguar/host`, or test/adapter functions added by the port.

Usage: python3 tools/audit_original_source_r154.py --reference /path/to/C-Decomp/src/game \
          --source src/game --out docs/RE38_R154_ORIGINAL_SOURCE_CROSSWALK.md

This intentionally uses conservative lexical matches, not a C semantic parser.
"""
from __future__ import annotations
import argparse
from collections import Counter
from pathlib import Path
import hashlib
import json
import re

FN_END = re.compile(r'([A-Za-z_]\w*)\s*\((?:[^(){};]|\([^()]*\))*\)\s*$', re.S)
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|\d+(?:\.\d+)?|[^\s]', re.S)


def strip_comments(s: str) -> str:
    # Replace comments with a space so tokens on either side cannot concatenate.
    out=[]; i=0; n=len(s); state='normal'
    while i<n:
        c=s[i]; nxt=s[i+1] if i+1<n else ''
        if state=='normal':
            if c=='/' and nxt=='/': out.append(' ');i+=2;state='line';continue
            if c=='/' and nxt=='*': out.append(' ');i+=2;state='block';continue
            if c=='"':state='string'
            elif c=="'":state='char'
            out.append(c)
        elif state=='line':
            if c=='\n':state='normal';out.append('\n')
        elif state=='block':
            if c=='*' and nxt=='/':state='normal';i+=2;continue
            if c=='\n':out.append('\n')
        else:
            out.append(c)
            if c=='\\' and i+1<n:i+=1;out.append(s[i])
            elif c==('"' if state=='string' else "'"):state='normal'
        i+=1
    return ''.join(out)


def extract(path: Path) -> dict[str, str]:
    src=strip_comments(path.read_text(errors='replace'))
    # Remove preprocessor lines from function-signature analysis; keep their
    # effect OUT of the equivalence claim. Conditional compilation may differ.
    scrub=re.sub(r'^\s*#(?:[^\n\\]|\\\n)*\n', '\n', src,flags=re.M)
    depth=0; start=0; body_start=None; name=None; out={}; i=0
    while i<len(scrub):
        c=scrub[i]
        if c=='{' and depth==0:
            head=scrub[start:i].strip()
            m=FN_END.search(head)
            name=m.group(1) if m and not re.search(r'\b(?:typedef|enum|struct|union)\s*$',head[:m.start()]) else None
            body_start=i
            depth=1
        elif c=='{' and depth>0: depth+=1
        elif c=='}' and depth>0:
            depth-=1
            if depth==0:
                if name and name not in out:
                    body=scrub[body_start:i+1]
                    out[name]=' '.join(TOKENS.findall(body))
                start=i+1
                name=None
        elif depth==0 and c==';':start=i+1
        i+=1
    return out


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--reference',required=True,type=Path,help='original Jaguar C reconstruction src/game/')
    ap.add_argument('--source',required=True,type=Path,help='PC port src/game/')
    ap.add_argument('--out',required=True,type=Path)
    ap.add_argument('--json-out',type=Path,help='optional machine-readable per-routine audit ledger')
    args=ap.parse_args()
    if not args.reference.is_dir() or not args.source.is_dir():ap.error('expected game-source directories')
    data=[]; aggregate=Counter(); missing=[]; ledger=[]
    for ref in sorted(args.reference.glob('*.c')):
        port=args.source/ref.name
        original=extract(ref)
        portf=extract(port) if port.exists() else {}
        exact={name for name,body in original.items() if name in portf and body==portf[name]}
        changed={name for name in original if name in portf and name not in exact}
        gone=set(original)-exact-changed
        aggregate.update(total=len(original),exact=len(exact),changed=len(changed),missing=len(gone))
        missing.extend((ref.name,x) for x in sorted(gone))
        data.append((ref.name,len(original),len(exact),len(changed),len(gone),sorted(changed)))
        ledger.append({
            'file':ref.name,
            'reference_file_sha256':hashlib.sha256(ref.read_bytes()).hexdigest(),
            'port_file_sha256':hashlib.sha256(port.read_bytes()).hexdigest() if port.exists() else None,
            'routines':[
                {'name':name,
                 'source_identity':('IDENTICAL_BODY_TOKENS' if name in exact else
                                    'DIFFERENT_BODY_TOKENS' if name in changed else 'NAME_UNMATCHED'),
                 'original_body_sha256':hashlib.sha256(body.encode()).hexdigest(),
                 'port_body_sha256':hashlib.sha256(portf[name].encode()).hexdigest() if name in portf else None,
                 'verified_jaguar_behavior':None}
                for name,body in sorted(original.items())]})
    lines=[
        '# RE38 R154 — Original-only C source crosswalk (NOT 1:1 behavior completion)',
        '',
        'Generated mechanically against the separate original Jaguar C reconstruction.',
        'Scope: functions found in the separate Jaguar readable-C reconstruction `src/game` files;',
        'added `new/` code, bots, multiplayer, editor and PC-specific modules are excluded.',
        'The C reconstruction itself includes some host scaffolding, so this is NOT a strict census',
        'of retail-ROM executable routines; a ROM symbol/address manifest is still required.',
        '',
        '**CRITICAL:** The exact-token count below is a conservative SOURCE-IDENTITY CHECK ONLY.',
        'Identical source does not prove original ROM behavior because the C reconstruction itself is not',
        'an instruction-by-instruction runtime oracle. Conversely, modified source may be fully equivalent',
        '(e.g. host-only checks or UB-safe fixed-point arithmetic). Do not call this a game completion percentage.',
        '',
        f'- Original reconstruction routine names detected: **{aggregate["total"]}**',
        f'- Byte-token-identical function bodies: **{aggregate["exact"]}**',
        f'- Named original functions with changed bodies: **{aggregate["changed"]}**',
        f'- Original names not found by this conservative parser: **{aggregate["missing"]}**',
        '',
        '| Original file | Original routines | Identical C bodies | Modified C bodies | Missing names |',
        '|---|---:|---:|---:|---:|',
    ]
    for f,t,e,c,m,_ in data:lines.append(f'| `{f}` | {t} | {e} | {c} | {m} |')
    lines.extend(['','## Named original-reference functions whose bodies differ','',
                  'These are audit candidates, **not** verified behavioral mismatches. Host-only',
                  'adapters and previously verified fidelity corrections often change C bodies.', ''])
    for f,_,_,_,_,names in data:
        if names:lines.append(f'- `{f}`: '+', '.join(f'`{n}`' for n in names))
    if missing:
        lines.extend(['','## Names requiring manual mapping (no automatic failure assumption)',''])
        lines.extend(f'- `{f}::{name}`' for f,name in missing)
    lines.extend(['','## Original-code 1:1 status (separate from source-body identity)','',
                  'Historic 68000 readable-source structural closure is reported as complete for its tracked',
                  '84,142-byte scope. A complete instruction/state behavior-equivalence numerator is **not available**.',
                  'The source-body counts above MUST NOT be promoted to a behavioral or GPU/DSP percentage.',
                  '',
                  'Known original-path differences still requiring hardware/ROM verification: AMP grid `$FFFF`',
                  'ownership and stale-owner guards; full first-visit/revisit AMP lifecycle; GPU renderer pixel',
                  'ordering; Blitter busy timing; JERRY audio output; and remaining original state transitions.',
                  '',
                  'R154 adds 10,560 deterministic `do_score` cases including C signed-overflow boundary tests.',
                  'These prove the specified 68000 arithmetic contract against an independent arithmetic oracle,',
                  'not exhaustive proof for the whole original game.',
                  ''])
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text('\n'.join(lines))
    if args.json_out:
        args.json_out.parent.mkdir(parents=True,exist_ok=True)
        args.json_out.write_text(json.dumps({
            'schema_version':1,
            'scope':'readable-C original reconstruction src/game only; excludes PC new/ and adapters',
            'NOT_A_BEHAVIOR_PERCENTAGE':True,
            'counts':dict(aggregate),
            'files':ledger},indent=2)+'\n')
    print(f'R154 source crosswalk: original_functions={aggregate["total"]} identical_bodies={aggregate["exact"]} changed={aggregate["changed"]} missing={aggregate["missing"]}')

if __name__=='__main__':main()
