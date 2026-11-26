#!/usr/bin/env python3
"""RE37 R118: live Xvfb diagnostic proof of spycam switching without changing actor control.

No commercial ROM embedded or copied. --exe points to an already built binary.
Requires Xvfb, Xlib and Xtst. Settings and logs are isolated in a temp dir.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def run(exe, display, rom=None, bind_in_f2=False):
    with tempfile.TemporaryDirectory(prefix='avp-r118-spycam-') as name:
        root = Path(name)
        gamebin = root / 'avp_pc'
        shutil.copy2(exe.resolve(), gamebin)
        (root / 'settings.cfg').write_text(
            'local_multiplayer.enabled=true\n'
            'local_multiplayer.players=2\n' +
            ('' if bind_in_f2 else
             'local_multiplayer.input.p1.key_25=0x7a\n'
             'local_multiplayer.input.p1.key_26=0x78\n')
        )
        env = dict(os.environ, DISPLAY=display, SDL_AUDIODRIVER='dummy')
        xvfb = subprocess.Popen(['Xvfb', display, '-screen', '0', '1024x768x24',
                                 '-nolisten', 'tcp'], stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        game = None
        try:
            time.sleep(.7)
            x = C.CDLL('libX11.so.6')
            xt = C.CDLL('libXtst.so.6')
            x.XOpenDisplay.argtypes = [C.c_char_p]
            x.XOpenDisplay.restype = C.c_void_p
            x.XDefaultRootWindow.argtypes = [C.c_void_p]
            x.XDefaultRootWindow.restype = C.c_ulong
            x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong),
                                    C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)),
                                    C.POINTER(C.c_uint)]
            x.XQueryTree.restype = C.c_int
            x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            x.XFlush.argtypes = [C.c_void_p]
            xt.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            dp = x.XOpenDisplay(display.encode())
            assert dp, 'Xvfb unavailable'
            with (root / 'run.log').open('w') as log:
                command = [str(gamebin), '--rom', str(rom.resolve()), '--mp-combattest-2'] if rom else [str(gamebin), '--diagnostic', '--mp-combattest-2']
                game = subprocess.Popen(command,
                                        cwd=root, env=env, stdout=log,
                                        stderr=subprocess.STDOUT)
                def await_log(term, seconds=14):
                    stop = time.monotonic() + seconds
                    while time.monotonic() < stop:
                        log.flush()
                        data = (root / 'run.log').read_text(errors='replace')
                        if term in data:
                            return data
                        if game.poll() is not None:
                            break
                        time.sleep(.1)
                    raise AssertionError(f'missing {term!r}, exit={game.poll()}\n' + data[-2200:])
                await_log('Retail frontend:' if rom else 'MP original tick 20:')
                rr, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                assert x.XQueryTree(dp, x.XDefaultRootWindow(dp),
                                    C.byref(rr), C.byref(parent),
                                    C.byref(children), C.byref(count)) and count.value
                x.XSetInputFocus(dp, children[count.value - 1], 1, 0)
                x.XFlush(dp)
                def press(sym):
                    code = x.XKeysymToKeycode(dp, sym)
                    assert code
                    xt.XTestFakeKeyEvent(dp, code, 1, 0)
                    x.XFlush(dp)
                    time.sleep(.13)
                    xt.XTestFakeKeyEvent(dp, code, 0, 0)
                    x.XFlush(dp)
                    time.sleep(.23)
                if bind_in_f2:
                    press(0xffbf) # F2
                    press(0xff52) # root -> Start Game
                    press(0xff52) # root -> Options
                    press(0xff0d) # Enter Options
                    press(0xff52) # Options -> Spycam
                    press(0xff0d) # Enter Spycam submenu
                    press(0xff54) # Spy Next Player
                    press(0xff0d) # capture key
                    press(ord('z'))
                    press(0xff54) # Spy Previous Player
                    press(0xff0d) # capture key
                    press(ord('x'))
                    press(0xff08) # Back to Options
                    press(0xff08) # Back to root Options row
                    press(0xff54) # Down to Start Game
                    press(0xff0d) # Launch multiplayer session
                    await_log('MP original tick 20:',25)
                    settings=(root / 'settings.cfg').read_text()
                    assert 'local_multiplayer.input.p1.key_25=0x7a' in settings, 'F2 next binding was not saved'
                    assert 'local_multiplayer.input.p1.key_26=0x78' in settings, 'F2 previous binding was not saved'
                elif rom:
                    press(0xffbf) # F2 at retail title
                    press(0xff52) # root last selectable row: Start Game
                    press(0xff0d) # Enter
                    await_log('MP original tick 20:',25)
                press(ord('z'))
                await_log('MP P1 spycam: observing P2 (remote).')
                press(ord('x'))
                await_log('MP P1 spycam: observing P1 (self).')
                # Three complete presses within one 100-ms Jaguar game tick.
                # This would be lost in R121's tick-sampled spycam path.
                def fast_tap(sym):
                    code = x.XKeysymToKeycode(dp, sym)
                    xt.XTestFakeKeyEvent(dp, code, 1, 0)
                    x.XFlush(dp)
                    time.sleep(.012)
                    xt.XTestFakeKeyEvent(dp, code, 0, 0)
                    x.XFlush(dp)
                    time.sleep(.012)
                before=(root / 'run.log').read_text(errors='replace')
                for _ in range(3): fast_tap(ord('z'))
                def await_three_ordered():
                    deadline=time.monotonic()+5
                    expected=['MP P1 spycam: observing P2 (remote).',
                              'MP P1 spycam: observing P1 (self).',
                              'MP P1 spycam: observing P2 (remote).']
                    while time.monotonic()<deadline:
                        full=(root / 'run.log').read_text(errors='replace')
                        newer=full[len(before):]
                        cursor=0
                        for token in expected:
                            i=newer.find(token,cursor)
                            if i<0:break
                            cursor=i+len(token)
                        else:return
                        time.sleep(.02)
                    raise AssertionError('rapid taps were dropped or reordered: '+newer[-1000:])
                await_three_ordered()
                fast_tap(ord('x'))
                await_log('MP P1 spycam: observing P1 (self).')
                assert game.poll() is None, 'session ended while spying'
                print('PASS live '+('retail-ROM' if rom else 'diagnostic')+' spycam: F2 binding='+str(bind_in_f2)+', P1 -> P2 -> P1; three rapid taps in <100 ms preserved')
                return True
        finally:
            if game and game.poll() is None:
                game.terminate()
                try:
                    game.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    game.kill()
                    game.wait()
            xvfb.terminate()
            try:
                xvfb.wait(timeout=4)
            except subprocess.TimeoutExpired:
                xvfb.kill()
                xvfb.wait()


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', required=True, type=Path)
    ap.add_argument('--display', default=':175')
    ap.add_argument('--rom', type=Path, default=None)
    ap.add_argument('--bind-in-f2', action='store_true', help='Bind both keys via F2 Options > Spycam')
    args = ap.parse_args()
    run(args.exe, args.display, args.rom, args.bind_in_f2)
