#!/usr/bin/env python3
"""Optional Linux/X11 two-camera *retail ROM* integration regression.

Run after building avp_pc with AVP_BUILD_LOCAL_MULTIPLAYER=ON:
    python3 new/multiplayer/tests/re26_retail_two_view_smoke.py \
        --exe build/avp_pc --rom /path/to/your/AVP.jag

Requires Xvfb, scrot, libX11, libXtst, Pillow. This does NOT simulate enemies,
combat, audio, gameplay goals, or ROM contents. It does not modify your port's
settings; a temporary directory contains the binary and temporary profile.
"""
import argparse
import ctypes as C
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def pixels(im):
    modern = getattr(im, "get_flattened_data", None)
    return modern() if modern else im.getdata()


def require_dependency(tool):
    if shutil.which(tool) is None:
        raise RuntimeError(f"Required test command not installed: {tool}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output-dir', type=Path, help='Optional screenshot copies')
    parser.add_argument('--combat', action='store_true',
                        help='Opt-in original AMP and weapon-script transaction probe')
    parser.add_argument('--marine-shotgun', action='store_true',
                        help='Enable F12 Misc Marine full-shotgun start and verify original ammo consumption')
    parser.add_argument('--display', default=':93', help='Unused Xvfb display number')
    args = parser.parse_args()
    if args.marine_shotgun and not args.combat:
        parser.error('--marine-shotgun requires --combat')
    from PIL import Image, ImageChops
    for command in ('Xvfb', 'scrot'):
        require_dependency(command)
    exe = args.exe.resolve(strict=True)
    rom = args.rom.resolve(strict=True)
    if not rom.is_file() or rom.stat().st_size != 4194304:
        raise RuntimeError('Must provide your own original 4 MiB Jaguar ROM')
    x11 = C.CDLL('libX11.so.6')
    xtest = C.CDLL('libXtst.so.6')
    x11.XOpenDisplay.restype = C.c_void_p
    x11.XOpenDisplay.argtypes = [C.c_char_p]
    x11.XDefaultRootWindow.restype = C.c_ulong
    x11.XDefaultRootWindow.argtypes = [C.c_void_p]
    x11.XQueryTree.restype = C.c_int
    x11.XQueryTree.argtypes = [C.c_void_p, C.c_ulong,
                               C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                               C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
    x11.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
    x11.XStringToKeysym.restype = C.c_ulong
    x11.XStringToKeysym.argtypes = [C.c_char_p]
    x11.XKeysymToKeycode.restype = C.c_uint
    x11.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
    x11.XFlush.argtypes = [C.c_void_p]
    xtest.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]

    with tempfile.TemporaryDirectory(prefix='avp_s66_retail_') as tmp:
        tmp = Path(tmp)
        test_exe = tmp / 'avp_pc'
        shutil.copy2(exe, test_exe)
        # 0=Forward in the independent P2 profile; 0x69 = X11 'i'.
        (tmp / 'settings.cfg').write_text('local_multiplayer.input.p2.key_0=0x69\n'
                                          'local_multiplayer.input.p2.key_6=0x70\n' +
                                          ('misc.marine_start_shotgun=true\n' if args.marine_shotgun else ''))
        env = os.environ.copy()
        env['DISPLAY'] = args.display
        display_process = subprocess.Popen(
            ['Xvfb', args.display, '-screen', '0', '800x600x24', '-nolisten', 'tcp'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        game_process = None
        try:
            time.sleep(0.6)
            with (tmp / 'game.log').open('wb') as logfile:
                game_process = subprocess.Popen(
                    [str(test_exe), '--rom', str(rom), '--diagnostic',
                     '--mp-combattest-2' if args.combat else '--mp-walktest-2'],
                    cwd=tmp, env=env, stdout=logfile, stderr=subprocess.STDOUT)
                dpy = x11.XOpenDisplay(args.display.encode())
                if not dpy:
                    raise RuntimeError('Could not open temporary Xvfb display')
                # Fresh ROM cache extraction and original campaign setup can
                # take longer than a reused developer build; wait for the
                # game's own live-walktest announcement, never a fixed boot
                # sleep that mistakes the initial black screen for a failure.
                deadline = time.monotonic() + 20.0
                while time.monotonic() < deadline:
                    logfile.flush()
                    if 'test active:' in (tmp / 'game.log').read_text(errors='replace'):
                        break
                    if game_process.poll() is not None:
                        raise RuntimeError('Walktest quit during retail ROM initialization')
                    time.sleep(0.2)
                else:
                    raise RuntimeError('Retail walktest never entered its native simulation')
                time.sleep(1.6)  # original 60-Hz fade reaches zero in ~1.1s
                def shot(name):
                    file = tmp / f'{name}.png'
                    subprocess.run(['scrot', '-z', str(file)], env=env,
                                   check=True, stdout=subprocess.DEVNULL)
                    return Image.open(file).convert('RGB')
                original = shot('p2_idle')
                # A window occupies 640x456. Both full game/HUD viewports
                # must contain ROM-derived non-black pixels after the fade.
                def lit(im, bounds):
                    return sum(pixel != (0, 0, 0) for pixel in pixels(im.crop(bounds)))
                left = (0, 0, 320, 456)
                right = (320, 0, 640, 456)
                if lit(original, left) < 4000 or lit(original, right) < 4000:
                    # Save useful evidence before the temporary workdir goes.
                    if args.output_dir:
                        args.output_dir.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(tmp / 'p2_idle.png', args.output_dir / 'p2_black_fail.png')
                        shutil.copy2(tmp / 'game.log', args.output_dir / 'game_fail.log')
                    raise RuntimeError('Retail viewport still black after original MazeList fade')
                def actor_positions():
                    # S73+ renders other actors as moving original-ROM sprites.
                    # Their pixels can change in P1's *stationary* camera.
                    # Read scoped original actor x/y/heading instead of using
                    # a brittle threshold for image differences.
                    logs=(tmp/'game.log').read_text(errors='replace')
                    states=re.findall(r'MP original tick (\d+): (.*)',logs)
                    if not states:
                        raise RuntimeError('Missing per-player actor state logs')
                    positions=re.findall(r'P\d+ ([0-9a-f]{8})/([0-9a-f]{8}) facing ([0-9a-f]{8})',states[-1][1])
                    if len(positions)!=2:
                        raise RuntimeError('Missing two original actor positions')
                    return positions
                initial_positions=actor_positions()
                root = x11.XDefaultRootWindow(dpy)
                r = C.c_ulong(); p = C.c_ulong()
                children = C.POINTER(C.c_ulong)(); n = C.c_uint()
                if not x11.XQueryTree(dpy, root, C.byref(r), C.byref(p),
                                      C.byref(children), C.byref(n)) or not n.value:
                    raise RuntimeError('No actual PC game window')
                x11.XSetInputFocus(dpy, children[n.value-1], 1, 0)
                code = x11.XKeysymToKeycode(dpy, x11.XStringToKeysym(b'i'))
                if not code:
                    raise RuntimeError('No keycode for P2 forward')
                x11.XFlush(dpy)
                xtest.XTestFakeKeyEvent(dpy, code, 1, 0)
                x11.XFlush(dpy)
                time.sleep(2.0)
                moved = shot('p2_forward')
                xtest.XTestFakeKeyEvent(dpy, code, 0, 0)
                x11.XFlush(dpy)
                def diff(bounds):
                    a = original.crop(bounds)
                    b = moved.crop(bounds)
                    return sum(any(rgb) for rgb in pixels(ImageChops.difference(a,b)))
                p1_changed, p2_changed = diff(left), diff(right)
                print(f'RETAIL VIDEO: P1 changed={p1_changed}, P2 changed={p2_changed}; '
                      f'P1 nonblack={lit(original,left)}, P2 nonblack={lit(original,right)}')
                # When original AMP AI is enabled, shared enemies visibly
                # animate/move in BOTH cameras even with P1 standing still.
                # The movement-only baseline still demands exact P1 pixels.
                moved_positions=actor_positions()
                if p2_changed<1000 or moved_positions[0]!=initial_positions[0] or \
                   moved_positions[1][:2]==initial_positions[1][:2]:
                    raise RuntimeError('Original per-player movement/view isolation failed')
                if args.combat:
                    # P2's fire comes from its isolated original Jaguar pad,
                    # never from the P1 legacy keyboard/mouse bindings.
                    fire_code = x11.XKeysymToKeycode(dpy,x11.XStringToKeysym(b'p'))
                    if not fire_code: raise RuntimeError('No P2 fire keycode')
                    xtest.XTestFakeKeyEvent(dpy,fire_code,1,0)
                    x11.XFlush(dpy)
                    time.sleep(2.5)
                    xtest.XTestFakeKeyEvent(dpy,fire_code,0,0)
                    x11.XFlush(dpy)
                    log=(tmp/'game.log').read_text(errors='replace')
                    matches=re.findall(r'MP original combat (\d+): P1 hp=(-?\d+) weapon=(\d+) ammo0=(\d+) fire=(\d+) P2 hp=(-?\d+) weapon=(\d+) ammo0=(\d+) fire=(\d+) original AMP modes=(\d+)',log)
                    if not matches: raise RuntimeError('No original combat-state samples')
                    if not any(int(m[4])==0 and int(m[8])==255 and int(m[9])>0
                               for m in matches):
                        raise RuntimeError('Independent P2 fire or original AMP visits were not observed')
                    if args.marine_shotgun:
                        if not any(int(m[2])==1 and int(m[3])==44 and
                                   int(m[6])==1 and int(m[7])<44 and
                                   int(m[4])==0 and int(m[8])==255 for m in matches):
                            raise RuntimeError('Marine shotgun grant or independent P2 ammo consumption failed')
                        print('PASS: retail 2P shotgun drawn; P2 fired shells, P1 kept all 44')
                    if game_process.poll() is not None:
                        raise RuntimeError('Original combat probe exited unexpectedly')
                    print('PASS: retail AMP modes updated; isolated P2 fire reached original player/weapon state with P1 idle')
                    if args.output_dir:
                        args.output_dir.mkdir(parents=True,exist_ok=True)
                        shutil.copy2(tmp/'game.log',args.output_dir/'combat_probe.log')
                if args.output_dir:
                    args.output_dir.mkdir(parents=True, exist_ok=True)
                    for name in ('p2_idle', 'p2_forward'):
                        shutil.copy2(tmp / f'{name}.png', args.output_dir / f'{name}.png')
                print('PASS: original-ROM 2P display fade, two complete views, independent P2 motion')
        finally:
            if game_process is not None and game_process.poll() is None:
                # Controlled stop after sampling; source logs SIGTERM as a
                # "fatal signal", not a spontaneous crash.
                game_process.terminate()
                try: game_process.wait(timeout=4)
                except subprocess.TimeoutExpired: game_process.kill()
            display_process.terminate()
            try: display_process.wait(timeout=4)
            except subprocess.TimeoutExpired: display_process.kill()


if __name__ == '__main__':
    try: main()
    except Exception as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        sys.exit(1)
