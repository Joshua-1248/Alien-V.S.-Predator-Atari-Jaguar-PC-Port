#!/usr/bin/env python3
"""ROM-backed F2 custom-map restart regression, exercising the original X11 UI.

Usage: python3 test_custom_map_restart_x11.py --exe build/avp_pc \
    --rom /private/AVP.jag --map /private/DM1.avpm [--players 2]

No commercial ROM or user-authored map is copied into the source/release.
This detects the R120 bug: the second F2 launch reused the retail-level
start pose, causing DM1 native DM actor initialization to fail before tick 1.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--exe',type=Path,required=True)
    ap.add_argument('--rom',type=Path,required=True)
    ap.add_argument('--map',type=Path,required=True)
    ap.add_argument('--players',type=int,choices=[1,2,3,4],default=1)
    ap.add_argument('--restarts',type=int,default=2)
    ap.add_argument('--display',default=':183')
    args=ap.parse_args()
    assert args.restarts>=1 and args.map.suffix=='.avpm'
    source_map=args.map.resolve()
    source_objects=source_map.with_suffix('.avpo')
    with tempfile.TemporaryDirectory(prefix='avp-re37-r121-custom-restart-') as td:
        work=Path(td)
        binary=work/'avp_pc'
        shutil.copy2(args.exe.resolve(),binary)
        custom=work/'custom_maps'
        custom.mkdir()
        shutil.copy2(source_map,custom/source_map.name)
        if source_objects.exists():
            shutil.copy2(source_objects,custom/source_objects.name)
        settings=(
            'local_multiplayer.enabled=true\n'
            f'local_multiplayer.players={args.players}\n'
            'local_multiplayer.campaign=0\n'
            'local_multiplayer.game_mode=1\n'
            'local_multiplayer.dm_level=16\n'
            'local_multiplayer.level_explicit=true\n'
            f'local_multiplayer.custom_map={source_map.stem}\n'
            'local_multiplayer.bot_count=0\n'
        )
        (work/'settings.cfg').write_text(settings)
        output=work/'run.log'
        env=dict(os.environ,DISPLAY=args.display)
        xvfb=subprocess.Popen(['Xvfb',args.display,'-screen','0','1024x768x24','-nolisten','tcp'],
                              stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        game=None
        try:
            time.sleep(.75)
            x=C.CDLL('libX11.so.6')
            xt=C.CDLL('libXtst.so.6')
            x.XOpenDisplay.argtypes=[C.c_char_p]
            x.XOpenDisplay.restype=C.c_void_p
            x.XDefaultRootWindow.argtypes=[C.c_void_p]
            x.XDefaultRootWindow.restype=C.c_ulong
            x.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),
                                    C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)]
            x.XQueryTree.restype=C.c_int
            x.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
            x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong]
            x.XKeysymToKeycode.restype=C.c_uint
            x.XFlush.argtypes=[C.c_void_p]
            xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
            dp=x.XOpenDisplay(args.display.encode())
            if not dp:
                raise RuntimeError('XOpenDisplay failed')
            with output.open('w') as stream:
                game=subprocess.Popen([str(binary),'--rom',str(args.rom.resolve())],
                                      cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
                def content():
                    stream.flush()
                    return output.read_text(errors='replace')
                def await_text(fragment,count,seconds):
                    end=time.monotonic()+seconds
                    while time.monotonic()<end:
                        data=content()
                        if data.count(fragment)>=count:return data
                        if game.poll() is not None:break
                        time.sleep(.2)
                    raise RuntimeError(f'Missing {fragment!r} instance {count}; process={game.poll()}\n'+content()[-2200:])
                await_text('Retail frontend: translated NewTitle',1,18)
                root=x.XDefaultRootWindow(dp)
                rr,parent,n=C.c_ulong(),C.c_ulong(),C.c_uint()
                children=C.POINTER(C.c_ulong)()
                if not x.XQueryTree(dp,root,C.byref(rr),C.byref(parent),C.byref(children),C.byref(n)) or not n.value:
                    raise RuntimeError('No game window')
                x.XSetInputFocus(dp,children[n.value-1],1,0)
                x.XFlush(dp)
                def press(key):
                    code=x.XKeysymToKeycode(dp,key)
                    if not code:raise RuntimeError(f'Unknown keysym {key:#x}')
                    xt.XTestFakeKeyEvent(dp,code,1,0)
                    x.XFlush(dp)
                    time.sleep(.09)
                    xt.XTestFakeKeyEvent(dp,code,0,0)
                    x.XFlush(dp)
                    time.sleep(.23)
                # The F2 root cursor initially points at row 0. Up wraps
                # to Start Game, matching the real menu navigation.
                for iteration in range(args.restarts+1):
                    press(0xffbf) # F2
                    press(0xff52) # Up: root row 0 -> Start Game
                    press(0xff0d) # Enter
                    data=await_text('MP original tick 20:',iteration+1,12)
                    for missing in ('Multiplayer native session faulted',
                                    'MP movement test cannot bind', 'FATAL signal'):
                        if missing in data:
                            raise AssertionError(f'Unexpected {missing!r}\n'+data[-2200:])
                    custom_count=iteration+1
                    if data.count('Playtest spawn committed:')<custom_count:
                        raise AssertionError('Custom spawn was not committed on each launch')
                    if data.count('Custom map ID 16:')<custom_count:
                        raise AssertionError('Custom map was not rebound on each launch')
                if game.poll() is not None:
                    raise AssertionError(f'Game exited prematurely: {game.returncode}')
                print(f'PASS {source_map.name}, P1-P{args.players}: '
                      f'{args.restarts+1} F2 launches; each passed tick 20; '
                      'custom-map spawn applied on every custom launch')
        finally:
            if game and game.poll() is None:
                game.terminate()
                try:game.wait(timeout=2)
                except subprocess.TimeoutExpired:game.kill();game.wait()
            xvfb.terminate()
            xvfb.wait(timeout=3)


if __name__=='__main__':main()
