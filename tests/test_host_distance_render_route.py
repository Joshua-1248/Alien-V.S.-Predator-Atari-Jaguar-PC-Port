#!/usr/bin/env python3
"""R43 regression: real ray, flat, sprite and shading call sites preserve all
multi-step PC mode values. The R39 bug used logical AND, collapsing 2..4 to 1.
Checks the compiled renderer's source and the SP/MP video wiring, not just
an isolated policy helper. No ROM is needed.
"""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
renderer=(root/'src/jaguar/avp_gpu/software_renderer.c').read_text()
video=(root/'src/platform/pc/pc_video_x11.c').read_text()
# No Boolean coercion of a numeric distance mode at the renderer call sites.
assert not re.search(r'&&\s*(?:p->)?state->host_extend_(?:draw|fog)_distance',renderer)
for mode, count in [('draw',3),('fog',2)]:
    name='avp_host_ray_limit' if mode=='draw' else 'avp_host_fog_distance'
    arg=f'host_extend_{mode}_distance'
    hits=re.findall(r'\b'+name+r'\s*\([^;]*?'+arg+r'\s*:\s*0\s*\)',renderer)
    assert len(hits)==count, (name,len(hits),count)
# R43: flats use a shared visible-row policy so their horizon follows the
# numeric draw mode instead of retaining the retail eight-room blank band.
flat_hits=re.findall(r'\bavp_host_flat_row_depth\s*\([^;]*?host_extend_draw_distance\s*:\s*0\s*,\s*&perp\s*\)',renderer)
assert len(flat_hits)==1, len(flat_hits)
flat_policy=(root/'include/jaguar/avp_gpu/host_distance.h').read_text()
assert 'max_depth=avp_host_ray_limit(draw_mode)' in flat_policy
assert 'state->host_extend_draw_distance=v->cfg.extend_draw_distance;' in video
assert 'state->host_extend_fog_distance=v->cfg.extend_fog_distance;' in video
assert 'v->gpu.host_extend_draw_distance=v->cfg.extend_draw_distance;' in video
assert 'v->gpu.host_extend_fog_distance=v->cfg.extend_fog_distance;' in video
# Verify the exact independent settings are available, not boolean toggles.
h=(root/'include/jaguar/avp_gpu/host_distance.h').read_text()
assert '8.0,16.0,24.0,32.0,128.0' in h
assert '1.0,0.5,1.0/3.0,0.25' in h
print('PASS: 3 ray/sprite/zbuffer draw, 1 flat horizon, and 2 fog renderer call sites preserve numeric modes')
print('PASS: SP and MP forward the full configuration values')
