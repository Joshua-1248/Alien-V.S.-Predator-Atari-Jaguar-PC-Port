#!/usr/bin/env python3
"""Regenerate editor-only read-only LEVDOOR.S map anchors from original C.

Never changes src/game/door_retail_meta.c or the runtime's bound tables.
Run from any directory: python3 new/map_maker/tools_gen_script_catalog.py
"""
from pathlib import Path
import re, json
root = Path(__file__).resolve().parents[2]
src = (root / 'src/game/door_retail_meta.c').read_text()
reference = json.loads((root / 'new/map_maker/RETAIL_LEVEL_METADATA_RESEARCH.json').read_text())
anchors = []


def table(prefix):
    return {int(m.group(1)): m.group(2) for m in re.finditer(
        rf'static const AvpDoor{prefix}Entry {"access" if prefix=="Access" else "exit" if prefix=="Exit" else "inter"}(\d+)\[\]=\{{(.*?)\}};', src, re.S)}

access_tables = table('Access')
exit_tables = table('Exit')
inter_tables = table('Interlock')
requires = {int(m.group(1)): [int(x) for x in re.findall(r'(\d+)u', m.group(2))]
            for m in re.finditer(r'static const u16 inter(\d+)_0_required\[\]=\{(.*?)\};', src, re.S)}

for level in range(1, 16):
    ref = reference['levels'][level-1]
    assert ref['level']==level
    aa=[];ee=[];ii=[]
    for off,code in re.findall(r'\{(\d+)u,\s*(AVP_ACC_[A-Z]+|-?\d+)\s*\}', access_tables.get(level,'')):
        v={'AVP_ACC_UNUSED':-1,'AVP_ACC_EXIT':-2,'AVP_ACC_JAMMED':-3,'AVP_ACC_ESCAPE':-4}.get(code)
        if v is None:v=int(code)
        aa.append((int(off),v));anchors.append((level,1,int(off),v,0,0,0))
    for off,x,y,dest in re.findall(r'\{(\d+)u,(\d+)u,(\d+)u,(\d+)u\}', exit_tables.get(level,'')):
        ee.append((int(off),int(x),int(y),int(dest)))
        anchors.append((level,2,int(off),0,int(x),int(y),int(dest)))
    for off,arr in re.findall(r'\{(\d+)u,inter(\d+)_0_required,',inter_tables.get(level,'')):
        off=int(off);arr=int(arr);assert arr==level and level in requires
        ii.append((off,requires[level]))
        anchors.append((level,3,off,0,0,0,0))
        for required in requires[level]:anchors.append((level,4,required,off,0,0,0))
    assert aa == [(o['maze_byte_offset'], {'AVP_ACC_UNUSED':-1,'AVP_ACC_EXIT':-2,'AVP_ACC_JAMMED':-3,'AVP_ACC_ESCAPE':-4}.get(o['access'],o['access'])) for o in ref['access']], f'access mismatch level {level}'
    assert ee == [(o['maze_byte_offset'],o['destination_x'],o['destination_y'],o['destination_level']) for o in ref['exits']],f'exit mismatch level {level}'
    assert ii == [(o['maze_byte_offset'],o['required_offsets']) for o in ref['interlocks']],f'interlock mismatch level {level}'
    for a in anchors:
        assert 0 <= a[2] < 64*64*8, f'bad offset {a}'
output='''/* Generated from the original src/game/door_retail_meta.c by
 * new/map_maker/tools_gen_script_catalog.py. Do not edit this by hand.
 * PC Map Maker read-only indices; never installed into game door tables. */
static const AvpMmScriptRow avp_mm_retail_script_rows[] = {
'''
for r in anchors:
    output += '    {'+', '.join(f'{n}' for n in r)+'},\n'
output += '};\n'
target = root / 'new/map_maker/map_maker_scripts_catalog.inc'
target.write_text(output)
print(f'Wrote {len(anchors)} read-only retail metadata anchors to {target}')
