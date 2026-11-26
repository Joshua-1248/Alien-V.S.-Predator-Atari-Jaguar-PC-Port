#!/usr/bin/env python3
"""Mechanical *triage* of readable Jaguar-C vs PC C function bodies.

This is a lexical comparison, NOT an executable equivalence or ROM-parity
claim. Opaque host extensions, renamed sound constants, preserved source
comments and differing calling conventions all affect the result. The report
helps choose functions for subsequent instruction-level audits without
blindly copying an imperfect source reconstruction.
"""
from pathlib import Path
import argparse
import json
import re
import zipfile

FILES='amp.c collectables.c collision.c computer.c doors.c hud.c hud_message.c levels.c main_game.c maze.c mazescrn.c music.c player.c savegame.c weapons.c'.split()
FUNC=re.compile(r'(?m)^(?:(?:static|extern)\s+)?(?:void|int|u32|u16|s32|s16|char|unsigned|size_t|const\s+\w+\s*\*|AvpAmp\s*\*)\s+([A-Za-z_]\w*)\s*\([^;\n]*\)\s*\{')

def extract_functions(text):
    out={}
    for match in FUNC.finditer(text):
        n=0
        for i in range(match.end()-1,len(text)):
            if text[i]=='{':n+=1
            elif text[i]=='}':n-=1
            if n==0:
                out[match.group(1)]=text[match.start():i+1]
                break
    return out

def normalize(text):
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
    return re.sub(r'\s+','',text)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',type=Path,required=True,help='Original C decompilation ZIP')
    p.add_argument('--port',type=Path,default=Path(__file__).resolve().parents[2])
    p.add_argument('--output',type=Path)
    args=p.parse_args()
    result={'caution':'Lexical comparison only. The Jaguar readable-C reconstruction is not authoritative retail 68000 machine code.', 'files':{}}
    with zipfile.ZipFile(args.reference) as z:
        names=z.namelist()
        for filename in FILES:
            hits=[n for n in names if n.endswith('/src/game/'+filename)]
            path=args.port/'src/game'/filename
            if len(hits)!=1 or not path.exists():
                result['files'][filename]={'status':'source_missing'}
                continue
            original=extract_functions(z.read(hits[0]).decode('utf-8','replace'))
            native=extract_functions(path.read_text(errors='replace'))
            common=set(original)&set(native)
            identical=sorted(f for f in common if normalize(original[f])==normalize(native[f]))
            changed=sorted(common-set(identical))
            result['files'][filename]={
                'reference_functions':len(original),'port_functions':len(native),
                'common_functions':len(common),'lexically_identical':len(identical),
                'changed_common_functions':changed,
                'reference_only':sorted(set(original)-set(native)),
                'port_only_count':len(set(native)-set(original)),
            }
    out=json.dumps(result,indent=2)
    if args.output: args.output.write_text(out+'\n')
    else: print(out)
    print('TRIAGE SUMMARY: '+', '.join(f'{k} {v.get("lexically_identical",0)}/{v.get("common_functions",0)} common lexical matches' for k,v in result['files'].items()))

if __name__=='__main__': main()
