#!/usr/bin/env python3
"""ROM-based X11 integration test for three and four *independent* viewports.

Requires Xvfb, scrot, libX11, libXtst, Pillow and the user's own retail ROM.
No ROM bytes are included. Exercises the unmodified original movement/render
path in the opt-in experimental session; this is not a campaign/co-op test.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
from PIL import Image, ImageChops


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--players', type=int, choices=(3, 4), required=True)
    ap.add_argument('--hires', action='store_true', help='Use F12 window-resolution world rendering for each viewport')
    ap.add_argument('--combat', action='store_true', help='Experimental original AMP and weapon updates; grant shotgun and verify P3/P4 ammo isolation')
    ap.add_argument('--audio-check', action='store_true', help='After combat, fire four shotguns together and verify original weapon audio launches per owner')
    ap.add_argument('--display', default=':96')
    ap.add_argument('--output-dir', type=Path)
    args = ap.parse_args()
    for cmd in ('Xvfb', 'scrot'):
        if not shutil.which(cmd):
            raise RuntimeError('Missing '+cmd)
    if args.rom.stat().st_size != 4194304:
        raise RuntimeError('Expected original 4 MiB retail Jaguar AVP ROM')
    exe = args.exe.resolve(strict=True)
    rom = args.rom.resolve(strict=True)
    x11 = C.CDLL('libX11.so.6')
    xtest = C.CDLL('libXtst.so.6')
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XDefaultRootWindow.argtypes = [C.c_void_p]
    x11.XDefaultRootWindow.restype = C.c_ulong
    x11.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong),
                             C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
    x11.XQueryTree.restype = C.c_int
    x11.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
    x11.XStringToKeysym.argtypes = [C.c_char_p]
    x11.XStringToKeysym.restype = C.c_ulong
    x11.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
    x11.XKeysymToKeycode.restype = C.c_uint
    x11.XFlush.argtypes = [C.c_void_p]
    x11.XResizeWindow.argtypes=[C.c_void_p,C.c_ulong,C.c_uint,C.c_uint]
    xtest.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    with tempfile.TemporaryDirectory(prefix=f'avp_re26_{args.players}p_') as d:
        d = Path(d)
        shutil.copy2(exe, d/'avp_pc')
        (d/'settings.cfg').write_text('local_multiplayer.input.p2.key_0=0x69\n' +
                                      'local_multiplayer.input.p3.key_0=0x74\n' +
                                      'local_multiplayer.input.p4.key_0=0x6f\n' +
                                      'local_multiplayer.input.p2.key_6=0x6a\n' +
                                      'local_multiplayer.input.p3.key_6=0x67\n' +
                                      'local_multiplayer.input.p4.key_6=0x68\n' +
                                      'misc.marine_start_shotgun=true\n' +
                                      ('enhancement.high_resolution_world=true\n' if args.hires else ''))
        env = dict(os.environ, DISPLAY=args.display)
        xvfb = subprocess.Popen(['Xvfb',args.display,'-screen','0','800x600x24','-nolisten','tcp'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        game = None
        dpy = None
        try:
            time.sleep(.5)
            with (d/'game.log').open('wb') as log:
                game = subprocess.Popen([str(d/'avp_pc'),'--rom',str(rom),'--diagnostic',
                                         f'--mp-{"combattest" if args.combat else "walktest"}-{args.players}'],
                                        cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT)
                deadline = time.monotonic()+24
                while time.monotonic()<deadline:
                    if f'active: {args.players} same-species players' in (d/'game.log').read_text(errors='replace'):
                        break
                    if game.poll() is not None: raise RuntimeError('Game exited during startup')
                    time.sleep(.2)
                else: raise RuntimeError('Game never started multi-player session')
                time.sleep(1.6)  # allow original 60 Hz fade
                dpy = x11.XOpenDisplay(args.display.encode())
                if not dpy: raise RuntimeError('XOpenDisplay failed')
                root = x11.XDefaultRootWindow(dpy)
                rr = C.c_ulong(); parent = C.c_ulong()
                children = C.POINTER(C.c_ulong)(); count = C.c_uint()
                if not x11.XQueryTree(dpy,root,C.byref(rr),C.byref(parent),
                                      C.byref(children),C.byref(count)) or not count.value:
                    raise RuntimeError('Game window missing')
                x11.XSetInputFocus(dpy,children[count.value-1],1,0)
                if args.hires:
                    x11.XResizeWindow(dpy,children[count.value-1],800,600)
                x11.XFlush(dpy)
                if args.hires:time.sleep(.7)
                def snapshot(label):
                    target = d/(label+'.png')
                    subprocess.run(['scrot','-z',str(target)],check=True,env=env,
                                   stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    im = Image.open(target).convert('RGB')
                    if args.output_dir:
                        args.output_dir.mkdir(parents=True,exist_ok=True)
                        shutil.copy2(target,args.output_dir/target.name)
                    return im
                ww,hh=(800,600) if args.hires else (640,456)
                areas = [(0,0,ww//2,hh//2),(ww//2,0,ww,hh//2),
                         (0,hh//2,ww//2,hh),(ww//2,hh//2,ww,hh)]
                def pix(im):
                    fn=getattr(im,"get_flattened_data",None)
                    return fn() if fn else im.getdata()
                def lit(im,a):
                    return sum(r!= (0,0,0) for r in pix(im.crop(a)))
                def changed(old,new,a):
                    return sum(any(p) for p in pix(ImageChops.difference(old.crop(a),new.crop(a))))
                prev = snapshot('idle')
                for p in range(args.players):
                    visible = lit(prev,areas[p]); print(f'P{p+1} nonblack={visible}')
                    if visible<1000: raise RuntimeError(f'P{p+1} is black/blank')
                keys={1:b'i',2:b't',3:b'o'}
                # Test backwards: a previously moved player may keep coasting
                # under the ORIGINAL Jaguar friction code after key release.
                # Since S73, the unmoved players' views include the moving
                # participant's REAL original-ROM world sprite. Therefore
                # pixel equality would reject correct shared-world rendering.
                # Compare original per-actor position/heading logs instead.
                def original_positions():
                    lines = re.findall(r'MP original tick (\d+): (.*)',
                                       (d/'game.log').read_text(errors='replace'))
                    if not lines:
                        raise RuntimeError('Missing original per-player movement logs')
                    matches = re.findall(r'P\d+ ([0-9a-f]{8})/([0-9a-f]{8}) facing ([0-9a-f]{8})',
                                         lines[-1][1])
                    if len(matches)!=args.players:
                        raise RuntimeError('Incomplete original actor state log')
                    return int(lines[-1][0]),matches
                _init_tick, initial_positions=original_positions()
                for p in range(args.players-1,0,-1):
                    keycode=x11.XKeysymToKeycode(dpy,x11.XStringToKeysym(keys[p]))
                    if not keycode: raise RuntimeError('X11 key unavailable')
                    xtest.XTestFakeKeyEvent(dpy,keycode,1,0); x11.XFlush(dpy)
                    time.sleep(1.4)
                    after = snapshot(f'p{p+1}_forward')
                    xtest.XTestFakeKeyEvent(dpy,keycode,0,0); x11.XFlush(dpy)
                    delta=[changed(prev,after,a) for a in areas[:args.players]]
                    print(f'P{p+1} alone key: pixel deltas {delta}')
                    if delta[p]<500: raise RuntimeError(f'P{p+1} camera did not move')
                    if not args.combat:
                        # Other actors may be visible in every viewport, so
                        # their world sprites *must* move even if the viewer
                        # stays stationary. Original actor coordinates prove
                        # no input leaked to unpressed P1..P(p-1).
                        current_tick,current_positions=original_positions()
                        if any(current_positions[q]!=initial_positions[q] for q in range(p)):
                            raise RuntimeError(f'P{p+1} input moved an unpressed player actor at tick {current_tick}')
                    if game.poll() is not None: raise RuntimeError('Game terminated unexpectedly')
                    time.sleep(.3)
                    prev=snapshot(f'p{p+1}_released')
                if args.combat:
                    # The new shotgun option initializes all Marine actor
                    # copies. Fire ONLY the last player's original weapon
                    # animation and compare distinct ammo records.
                    firekey=b'g' if args.players==3 else b'h'
                    firecode=x11.XKeysymToKeycode(dpy,x11.XStringToKeysym(firekey))
                    if not firecode: raise RuntimeError('Fire key unavailable')
                    xtest.XTestFakeKeyEvent(dpy,firecode,1,0);x11.XFlush(dpy)
                    time.sleep(4.0)
                    xtest.XTestFakeKeyEvent(dpy,firecode,0,0);x11.XFlush(dpy)
                result=(d/'game.log').read_text(errors='replace')
                states=re.findall(r'MP original tick \d+: (.*)',result)
                if len(states)<2: raise RuntimeError('Insufficient simulation ticks')
                # Every slot must have a distinct position in the same authored world.
                pcoords=re.findall(r'P\d+ ([0-9a-f]{8})/([0-9a-f]{8}) facing',states[0])
                if len(pcoords)!=args.players or len(set(pcoords))!=args.players:
                    raise RuntimeError('Duplicate or absent authored actor positions')
                if not args.combat:
                    _end_tick,end_positions=original_positions()
                    for player in range(1,args.players):
                        if end_positions[player][:2]==initial_positions[player][:2]:
                            raise RuntimeError(f'P{player+1} input did not move the original actor')
                if args.combat:
                    combat=re.findall(r'MP original combat (\d+): (.*)',result)
                    if len(combat)<2: raise RuntimeError('Missing AMP+weapon update samples')
                    visits=re.search(r'original AMP modes=(\d+)',combat[-1][1])
                    updates=re.search(r'weapon updates=(\d+)',combat[-1][1])
                    if not visits or int(visits.group(1))==0 or not updates or int(updates.group(1))<args.players*10:
                        raise RuntimeError('Original AMP/world or per-player weapon updates absent')
                    states=[]
                    for _tick,line in combat:
                        ammo={int(p):(int(a),int(f)) for p,a,f in re.findall(r'P(\d+) hp=-?\d+ weapon=1 ammo0=(\d+) fire=(\d+)',line)}
                        states.append(ammo)
                    if not any(len(sample)==args.players and
                               sample[args.players][0]<44 and
                               all(sample[p][0]==44 and sample[p][1]==0 for p in range(1,args.players))
                               for sample in states):
                        raise RuntimeError('Last player shotgun ammo did not advance independently')
                    print(f'PASS: retail {args.players}P original AMP and independent P{args.players} shotgun firing')
                if args.audio_check:
                    if not args.combat or args.players!=4:
                        raise RuntimeError('--audio-check requires four-player --combat')
                    # Active simultaneously: the original CPU voice 1 is
                    # actor-scoped, the hosted PCM weapon streams have four
                    # separate lanes. Verify sound-launch attribution, not
                    # physical sound quality or damage to enemies.
                    codes=[x11.XKeysymToKeycode(dpy,x11.XStringToKeysym(key))
                           for key in (b'space',b'j',b'g',b'h')]
                    if not all(codes):raise RuntimeError('X11 multi-fire key missing')
                    for code in codes:xtest.XTestFakeKeyEvent(dpy,code,1,0)
                    x11.XFlush(dpy)
                    time.sleep(3.7)
                    for code in codes:xtest.XTestFakeKeyEvent(dpy,code,0,0)
                    x11.XFlush(dpy)
                    time.sleep(.5)
                    audio=(d/'game.log').read_text(errors='replace')
                    launches=re.findall(r'audioP1=(\d+) audioP2=(\d+) audioP3=(\d+) audioP4=(\d+)',audio)
                    if not launches:raise RuntimeError('Missing audio launch counters')
                    counts=tuple(int(x) for x in launches[-1])
                    if not all(x>0 for x in counts):
                        raise RuntimeError(f'Missing per-player original weapon sound launches: {counts}')
                    print('PASS: four simultaneous original weapon-sound owners:',counts)
                print(f'PASS: retail {args.players}-player single-window views, unique spawns, independent P2-P{args.players} input')
                if args.output_dir: shutil.copy2(d/'game.log',args.output_dir/'game.log')
        finally:
            if game is not None and game.poll() is None:
                game.terminate()
                try: game.wait(timeout=3)
                except subprocess.TimeoutExpired: game.kill()
            xvfb.terminate()
            try: xvfb.wait(timeout=3)
            except subprocess.TimeoutExpired: xvfb.kill()

if __name__=='__main__':
    main()
