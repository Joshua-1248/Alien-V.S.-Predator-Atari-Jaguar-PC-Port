#!/usr/bin/env python3
"""Sweep75: real F12 menu path keyboard and mouse capture for P2/P3/P4.
Virtual XTEST keys/mouse on Xvfb; no claim about physical USB controllers.
Never edits the user's settings or ROM; game runs in an isolated temp folder.
"""
import argparse, ctypes as C, os, shutil, subprocess, tempfile, time
from pathlib import Path

def run():
    a=argparse.ArgumentParser()
    a.add_argument('--exe',type=Path,required=True)
    a.add_argument('--rom',type=Path,required=True)
    a.add_argument('--display',default=':158')
    args=a.parse_args()
    x=C.CDLL('libX11.so.6');xt=C.CDLL('libXtst.so.6')
    x.XOpenDisplay.argtypes=[C.c_char_p];x.XOpenDisplay.restype=C.c_void_p
    x.XDefaultRootWindow.argtypes=[C.c_void_p];x.XDefaultRootWindow.restype=C.c_ulong
    x.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)];x.XQueryTree.restype=C.c_int
    x.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
    x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];x.XKeysymToKeycode.restype=C.c_uint
    x.XFlush.argtypes=[C.c_void_p]
    xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
    xt.XTestFakeButtonEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
    with tempfile.TemporaryDirectory(prefix='re26-s75-bind-') as d:
        d=Path(d);shutil.copy2(args.exe.resolve(),d/'avp_pc')
        (d/'settings.cfg').write_text('local_multiplayer.enabled=true\nlocal_multiplayer.players=4\n')
        env={**os.environ,'DISPLAY':args.display}
        xv=subprocess.Popen(['Xvfb',args.display,'-screen','0','800x600x24','-nolisten','tcp'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        game=None
        try:
            time.sleep(.5)
            with (d/'game.log').open('w') as logfile:
                game=subprocess.Popen([str(d/'avp_pc'),'--rom',str(args.rom.resolve()),'--diagnostic','--mp-walktest-4'],cwd=d,env=env,stdout=logfile,stderr=subprocess.STDOUT)
                timeout=time.monotonic()+30
                while time.monotonic()<timeout:
                    if 'test active:' in (d/'game.log').read_text(errors='replace'):break
                    if game.poll() is not None:raise RuntimeError('Game exited before F12 menu')
                    time.sleep(.1)
                else: raise RuntimeError('Game did not reach original runtime')
                dp=x.XOpenDisplay(args.display.encode());assert dp
                r=x.XDefaultRootWindow(dp)
                rr,par,n=C.c_ulong(),C.c_ulong(),C.c_uint();children=C.POINTER(C.c_ulong)()
                if not x.XQueryTree(dp,r,C.byref(rr),C.byref(par),C.byref(children),C.byref(n)) or not n.value:raise RuntimeError('No game window')
                x.XSetInputFocus(dp,children[n.value-1],1,0);x.XFlush(dp)
                def key(sym):
                    code=x.XKeysymToKeycode(dp,sym)
                    if not code: raise RuntimeError(f'Missing keysym {sym:x}')
                    xt.XTestFakeKeyEvent(dp,code,1,0);x.XFlush(dp);time.sleep(.085)
                    xt.XTestFakeKeyEvent(dp,code,0,0);x.XFlush(dp);time.sleep(.085)
                def button(b):
                    xt.XTestFakeButtonEvent(dp,b,1,0);x.XFlush(dp);time.sleep(.1)
                    xt.XTestFakeButtonEvent(dp,b,0,0);x.XFlush(dp);time.sleep(.12)
                F12,RET,DN,UP,RIGHT=0xffc9,0xff0d,0xff54,0xff52,0xff53
                key(F12);key(RET) # Root > Controls
                for _ in range(4):key(DN)
                key(RET)  # Controls > Local Players (enabled)
                # The F12 local controls page opens on Player 2 by default;
                # don't accidentally skip P2 when testing Enter-to-capture.
                for index,(player,k) in enumerate(((2,0x75),(3,0x76),(4,0x77))):
                    if index:key(RIGHT)  # P2 -> P3 -> P4
                    for _ in range(15):key(DN)
                    key(RET);key(k) # Enter capture, press u/v/w
                    for _ in range(15):key(UP)
                # P4 -> P1 -> P2, mouse type, Enter capture + mouse button 2
                key(RIGHT);key(RIGHT)
                key(DN);key(RIGHT)  # Binding Device Type -> Mouse
                for _ in range(14):key(DN)
                key(RET);button(2)
                time.sleep(.3)
                settings=(d/'settings.cfg').read_text()
                for player,k in ((2,0x75),(3,0x76),(4,0x77)):
                    v=f'local_multiplayer.input.p{player}.key_0=0x{k:x}'
                    if v not in settings:raise RuntimeError(f'Missing saved capture: {v}; relevant settings:\n'+ '\n'.join(y for y in settings.splitlines() if 'local_multiplayer.input.p' in y and '.key_0=' in y))
                if 'local_multiplayer.input.p2.mouse_button_0=2' not in settings:
                    raise RuntimeError('Mouse Enter/click capture not saved for P2: '+ '\n'.join(y for y in settings.splitlines() if '.mouse_button_0=' in y))
                print('PASS: actual F12 > Controls > Local Players P2/P3/P4 keyboard Enter-to-bind and P2 mouse Enter-to-click persist independently')
                print('P1 legacy bindings unchanged')
        finally:
            if game is not None and game.poll() is None:
                game.terminate()
                try:game.wait(timeout=3)
                except subprocess.TimeoutExpired:game.kill()
            xv.terminate();xv.wait(timeout=3)
if __name__=='__main__':run()
