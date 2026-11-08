#!/usr/bin/env python3
"""Retail-ROM Xvfb/XTEST proof that F2 > Start Game reaches native session.

Uses a private copy of the compiled PC binary and isolated settings. Never
modifies a user's original settings, installs game assets or ships a ROM.
"""
import argparse
import ctypes as ct
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time


def main():
    a=argparse.ArgumentParser()
    a.add_argument('--exe',required=True,type=Path)
    a.add_argument('--rom',required=True,type=Path)
    a.add_argument('--display',default=':149')
    a.add_argument('--players',type=int,choices=[1,2,3,4],default=3)
    a.add_argument('--timeout',type=float,default=30)
    args=a.parse_args()
    with tempfile.TemporaryDirectory(prefix='re26-f2-start-') as d:
        p=Path(d)
        exe=p/'avp_pc';shutil.copy2(args.exe.resolve(),exe)
        (p/'settings.cfg').write_text('local_multiplayer.enabled=true\nlocal_multiplayer.players=2\nlocal_multiplayer.campaign=0\n')
        log=p/'run.log'
        env=dict(os.environ,DISPLAY=args.display)
        xvfb=subprocess.Popen(['Xvfb',args.display,'-screen','0','1024x768x24','-nolisten','tcp'],
                              stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        game=None
        try:
            time.sleep(0.8)
            x=ct.CDLL('libX11.so.6');xt=ct.CDLL('libXtst.so.6')
            x.XOpenDisplay.argtypes=[ct.c_char_p];x.XOpenDisplay.restype=ct.c_void_p
            x.XDefaultRootWindow.argtypes=[ct.c_void_p];x.XDefaultRootWindow.restype=ct.c_ulong
            x.XQueryTree.argtypes=[ct.c_void_p,ct.c_ulong,ct.POINTER(ct.c_ulong),ct.POINTER(ct.c_ulong),ct.POINTER(ct.POINTER(ct.c_ulong)),ct.POINTER(ct.c_uint)];x.XQueryTree.restype=ct.c_int
            x.XSetInputFocus.argtypes=[ct.c_void_p,ct.c_ulong,ct.c_int,ct.c_ulong]
            x.XKeysymToKeycode.argtypes=[ct.c_void_p,ct.c_ulong];x.XKeysymToKeycode.restype=ct.c_uint
            x.XFlush.argtypes=[ct.c_void_p]
            xt.XTestFakeKeyEvent.argtypes=[ct.c_void_p,ct.c_uint,ct.c_int,ct.c_ulong]
            dp=x.XOpenDisplay(args.display.encode())
            if not dp:raise RuntimeError('XOpenDisplay failed')
            with log.open('w') as output:
                game=subprocess.Popen([str(exe),'--rom',str(args.rom.resolve())],env=env,stdout=output,stderr=subprocess.STDOUT)
                deadline=time.time()+args.timeout
                time.sleep(10)
                root=x.XDefaultRootWindow(dp)
                rr,parent,count=ct.c_ulong(),ct.c_ulong(),ct.c_uint()
                children=ct.POINTER(ct.c_ulong)()
                if not x.XQueryTree(dp,root,ct.byref(rr),ct.byref(parent),ct.byref(children),ct.byref(count)) or not count.value:
                    raise RuntimeError('No game window (status=%r, log=%s)' % (game.poll(),log.read_text(errors='replace')[-3500:]))
                x.XSetInputFocus(dp,children[count.value-1],1,0);x.XFlush(dp)
                def press(k):
                    code=x.XKeysymToKeycode(dp,k)
                    if not code:raise ValueError('Unknown key '+hex(k))
                    xt.XTestFakeKeyEvent(dp,code,1,0);x.XFlush(dp);time.sleep(.10)
                    xt.XTestFakeKeyEvent(dp,code,0,0);x.XFlush(dp);time.sleep(.20)
                press(0xffbf) # F2
                if args.players == 1:press(0xff51) # Left: 2 -> 1
                else:
                    for _ in range(args.players-2):press(0xff53) # Right increments player count
                press(0xff52) # Up from row zero wraps to Start Game
                press(0xff0d) # Enter Start Game
                while time.time()<deadline:
                    output.flush()
                    txt=log.read_text(errors='replace')
                    if 'MP original tick 1:' in txt and 'F2 Start Game:' in txt:
                        assert f'{args.players} players' in txt
                        assert f'F2 game launch confirmed: entering {args.players}-player native session.' in txt
                        print('PASS: actual F2 > Start Game -> original native %d-player live tick' % args.players)
                        return
                    if game.poll() is not None:
                        raise RuntimeError('Process exited prematurely (status %d): %s' % (game.returncode,txt[-3500:]))
                    time.sleep(.35)
                raise RuntimeError('Timed out waiting for F2 launch. Last log:\n'+log.read_text(errors='replace')[-4500:])
        finally:
            if game and game.poll() is None:
                game.terminate()
                try:game.wait(timeout=2)
                except subprocess.TimeoutExpired:game.kill();game.wait()
            xvfb.terminate();xvfb.wait(timeout=3)

if __name__=='__main__':main()
