#!/usr/bin/env python3
"""Ensure unfinished-code marker audit detects deferred work but not X11 queues."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
validator = (ROOT / "tools" / "validate.sh").read_text()
match = re.search(r"grep -RniE '([^']+)'", validator)
assert match, "validation marker check missing"
pattern = match.group(1)
assert '"$ROOT/new"' in validator, "PC-only modules omitted from marker audit"
assert 'bash "$ROOT/tests/run_tests.sh"' in validator, (
    "standalone suite must use bash explicitly: archive may not retain execute permission")
assert re.search(r'\$BUILD/libavp_c\.a"\s+-Wl,--no-whole-archive\s+-lm\s+-o', validator), (
    "whole-archive game link must include libm after the archive (floor/fmod)")

positive = (
    "/* TODO: reimplement */",
    "// FIXME: incomplete data",
    "/* placeholder until hardware timing is known */",
    "/* stub implementation */",
    "/* stub routine */",
    "/* queued for later */",
    "/* queued for future reimplementation */",
    "/* queued for implementation */",
)
negative = (
    "/* XIfEvent leaves input/window events queued for the normal event loop. */",
    "/* message queued for the next draw */",
    "/* GPU requests queued for immediate execution */",
    "/* all implementation markers resolved */",
)
for want, cases in ((True, positive), (False, negative)):
    for sample in cases:
        result = subprocess.run(("grep", "-niE", pattern), input=sample + "\n",
                                text=True, capture_output=True, check=False)
        detected = result.returncode == 0
        assert result.returncode in (0, 1), result.stderr
        assert detected == want, f"unexpected audit classification: {sample}"

# Catch any real active-source markers, not just the handcrafted examples.
r = subprocess.run(("grep", "-RniE", pattern, str(ROOT / "src"),
                    str(ROOT / "include"), str(ROOT / "new")),
                   text=True, capture_output=True, check=False)
assert r.returncode == 1, f"unfinished active-code marker or grep error:\n{r.stdout}{r.stderr}"
print("R247 unfinished-marker audit: deferred-work cases detected, benign X11 queues allowed, active tree clean")
