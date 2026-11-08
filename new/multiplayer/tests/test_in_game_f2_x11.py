#!/usr/bin/env python3
"""Live X11 F2 co-op setup smoke. No commercial ROM or assets required.

Run after building avp_pc:
  python3 new/multiplayer/tests/test_in_game_f2_x11.py --exe build/avp_pc
Requires Xvfb, libX11, libXtst; does not touch user settings.
"""
import ctypes as C, os, shutil, subprocess, tempfile, time, argparse
from pathlib import Path
parser=argparse.ArgumentParser(description='Exercise live F2 AI handover and new-game restart in a no-ROM diagnostic X11 session.')
parser.add_argument('--exe',required=True,type=Path)
parser.add_argument('--display',default=':158')
args=parser.parse_args()
src=args.exe.resolve()
with tempfile.TemporaryDirectory(prefix='avp-re28-s101-livef2-') as td:
    d=Path(td); gamebin=d/'avp_pc'; shutil.copy2(src,gamebin)
    (d/'settings.cfg').write_text('local_multiplayer.enabled=true\nlocal_multiplayer.players=2\nlocal_multiplayer.player_0.bot_enabled=false\nlocal_multiplayer.player_1.bot_enabled=false\n')
    log=d/'run.log'; display=args.display; env=dict(os.environ,DISPLAY=display)
    xvfb=subprocess.Popen(['Xvfb',display,'-screen','0','960x700x24','-nolisten','tcp'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    game=None
    try:
        time.sleep(.8)
        x=C.CDLL('libX11.so.6'); xt=C.CDLL('libXtst.so.6')
        x.XOpenDisplay.argtypes=[C.c_char_p];x.XOpenDisplay.restype=C.c_void_p
        x.XDefaultRootWindow.argtypes=[C.c_void_p];x.XDefaultRootWindow.restype=C.c_ulong
        x.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)];x.XQueryTree.restype=C.c_int
        x.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
        x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];x.XKeysymToKeycode.restype=C.c_uint
        x.XFlush.argtypes=[C.c_void_p]
        xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
        dp=x.XOpenDisplay(display.encode())
        assert dp
        with log.open('w') as output:
            game=subprocess.Popen([str(gamebin),'--diagnostic','--mp-combattest-2'],cwd=d,env=env,stdout=output,stderr=subprocess.STDOUT)
            time.sleep(3.0)
            assert game.poll() is None, 'Game crashed at startup: '+log.read_text()[-1800:]
            root=x.XDefaultRootWindow(dp); rr,parent,count=C.c_ulong(),C.c_ulong(),C.c_uint(); children=C.POINTER(C.c_ulong)()
            assert x.XQueryTree(dp,root,C.byref(rr),C.byref(parent),C.byref(children),C.byref(count)) and count.value
            x.XSetInputFocus(dp,children[count.value-1],1,0);x.XFlush(dp)
            def press(key):
                code=x.XKeysymToKeycode(dp,key);assert code
                xt.XTestFakeKeyEvent(dp,code,1,0);x.XFlush(dp);time.sleep(.08)
                xt.XTestFakeKeyEvent(dp,code,0,0);x.XFlush(dp);time.sleep(.14)
            def wait(msg,timeout=6):
                t=time.time()+timeout
                while time.time()<t:
                    output.flush(); data=log.read_text(errors='replace')
                    if msg in data:return
                    if game.poll() is not None:break
                    time.sleep(.1)
                raise RuntimeError(f'Missing {msg}: game={game.poll()} log: '+data[-2600:])
            wait('MP original tick 20:',12)
            press(0xffbf) # F2 opens in game
            time.sleep(2.2) # gameplay simulation must freeze while F2 is open
            assert 'MP original tick 40:' not in log.read_text(), 'World ticked with F2 open'
            print('PASS modal F2 freezes gameplay ticks.')
            press(0xff52) # cursor 0 -> row8 Start Game
            press(0xff54) # row9 P1 AI
            press(0xff54) # row10 P2 AI
            press(0xff53) # enable P2
            press(0xffbf) # close
            wait('MP P2 AI control enabled live.')
            assert 'MP P1 AI control enabled live.' not in log.read_text()
            print('PASS in-game F2 toggles only P2 AI and gameplay resumes.')
            press(0xffbf)
            press(0xff53) # cursor stays row10, disable P2
            press(0xff1b) # Escape closes menu, should not quit
            wait('MP P2 AI control disabled live.')
            assert game.poll() is None
            print('PASS in-game F2 Escape resumes; P2 AI switch back to human works.')
            press(0xffbf) # F2 reopen from row10
            press(0xff54) # P3 row11
            press(0xff54) # P4 row12
            press(0xff54) # row0 player count
            press(0xff53) # 2 -> 3 players
            press(0xff52) # row8 Start Game
            press(0xff0d) # Start new session
            wait('F2 diagnostic new game restarted with fresh actor/world.',8)
            wait('F2 game launch confirmed: entering 3-player native session.',8)
            assert game.poll() is None
            print('PASS in-game F2 Start Game launches NEW 3-player session.')
            print('LOG',log.read_text(errors='replace')[-1700:])
    finally:
        if game and game.poll() is None:game.terminate();game.wait(timeout=4)
        xvfb.terminate();xvfb.wait(timeout=4)
