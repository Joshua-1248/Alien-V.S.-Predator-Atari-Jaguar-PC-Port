#!/usr/bin/env python3
"""R39 source-route audit: all F12 Enhancements and new Video fog option.
This checks that each feature has an explicit, reachable multiplayer route.
It does NOT claim a retail-ROM visual or interaction test was performed.
"""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
video = (root/'src/platform/pc/pc_video_x11.c').read_text()
walk = (root/'new/multiplayer/local_mp_walktest.c').read_text()
command = (root/'new/multiplayer/local_mp_commands.c').read_text()
renderer = (root/'src/jaguar/avp_gpu/software_renderer.c').read_text()
prototype = (root/'src/platform/pc/pc_prototype_textures.c').read_text()

def between(src, start, stop):
    a=src.index(start)
    b=src.index(stop, a+len(start))
    return src[a:b]
view = between(video, 'int avp_pc_video_local_mp_render_actor(', 'int avp_pc_video_local_mp_waveform_step(')
ensure = between(video, 'static int local_mp_view_ensure(', 'int avp_pc_video_local_mp_set_other_players(')
post = between(walk, 'static int walk_post_actor(', 'static int ') if walk.find('static int ',walk.index('static int walk_post_actor(')+20)>0 else walk
# Use the scoped post-actor section independent of later helpers.
post = walk[walk.index('static int walk_post_actor('):walk.index('    UpdtScrOverlays();',walk.index('static int walk_post_actor('))+len('    UpdtScrOverlays();')]
mp_tick = walk[walk.index('            avp_amp_set_marine_pulse_los_hits('):walk.index('            avp_amp_set_facehug_attach_disabled(')]
checklist = [
    ('Smooth Presentation', ['avp_pc_video_smooth_presentation(t->video)' in walk, 'v->cfg.smooth_presentation' in view]),
    ('Game Micro Optimization', ['avp_game_micro_opt_set(v->cfg.game_micro_optimization)' in video,
                                 'amp_capture_direct.inc' in (root/'new/multiplayer/local_mp_amp_native.inc').read_text()]),
    ('High Resolution World', ['v->cfg.high_resolution_world' in ensure,'wanted_w=rect[player].width;wanted_h=rect[player].height;' in ensure]),
    ('Extend Draw Distance', ['state->host_extend_draw_distance=v->cfg.extend_draw_distance' in view,
                               'avp_host_ray_limit(state ? state->host_extend_draw_distance : 0)' in renderer]),
    ('Extend Fog Distance', ['state->host_extend_fog_distance=v->cfg.extend_fog_distance' in view,
                              'avp_host_fog_distance(d,state ? state->host_extend_fog_distance : 0)' in renderer]),
    ('Prototype Texture Restoration', ['*s=v->gpu' in ensure,
                                       'avp_pc_prototype_textures_apply_search' in video,
                                       'memcpy(im->rgb,pixels,AVP_PC_PROTOTYPE_RGB_BYTES)' in prototype]),
    ('Unlimited Shooting Range', ['avp_pc_video_unlimited_shooting_range(t->video)' in post]),
    ('Buffer Short Button Presses', ['avp_pc_video_simulation_pad(video,pad)' in walk,
                                      'avp_mp_enhancement_buffer_poll(' in walk,
                                      'avp_mp_enhancement_buffer_take(' in walk]),
    ('Enhanced Analog Controls', ['player==0 ? avp_pc_video_enhanced_analog_enabled(v)' in view,
                                    'v->gamepad_enhanced_analog_controls' in command,
                                    't->p1_enhanced_aim=avp_pc_video_enhanced_analog_enabled(video)' in walk]),
    ('Buffer Short Analog Controls', ['avp_pc_video_buffer_short_analog(video)' in walk,
                                       'avp_pc_analog_buffer_take(' in (root/'new/multiplayer/local_mp_enhancement_buffer.h').read_text()]),
    ('Marine Line-of-Sight Hits', ['avp_amp_set_marine_pulse_los_hits(' in mp_tick]),
    ('Marine Fire Interrupt', ['avp_amp_set_marine_fire_interrupt(' in mp_tick]),
    ('Remember Explored Automap', ['avp_gpu_automap_history_enter(&v->local_mp_automap_history[player]' in video,
                                      'v->local_mp_controls.player[player].remember_automap' in video]),
    ('Video: Disable Fog', ['state->host_disable_fog=v->cfg.disable_fog' in view,
                             'state && state->host_disable_fog' in renderer,
                             'p->state && p->state->host_disable_fog' in renderer]),
    ('Misc: Fixed HUD Hue', ['state->hud_fixed_hue=(unsigned char)!!v->cfg.hud_fixed_hue' in view]),
]
failed = [name for name,checks in checklist if not all(checks)]
for name,checks in checklist:
    print(f"{'PASS' if all(checks) else 'FAIL'}  {name}  ({sum(checks)}/{len(checks)} route checks)")
if failed:
    print('Missing multiplayer routes: '+', '.join(failed),file=sys.stderr)
    sys.exit(1)
print(f'PASS {len(checklist)} enhancement/video multiplayer source routes')
