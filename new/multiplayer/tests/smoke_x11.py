#!/usr/bin/env python3
"""Optional Linux Xvfb/XTEST smoke test: requires user-provided original AVP.jag.

python3 new/multiplayer/tests/smoke_x11.py --exe build-gcc/avp_pc --rom /path/to/AVP.jag

Tests actual F12->Misc->enable, title F2 setup and settings persistence.
Preserves any existing settings.cfg alongside the executable.
"""
import argparse
import ctypes as ct
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--display', default=':113')
    ap.add_argument('--settle-seconds', type=float, default=6.0)
    args = ap.parse_args()
    exe = args.exe.resolve()
    if not exe.is_file() or not args.rom.is_file():
        ap.error('exe and original retail ROM must exist')
    setting = exe.parent / 'settings.cfg'
    original = setting.read_bytes() if setting.exists() else None
    # Normalize only this temporary test's local-MP options; restore exact bytes.
    if original is not None:
        kept = [line for line in original.decode('utf-8', errors='replace').splitlines()
                if not line.startswith('local_multiplayer.')]
        setting.write_text('\n'.join(kept) + '\nlocal_multiplayer.enabled=false\n'
                           'local_multiplayer.players=2\nlocal_multiplayer.campaign=0\n')
    env = dict(os.environ, DISPLAY=args.display)
    xvfb = subprocess.Popen(['Xvfb', args.display, '-screen', '0', '1024x768x24',
                              '-nolisten', 'tcp'], stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
    game = None
    try:
        with tempfile.TemporaryDirectory() as d:
            time.sleep(1)
            with open(Path(d) / 'output.txt', 'w') as log:
                game = subprocess.Popen([str(exe), '--rom', str(args.rom.resolve()),
                                         '--log', str(Path(d) / 'avp.log')], env=env,
                                        stdout=log, stderr=subprocess.STDOUT)
                x = ct.CDLL('libX11.so.6')
                xt = ct.CDLL('libXtst.so.6')
                x.XOpenDisplay.argtypes = [ct.c_char_p]
                x.XOpenDisplay.restype = ct.c_void_p
                dp = x.XOpenDisplay(args.display.encode())
                if not dp:
                    raise RuntimeError('could not open Xvfb display')
                x.XDefaultRootWindow.argtypes = [ct.c_void_p]
                x.XDefaultRootWindow.restype = ct.c_ulong
                x.XQueryTree.argtypes = [ct.c_void_p, ct.c_ulong,
                    ct.POINTER(ct.c_ulong), ct.POINTER(ct.c_ulong),
                    ct.POINTER(ct.POINTER(ct.c_ulong)), ct.POINTER(ct.c_uint)]
                x.XQueryTree.restype = ct.c_int
                x.XSetInputFocus.argtypes = [ct.c_void_p, ct.c_ulong, ct.c_int, ct.c_ulong]
                x.XKeysymToKeycode.argtypes = [ct.c_void_p, ct.c_ulong]
                x.XKeysymToKeycode.restype = ct.c_uint
                x.XFlush.argtypes = [ct.c_void_p]
                xt.XTestFakeKeyEvent.argtypes = [ct.c_void_p, ct.c_uint, ct.c_int, ct.c_ulong]

                def press(symbol):
                    code = x.XKeysymToKeycode(dp, symbol)
                    if not code:
                        raise RuntimeError('key unavailable in X keyboard map')
                    xt.XTestFakeKeyEvent(dp, code, 1, 0)
                    x.XFlush(dp)
                    time.sleep(.10)
                    xt.XTestFakeKeyEvent(dp, code, 0, 0)
                    x.XFlush(dp)
                    time.sleep(.23)

                time.sleep(args.settle_seconds)
                if game.poll() is not None:
                    raise RuntimeError('game exited before the title screen')
                root = x.XDefaultRootWindow(dp)
                rr, parent, count = ct.c_ulong(), ct.c_ulong(), ct.c_uint()
                children = ct.POINTER(ct.c_ulong)()
                if not x.XQueryTree(dp, root, ct.byref(rr), ct.byref(parent),
                                    ct.byref(children), ct.byref(count)) or not count.value:
                    raise RuntimeError('game window not found')
                x.XSetInputFocus(dp, children[count.value - 1], 1, 0)
                x.XFlush(dp)
                time.sleep(.5)
                press(0xffc9)       # F12
                for _ in range(3): press(0xff54)  # Down x3: Misc
                press(0xff0d)       # Enter Misc
                press(0xff52)       # Up wraps to Local Multiplayer
                press(0xff0d)       # Enable
                press(0xffc9)       # Close F12
                press(0xffbf)       # F2: Local Multiplayer
                press(0xff53)       # Right: 2 -> 3 players
                press(0xff54)       # Down: campaign
                press(0xff53)       # Right: Marine -> Alien campaign
                press(0xff1b)       # Esc: return to untouched original title
                if not setting.exists():
                    raise AssertionError('settings file not saved')
                cfg = setting.read_text()
                for key in ('local_multiplayer.enabled=true',
                            'local_multiplayer.players=3',
                            'local_multiplayer.campaign=1'):
                    if key not in cfg:
                        raise AssertionError('missing expected setting: ' + key)
                # Reopen F12 and select Controls -> Local Players (2-4).
                press(0xffc9)       # F12
                press(0xff0d)       # Controls
                for _ in range(4): press(0xff54)  # Down to Local Players
                press(0xff0d)       # Enter Player Controls
                for _ in range(15): press(0xff54) # Scroll to Forward action
                press(0xff0d)       # Capture P2 keyboard binding
                press(ord('j'))     # Set P2 Forward = J, leaving P1 original alone
                cfg = setting.read_text()
                if 'local_multiplayer.input.p2.key_0=0x6a' not in cfg:
                    raise AssertionError('P2 keyboard capture did not persist; log: '+(Path(d)/'output.txt').read_text(errors='replace')[-2500:])
                if 'keyboard.forward=0x77' not in cfg:
                    raise AssertionError('Player 1 keyboard forward changed unexpectedly')
                print('PASS: actual X11 F12 enable -> F2 setup -> F12 P2 controls -> keyboard persistence')
    finally:
        if game and game.poll() is None:
            game.kill()
            game.wait()
        xvfb.terminate()
        xvfb.wait()
        if original is not None:
            setting.write_bytes(original)
        elif setting.exists():
            setting.unlink()


if __name__ == '__main__':
    main()
