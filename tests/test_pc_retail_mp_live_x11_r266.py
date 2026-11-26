#!/usr/bin/env python3
"""R266: Verify retail-ROM four-player simulation really starts.

The game's multiplayer diagnostics write to *stderr*, not the --log file.
Capture stdout + stderr together. The R265 sweep looked in --log only and
incorrectly reported zero multiplayer ticks even when tick 80 was present.
This test requires an externally supplied canonical retail ROM and Xvfb;
no copyrighted ROM data is bundled with the source/release.
"""
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import time

EXPECTED_HASH = "b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b"
WAIT_SECONDS = 22.0
MIN_TICK = 20


def main():
    if len(sys.argv) != 3:
        print("usage: test_pc_retail_mp_live_x11_r266.py AVP_EXECUTABLE AVP.jag", file=sys.stderr)
        return 2
    exe, rom = (str(pathlib.Path(s).resolve()) for s in sys.argv[1:])
    if not os.environ.get("DISPLAY"):
        print("X11 DISPLAY not set", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="avp_r266_retail_mp_") as sandbox:
        log_path = pathlib.Path(sandbox) / "combined_stdout_stderr.log"
        env = os.environ.copy()
        env["HOME"] = sandbox
        env["XDG_CACHE_HOME"] = str(pathlib.Path(sandbox) / "cache")
        env["XDG_CONFIG_HOME"] = str(pathlib.Path(sandbox) / "config")
        with log_path.open("w+") as log:
            process = subprocess.Popen(
                [exe, "--rom", rom, "--diagnostic", "--mp-combattest-4"],
                stdout=log, stderr=subprocess.STDOUT, cwd=sandbox, env=env,
            )
            cursor = 0
            evidence = {"hash": False, "mp_start": False, "tick": 0, "four_players": False}
            observed = ""
            deadline = time.monotonic() + WAIT_SECONDS
            try:
                while time.monotonic() < deadline:
                    log.flush()
                    log.seek(cursor)
                    new = log.read()
                    cursor = log.tell()
                    observed += new
                    # Bound memory and retain enough to catch line fragments.
                    if len(observed) > 250000:
                        observed = observed[-250000:]
                    if "ROM SHA-256: " + EXPECTED_HASH in observed:
                        evidence["hash"] = True
                    if "RE27 native combat: 4 players" in observed:
                        evidence["mp_start"] = True
                    matches = list(re.finditer(r"MP original tick (\d+):([^\n]*)", observed))
                    if matches:
                        tick, line = int(matches[-1].group(1)), matches[-1].group(2)
                        evidence["tick"] = max(evidence["tick"], tick)
                        if all("P%d " % p in line for p in range(1, 5)):
                            evidence["four_players"] = True
                    if (evidence["hash"] and evidence["mp_start"] and
                        evidence["four_players"] and evidence["tick"] >= MIN_TICK):
                        print("PASS: canonical retail ROM, four native players, original tick %d" % evidence["tick"])
                        return 0
                    if process.poll() is not None:
                        break
                    time.sleep(0.12)
                print("FAIL: startup evidence absent or game terminated: %r, exit=%r" %
                      (evidence, process.poll()), file=sys.stderr)
                print(observed[-3500:], file=sys.stderr)
                return 1
            finally:
                if process.poll() is None:
                    process.terminate()  # deliberate test shutdown, not a crash
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=3)


if __name__ == "__main__":
    sys.exit(main())
