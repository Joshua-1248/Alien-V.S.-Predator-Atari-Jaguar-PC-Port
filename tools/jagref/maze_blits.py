#!/usr/bin/env python3
"""RE24-S12: decode the original 3D-view Blitter traffic logged by jagref
(JAGM_BLIT_LOG=file) into per-column wall and per-row floor/ceiling tables.

These are the exact fixed-point parameters the retail GPU hands the Blitter
(TRM pp.70-77): destination column/row, texture window, A1 16.16 start and
increment, and the SRCSHADE intensity delta (IINC integer part).  They are the
reference specification a reconstructed native renderer must reproduce.
Only numbers are printed; no image data is written.

usage: maze_blits.py blits.txt FIELD [--walls|--floors|--summary]
"""
import sys
def s16(v): v&=0xFFFF; return v-0x10000 if v&0x8000 else v
def s8(v): v&=0xFF; return v-0x100 if v&0x80 else v
def parse(path,field):
    for line in open(path):
        p=line.split()
        if int(p[0])!=field: continue
        r={k:int(v,16) for k,v in (t.split('=') for t in p[2:])}
        if r['cmd']!=0x41802801: continue
        yield r
def main():
    path,field=sys.argv[1],int(sys.argv[2]);mode=sys.argv[3] if len(sys.argv)>3 else '--summary'
    walls=[];floors=[]
    for r in parse(path,field):
        a2x=s16(r['a2p']);a2y=s16(r['a2p']>>16);n=r['cnt']&0xFFFF
        u=s16(r['a1p'])+(r['a1fp']&0xFFFF)/65536.0;v=s16(r['a1p']>>16)+(r['a1fp']>>16)/65536.0
        du=s16(r['a1i'])+(r['a1fi']&0xFFFF)/65536.0;dv=s16(r['a1i']>>16)+(r['a1fi']>>16)/65536.0
        rec=dict(x=a2x,y=a2y,n=n,tex=r['a1b'],u=u,v=v,du=du,dv=dv,shade=s8(r['iinc']>>16))
        ((r['a2f']>>16)&3)==2 and walls.append(rec) or (((r['a2f']>>16)&3)==1 and floors.append(rec))
    if mode=='--walls':
        for w in sorted(walls,key=lambda w:w['x']):
            print("x=%3d top=%3d h=%3d tex=%06x u=%9.4f v=%9.4f dv=%8.5f shade=%4d"%(w['x'],w['y'],w['n'],w['tex'],w['u'],w['v'],w['dv'],w['shade']))
    elif mode=='--floors':
        for f in sorted(floors,key=lambda f:(f['y'],f['x'])):
            print("row=%3d x=%3d len=%3d tex=%06x u=%9.4f v=%9.4f du=%8.5f dv=%8.5f shade=%4d"%(f['y'],f['x'],f['n'],f['tex'],f['u'],f['v'],f['du'],f['dv'],f['shade']))
    else:
        xs=sorted(set(w['x'] for w in walls))
        print(f"field {field}: {len(walls)} wall column blits covering x={xs[0] if xs else '-'}..{xs[-1] if xs else '-'} ({len(xs)} distinct), {len(floors)} floor/ceiling span blits over rows {min((f['y'] for f in floors),default='-')}..{max((f['y'] for f in floors),default='-')}")
if __name__=='__main__':main()
