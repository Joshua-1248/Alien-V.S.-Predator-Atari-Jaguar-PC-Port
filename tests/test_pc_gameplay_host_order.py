#!/usr/bin/env python3
"""Source-contract regression: host frame sequencing versus MAZE.S::NextFrame.

This test deliberately does not patch or instrument production gameplay.
It ensures PC orchestration does not again omit the CPU swap/snapshot or
open a COMPUTER.S session in the middle of a NextFrame update.
"""
from pathlib import Path
import re
import sys
import os

root=Path(__file__).resolve().parents[1]
pc=Path(os.environ.get('AVP_PC_MAIN_SOURCE',root/'src/platform/pc/pc_main.c')).read_text()
maze=(root/'src/game/maze.c').read_text()

# Check only the gameplay tick's host path, not the startup/dev helpers.
frame_start=pc.index('while(now>=next_sim&&!quit&&catchup<5u)')
start=pc.index('if(avp_pc_video_full_cpu_systems(v)){',frame_start)
end=pc.index('}else{\n                readpad();\n                MovePlayer();',start)
tick=pc[start:end]

# Jaguar's authoritative CPU order (do not modify gameplay source to fix PC).
original=maze[maze.index('void NextFrame(void)'):]
original=original[:original.index('\n}\n')]
for item in ('gpu_xpos=x_pos','gpu_ypos=y_pos','gpu_angle=centre_angle',
             'gpu_amps_at=amps_at','gpu_gmps_at=gmps_at','gpu_clist_at=clist_at',
             'gpu_sin_ang=sin_ang','gpu_cos_ang=cos_ang'):
    assert item in original, f'original source oracle changed: {item}'
    assert item in pc[frame_start:start], f'host missing GPU snapshot: {item}'

# Check that all original frame operations remain in their source-defined order.
ordered=('UpdatePlayer();','UpdateAMPs();','Process_EndGame();',
         'PreFrame();','SwapScreens();','PostFrame();','DoPause();',
         'enter_retail_computer(v);')
indices=[tick.index(x) for x in ordered]
assert indices==sorted(indices),f'PC reordered NextFrame/Computer: {list(zip(ordered,indices))}'
assert frame_start < pc.index('gpu_xpos=x_pos',frame_start,start) < start
assert frame_start < pc.index('gpu_amps_at=amps_at',frame_start,start) < start
assert frame_start < pc.index('if(alien_bite)',frame_start,start) < start
assert tick.count('SwapScreens();')==1
assert tick.count('enter_retail_computer(v);')==1
assert tick.index('if(launch_flag)goto campaign_completed;') > tick.index('enter_retail_computer(v);')
# MAZE.S full frame must itself still have the native swap in that exact slot.
for one,two in zip(('PreFrame();','SwapScreens();'),('SwapScreens();','PostFrame();')):
    assert original.index(one)<original.index(two)
print('R185 PC NextFrame/Computer sequence: PASS')
