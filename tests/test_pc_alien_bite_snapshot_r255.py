#!/usr/bin/env python3
"""PC/Jaguar source contract: the Alien view-only bite must use matching 68000 wraps.

Actual NextFrame arithmetic is covered by test_alien_bite_snapshot_r255.c;
this checks that the separately orchestrated PC host uses the exact same
operation and does not alter physical x_pos/y_pos.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
maze=(root/'src/game/maze.c').read_text()
host=(root/'src/platform/pc/pc_main.c').read_text()
for name,source,add,sub in (
    ('MAZE.S',maze,'view_cos','view_sin'),
    ('PC host',host,'cos_ang','sin_ang'),
):
    start=source.index('if(alien_bite){',source.index('gpu_xpos=x_pos'))
    end=source.index('}',start)
    fragment=source[start:end]
    assert 'gpu_xpos += (u32)('+add+' >> alien_bite);' in fragment,name
    assert 'gpu_ypos -= (u32)('+sub+' >> alien_bite);' in fragment,name
    assert 'x_pos=' not in fragment and 'y_pos=' not in fragment,name
print('R255 PC/MAZE Alien bite modulo-32 snapshot contract: PASS')
