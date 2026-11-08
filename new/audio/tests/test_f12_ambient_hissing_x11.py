#!/usr/bin/env python3
"""Optional retail-ROM Xvfb smoke test of F12 Audio hissing controls.

Requires python-xlib + XTest and a user-owned retail Atari Jaguar AVP ROM.
The ROM is loaded by the PC port but is never bundled in a release.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--display', default=':116')
    args = parser.parse_args()

    from Xlib import X, XK, display as xd
    from Xlib.ext import xtest

    with tempfile.TemporaryDirectory(prefix='avp-r52-audio-') as tmp:
        folder = Path(tmp)
        exe = folder / 'avp_pc'
        shutil.copy2(args.exe, exe)
        settings = folder / 'settings.cfg'
        (folder / 'Xauthority').touch()
        os.environ['XAUTHORITY'] = str(folder / 'Xauthority')
        xvfb = subprocess.Popen(['Xvfb', args.display, '-ac', '-screen', '0',
                                 '1024x768x24', '-nolisten', 'tcp'],
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        game = None
        d = None
        try:
            env = os.environ.copy()
            env['DISPLAY'] = args.display
            time.sleep(0.8)
            game = subprocess.Popen([str(exe), '--rom', str(args.rom.resolve()),
                                     '--log', str(folder / 'game.log')],
                                    cwd=folder, env=env,
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.STDOUT)
            d = xd.Display(args.display)
            time.sleep(4.0)
            assert game.poll() is None, 'game exited before F12 Audio test'
            root = d.screen().root
            children = root.query_tree().children
            assert children, 'game window not found'
            win = children[-1]
            win.set_input_focus(X.RevertToParent, X.CurrentTime)
            d.sync()

            def press(sym):
                code = d.keysym_to_keycode(XK.string_to_keysym(sym))
                assert code, f'missing keysym {sym}'
                xtest.fake_input(d, X.KeyPress, code)
                d.sync()
                time.sleep(0.07)
                xtest.fake_input(d, X.KeyRelease, code)
                d.sync()
                time.sleep(0.10)

            def saved(key, value):
                assert settings.exists(), 'Audio menu did not save settings.cfg'
                cfg = dict(line.split('=', 1) for line in settings.read_text().splitlines()
                           if '=' in line)
                assert cfg.get(key) == value, f'{key}: {cfg.get(key)} != {value}'

            press('F12')
            press('Down')
            press('Down')
            press('Return')  # F12 -> Audio; cursor row 0
            for _ in range(6): press('Down')
            press('Left')    # Ambient Hissing Noise Volume 100 -> 99
            saved('audio.ambient_hissing_noise_volume', '99')
            saved('audio.ambience', 'true')
            saved('audio.ambient_hissing_noises', 'true')
            for _ in range(4): press('Down')
            press('Return')  # Ambient Hissing Noises -> Off
            saved('audio.ambient_hissing_noises', 'false')
            saved('audio.ambience', 'true')
            press('Up')
            press('Return')  # Ambience -> Off; hissing remains Off
            saved('audio.ambience', 'false')
            saved('audio.ambient_hissing_noises', 'false')
            press('Down')
            press('Return')  # Ambient Hissing Noises -> On; ambience stays Off
            saved('audio.ambience', 'false')
            saved('audio.ambient_hissing_noises', 'true')
            print('PASS: retail-ROM F12 Audio hissing controls and independent persistence')
        finally:
            if d:
                d.close()
            if game and game.poll() is None:
                game.kill()
                game.wait()
            xvfb.terminate()
            xvfb.wait()


if __name__ == '__main__':
    main()
