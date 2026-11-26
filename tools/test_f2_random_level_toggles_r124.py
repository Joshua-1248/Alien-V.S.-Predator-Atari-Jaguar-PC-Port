#!/usr/bin/env python3
"""ROM-backed X11 test of both F2 random level settings (no gameplay modifications)."""
import argparse
import ctypes as C
import os, shutil, subprocess, time
from pathlib import Path
from PIL import ImageGrab

parser=argparse.ArgumentParser(description='Run real F2 random-level toggle UI checks under Xvfb')
parser.add_argument('--rom',required=True,type=Path,help='Path to your original retail AVP.jag (never modified)')
parser.add_argument('--gcc',required=True,type=Path,help='Fresh GCC avp_pc executable')
parser.add_argument('--clang',required=True,type=Path,help='Fresh Clang avp_pc executable')
parser.add_argument('--workdir',required=True,type=Path,help='Writable output directory for test captures and settings')
parser.add_argument('--display',default=':191',help='Unused X11 display number')
args=parser.parse_args()
ROM=args.rom.resolve();ROOT=args.workdir.resolve();D=args.display
assert ROM.is_file(),ROM
for executable in (args.gcc,args.clang):assert executable.is_file(),executable
ROOT.mkdir(parents=True,exist_ok=True)
lib=C.CDLL('libX11.so.6');xt=C.CDLL('libXtst.so.6')
lib.XOpenDisplay.argtypes=[C.c_char_p];lib.XOpenDisplay.restype=C.c_void_p
lib.XDefaultRootWindow.argtypes=[C.c_void_p];lib.XDefaultRootWindow.restype=C.c_ulong
lib.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)];lib.XQueryTree.restype=C.c_int
lib.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
lib.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];lib.XKeysymToKeycode.restype=C.c_uint
lib.XFlush.argtypes=[C.c_void_p]
xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
keys={'F2':0xffbf,'Up':0xff52,'Down':0xff54,'Right':0xff53,'Left':0xff51,'Enter':0xff0d}
xvfb=subprocess.Popen(['Xvfb',D,'-screen','0','1280x800x24','-nolisten','tcp'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

def get_settings(p):
    result={}
    if p.exists():
        for line in p.read_text().splitlines():
            if '=' in line:
                k,v=line.split('=',1);result[k]=v
    return result

def focus(dp):
    root=lib.XDefaultRootWindow(dp);rr=C.c_ulong();pp=C.c_ulong();count=C.c_uint();children=C.POINTER(C.c_ulong)()
    if not lib.XQueryTree(dp,root,C.byref(rr),C.byref(pp),C.byref(children),C.byref(count)) or not count.value:
        raise AssertionError('no X11 window to focus')
    lib.XSetInputFocus(dp,children[count.value-1],1,0);lib.XFlush(dp)

def key(dp,k):
    c=lib.XKeysymToKeycode(dp,keys[k]);assert c,k
    xt.XTestFakeKeyEvent(dp,c,1,0);lib.XFlush(dp);time.sleep(.07)
    xt.XTestFakeKeyEvent(dp,c,0,0);lib.XFlush(dp);time.sleep(.13)

def run(compiler,dp):
    d=ROOT/('LIVE_F2_'+compiler);d.mkdir(exist_ok=True)
    exe=d/'avp_pc';shutil.copy2(args.gcc if compiler=='gcc' else args.clang,exe)
    cfg=d/'settings.cfg'
    cfg.write_text('local_multiplayer.enabled=true\nlocal_multiplayer.players=2\nlocal_multiplayer.game_mode=1\nlocal_multiplayer.spawn_in_random_level=false\nlocal_multiplayer.respawn_on_random_level=false\n')
    out=d/'stdouterr.txt';env=dict(os.environ,DISPLAY=D)
    with out.open('w') as f:
        proc=subprocess.Popen([str(exe),'--rom',str(ROM),'--log',str(d/'avp_pc.log')],cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT)
        try:
            time.sleep(9.0);assert proc.poll() is None, f'game quit early: {proc.returncode}'
            focus(dp)
            for k in ('F2','Up','Up','Up','Enter','Down'):
                key(dp,k)
            # At the Spawn option; values must be disabled before adjusting.
            ImageGrab.grab(xdisplay=D).save(str(d/'f2_initial.png'))
            assert get_settings(cfg).get('local_multiplayer.spawn_in_random_level')=='false', 'unexpected spawn default'
            key(dp,'Right')
            a=get_settings(cfg)
            assert a.get('local_multiplayer.spawn_in_random_level')=='true', f'keyboard Right failed for initial-spawn toggle: {a}'
            assert a.get('local_multiplayer.respawn_on_random_level')=='false','initial toggle changed respawn'
            key(dp,'Down');key(dp,'Right')
            a=get_settings(cfg)
            assert a.get('local_multiplayer.spawn_in_random_level')=='true' and a.get('local_multiplayer.respawn_on_random_level')=='true',f'keyboard Right failed for respawn toggle: {a}'
            ImageGrab.grab(xdisplay=D).save(str(d/'f2_both_enabled.png'))
            key(dp,'Left')
            a=get_settings(cfg)
            assert a.get('local_multiplayer.spawn_in_random_level')=='true' and a.get('local_multiplayer.respawn_on_random_level')=='false',f'keyboard Left failed for respawn toggle: {a}'
            key(dp,'Up');key(dp,'Left')
            a=get_settings(cfg)
            assert a.get('local_multiplayer.spawn_in_random_level')=='false' and a.get('local_multiplayer.respawn_on_random_level')=='false',f'keyboard Left failed for initial-spawn toggle: {a}'
            ImageGrab.grab(xdisplay=D).save(str(d/'f2_both_disabled.png'))
            print(compiler,'PASS: independently toggled both options with keyboard Right and Left; saved values persisted')
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=3)
                except subprocess.TimeoutExpired:proc.kill()

try:
    time.sleep(.8)
    dp=lib.XOpenDisplay(D.encode())
    if not dp:raise RuntimeError('unable to open X display')
    for comp in ('gcc','clang'):
        run(comp,dp)
finally:
    xvfb.terminate();xvfb.wait(timeout=5)
