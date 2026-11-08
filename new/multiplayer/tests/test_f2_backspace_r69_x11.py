#!/usr/bin/env python3
"""R69: retail-ROM Xvfb/XTest proof Backspace returns F2 submenus to root.

Isolates settings and executable in a temporary directory. The original Jaguar
ROM is provided as an argument, never written to the release or test directory.
"""
import argparse
import ctypes as C
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import time
from PIL import ImageGrab, ImageChops


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--display', default=':169')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='avp-r69-backspace-') as temp:
        d = Path(temp)
        exe = d/'avp_pc'
        shutil.copy2(args.exe.resolve(), exe)
        (d/'settings.cfg').write_text('local_multiplayer.enabled=true\nlocal_multiplayer.players=2\n')
        env = dict(os.environ, DISPLAY=args.display)
        xvfb = subprocess.Popen(['Xvfb', args.display, '-screen', '0', '1024x768x24', '-nolisten', 'tcp'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        game = None
        try:
            time.sleep(0.6)
            x = C.CDLL('libX11.so.6')
            xt = C.CDLL('libXtst.so.6')
            x.XOpenDisplay.argtypes = [C.c_char_p]
            x.XOpenDisplay.restype = C.c_void_p
            x.XDefaultRootWindow.argtypes = [C.c_void_p]
            x.XDefaultRootWindow.restype = C.c_ulong
            x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
            x.XQueryTree.restype = C.c_int
            x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            x.XFlush.argtypes = [C.c_void_p]
            xt.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            dp = x.XOpenDisplay(args.display.encode())
            assert dp, 'X11 display not ready'
            with (d/'run.log').open('w') as log:
                game = subprocess.Popen([str(exe), '--rom', str(args.rom.resolve())],
                                        cwd=d, env=env, stdout=log, stderr=subprocess.STDOUT)
                time.sleep(8)
                assert game.poll() is None, 'Game exited before F2 title menu'
                root = x.XDefaultRootWindow(dp)
                rr, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                assert x.XQueryTree(dp, root, C.byref(rr), C.byref(parent), C.byref(children), C.byref(count))
                assert count.value, 'Game window not found'
                x.XSetInputFocus(dp, children[count.value - 1], 1, 0)
                x.XFlush(dp)
                def press(key):
                    code = x.XKeysymToKeycode(dp, key)
                    assert code, hex(key)
                    xt.XTestFakeKeyEvent(dp, code, 1, 0)
                    x.XFlush(dp)
                    time.sleep(0.10)
                    xt.XTestFakeKeyEvent(dp, code, 0, 0)
                    x.XFlush(dp)
                    time.sleep(0.23)
                def capture():
                    time.sleep(0.24)
                    return ImageGrab.grab(bbox=(12, 12, 720, 230), xdisplay=args.display).convert('RGB')
                def same_root(before, after, name):
                    # The box is entirely black under menu text; the original
                    # animated title cannot contribute moving pixels here.
                    diff = ImageChops.difference(before, after)
                    changed = sum(1 for pixel in diff.get_flattened_data() if pixel != (0, 0, 0))
                    fraction = changed / (diff.width * diff.height)
                    assert fraction < 0.015, f'{name}: failed to return to parent, diff={fraction:.3%}'
                    print(f'PASS: F2 {name} Backspace returns to root (pixel delta {fraction:.3%})')
                # Root selection begins at Number of Players. Up x4 selects Bots.
                press(0xffbf)  # F2
                for _ in range(4):
                    press(0xff52)  # Up
                for name, moves in [('Bots', 0), ('Modifiers', 1), ('Options', 1)]:
                    for _ in range(moves):
                        press(0xff54)  # Down
                    root_img = capture()
                    press(0xff0d)  # Enter into submenu
                    submenu_img = capture()
                    assert ImageChops.difference(root_img, submenu_img).getbbox(), f'{name}: submenu did not open'
                    press(0xff08)  # Backspace
                    same_root(root_img, capture(), name)
                # Backspace at root closes the F2 overlay; no game starts.
                root_img = capture()
                press(0xff08)
                closed_img = capture()
                assert ImageChops.difference(root_img, closed_img).getbbox(), 'Backspace did not close F2 root'
                assert game.poll() is None
                print('PASS: F2 root Backspace closes overlay without starting a game')
        finally:
            if game and game.poll() is None:
                game.terminate()
                try:game.wait(timeout=3)
                except subprocess.TimeoutExpired:game.kill();game.wait()
            xvfb.terminate()
            try:xvfb.wait(timeout=3)
            except subprocess.TimeoutExpired:xvfb.kill();xvfb.wait()

if __name__ == '__main__':
    main()
