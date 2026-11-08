#!/usr/bin/env python3
"""Verify R246 guard accepts working PSVO host pre-copy and rejects regressions.

Offline quality gate: no ROM, hardware access, instrumentation, or game edits.
"""
from __future__ import annotations
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from runpy import run_path

root = Path(__file__).resolve().parents[1]
with redirect_stdout(StringIO()) as captured:
    auditor = run_path(str(root/'tools/audit_architecture.py'))
report = captured.getvalue()
assert '[FAIL]' not in report and 'PASS: A1 Jaguar compatibility ownership seams remain intact' in report
valid = auditor['pause_psvo_bridge_is_narrow']
source = (root/'src/platform/pc/pc_video_x11.c').read_text()
assert valid(source), 'Working host pause bridge rejected'
invalid = {
    'second PSVO event handler': source+'\n0x5053564fu',
    'lost high-resolution condition': source.replace('v->presenting_world_hires &&', '1 &&'),
    'missing native pre-copy': source.replace('avp_pc_scale_nearest32(v->world_fb.pixels,','avp_pc_scale_other(v->world_fb.pixels,'),
    'lost Jaguar OP delegation': source.replace('avp_jaguar_pause_op_event(&v->gpu.pause_op,event,a,b,c,','avp_other_pause_op_event(&v->gpu.pause_op,event,a,b,c,'),
}
for label, mutated in invalid.items():
    assert mutated != source and not valid(mutated), f'Guard permitted {label}'
print('PASS R246 architecture checker on working source; 4 targeted invalid variants rejected')
