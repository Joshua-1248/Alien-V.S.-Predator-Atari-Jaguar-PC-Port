#!/usr/bin/env python3
"""ROM-derived MAZE.S/PostFirst host handoff: static integration regression.

No instrumentation of production code. Check that the hosted new-campaign
sequence invokes the exact existing Jaguar composite rather than two of its
three underlying methods. Independently verify the original reconstruction.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
orig=(root/'src/game/maze.c').read_text()
composite=(root/'src/game/mazescrn.c').read_text()
host=(root/'src/platform/pc/pc_main.c').read_text()
a=orig.split('void PostFirst(void)',1)[1].split('void ResetMaze(void)',1)[0]
assert a.index('InitScrOverlays();')<a.index('ScreenOff();')<a.index('SetMazeList();')<a.index('FadeUp();')
a=composite.split('void InitScrOverlays(void)',1)[1].split('void hug_init(void)',1)[0]
assert a.index('StdHUD();')<a.index('Init1stOvers();')<a.index('InitHUDPal();')
a=host.split('static int pc_begin_campaign(',1)[1].split('static void usage(',1)[0]
start=a.index('ResetPlayer();')
assert start<a.index('InitScrOverlays();',start)<a.index('ScreenOff();',start)<a.index('SetMazeList();',start)<a.index('FadeUp();',start)<a.index('savegame=0;',start)<a.index('end_count=0;',start)<a.index('use_cocoon=0;',start)<a.index('hug_init();',start)
assert 'Init1stOvers();' not in a and 'InitHUDPal();' not in a, 'Host must use the complete composite routine.'
print('Original PostFirst/InitScrOverlays ordering and PC handoff: PASS')
