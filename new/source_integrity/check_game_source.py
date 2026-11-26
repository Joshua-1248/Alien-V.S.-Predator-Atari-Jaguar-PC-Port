#!/usr/bin/env python3
"""Check pristine reference source and strict, explicitly recorded PC overlays."""
import argparse, importlib.util, json, os, sys, zipfile
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent.parent
spec=importlib.util.spec_from_file_location('project_source',ROOT/'new/port_overlay/project_source.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference-zip',type=Path);args=p.parse_args()
    m=mod.load();layout=json.loads((HERE/'forwarding_layout_r396.json').read_text());allowed=set(m['reference_sha256'])|set(layout);fail=[]
    try: outputs=mod.verify()
    except (ValueError,OSError,KeyError) as e: print('PROTECTED SOURCE CHANGE:',e,file=sys.stderr);return 1
    for folder in ('src','include'):
        for f in (ROOT/folder).rglob('*'):
            if f.is_file() and f.relative_to(ROOT).as_posix() not in allowed: fail.append(str(f))
    for name,target in layout.items():
        f=ROOT/name; t=ROOT/target
        rel=os.path.relpath(t,f.parent).replace(os.sep,'/')
        expected=[('/* Compatibility forwarding '+kind+'; implementation lives under new/. */\n#include "'+rel+'"\n').encode() for kind in ('header','file')]
        if not f.is_file() or not t.is_file() or f.read_bytes() not in expected: fail.append(name)
    if args.reference_zip:
        if mod.digest(args.reference_zip.read_bytes())!=m['reference_archive_sha256']:fail.append('Original reference archive SHA-256')
        else:
            with zipfile.ZipFile(args.reference_zip) as z:
                for name,sha in m['reference_sha256'].items():
                    member=next(n for n in z.namelist() if n.endswith('/'+name))
                    if mod.digest(z.read(member))!=sha:fail.append(name)
    if fail:
        for name in fail: print('PROTECTED SOURCE CHANGE:',name,file=sys.stderr)
        return 1
    print(f'PASS: {len(m["reference_sha256"])} original files byte-identical to reference; {len(outputs)} explicit PC compatibility delta replays; {len(layout)} forwarding files.')
    return 0
if __name__=='__main__':sys.exit(main())
