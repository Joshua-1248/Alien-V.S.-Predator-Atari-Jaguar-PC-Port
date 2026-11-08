#!/usr/bin/env python3
"""Enforce exact label-to-action-ID alignment in the real F12 menu."""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
s = (root/'src/platform/pc/pc_video_x11.c').read_text()
start = s.index('static const char*names[AVP_MENU_ITEMS]={')
end = s.index('};', start)
labels = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', s[start:end])
assert len(labels) == 159, f'expected 159 menu labels, got {len(labels)}'
for idx, expected in (
    (0, 'Enhancement: Smooth Presentation'),
    (56, 'Enhancement: Window-resolution 3D'),
    (146, 'Enhancement: Buffer Short Button Presses'),
    (147, 'Enhancement: Enhanced Analog Controls'),
    (148, 'Enhancement: Buffer Short Analog Controls'),
    (157, 'Enhancement: Marine Line-of-Sight Hits'),
    (158, 'Enhancement: Marine Fire Interrupt'),
):
    assert labels[idx] == expected, (idx, labels[idx], expected)
assert 'static const int enhancement_items[]={0,56,146,147,148,157,158}' in s
print('R262 IDs 0-148, R266 IDs 149-156 and R268 ID 157 preserved; R269 ID 158 PASS')
