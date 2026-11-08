#!/usr/bin/env python3
"""Incremental architecture guard for the RE #10 compatibility migration.

This does not pretend the whole PC backend is clean yet.  It locks in only the
ownership seams already migrated by A1 so later work cannot quietly put those
Jaguar semantics back into the X11/backend files.
"""
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[1]
pc_video=(root/'src/platform/pc/pc_video_x11.c').read_text()
pc_tree='\n'.join(p.read_text(errors='ignore') for p in (root/'src/platform/pc').rglob('*') if p.is_file())


def pause_psvo_bridge_is_narrow(source: str) -> bool:
    """Allow only the host framebuffer pre-copy, never Jaguar pause semantics.

    R178's working high-resolution SaveOvers handoff must be allowed to
    identify the PSVO event so it can snapshot the *displayed* world before
    forwarding the untouched event and native pixels to the Jaguar-owned OP.
    The previous no-PSVO-literal rule incorrectly rejected that host bridge.
    """
    event = '0x5053564fu'
    begin = source.find('void avp_pc_video_object_event(')
    end = source.find('int avp_pc_video_pause_active(', begin)
    if begin < 0 or end <= begin or source.count(event) != 1:
        return False
    body = source[begin:end]
    pre_copy_guard = ('if(event==0x5053564fu && v->presenting_world_hires &&' in body
                      and 'v->world_fb.pixels && v->fb.pixels)' in body)
    scaling = body.find('avp_pc_scale_nearest32(v->world_fb.pixels,')
    forward = body.find('avp_jaguar_pause_op_event(&v->gpu.pause_op,event,a,b,c,')
    return (pre_copy_guard and scaling >= 0 and forward > scaling
            and 'v->fb.pixels,v->fb.width,v->fb.height);' in body[forward:]
            and event not in source[:begin] and event not in source[end:])
checks=[
    ('host Jaguar blitter moved out of platform/pc',not (root/'src/platform/pc/pc_blitter.c').exists() and (root/'src/jaguar/host/blitter_host.c').exists()),
    ('host Jaguar joypad moved out of platform/pc',not (root/'src/platform/pc/pc_joypad.c').exists() and (root/'src/jaguar/host/joypad_host.c').exists()),
    ('host Jaguar/MMIO moved out of platform/pc',not (root/'src/platform/pc/pc_jaguar.c').exists() and (root/'src/jaguar/host/jaguar_host.c').exists()),
    ('AVP Jaguar visual bridge moved out of platform/pc',not (root/'src/platform/pc/pc_visual_bridge.cpp').exists() and (root/'src/jaguar/avp_gpu/visual_bridge.cpp').exists()),
    ('legacy M20 implementation moved out of platform/pc',not (root/'src/platform/pc/m20').exists() and (root/'src/jaguar/avp_gpu/panels.cpp').exists()),
    ('legacy avp_m20 include namespace removed',not (root/'include/avp_m20').exists()),
    ('PC visual bridge header removed',not (root/'include/pc/pc_visual_bridge.h').exists() and (root/'include/jaguar/avp_gpu/bridge.h').exists()),
    ('Jaguar framebuffer module exists',(root/'src/jaguar/hw/framebuffer.c').exists()),
    ('X11 video uses Jaguar framebuffer',('AvpJaguarFramebuffer fb;' in pc_video or 'AvpJaguarFramebuffer fb,world_fb;' in pc_video) and 'uint32_t *pixels,*present_pixels' not in pc_video),
    ('pause OP fourcc PSPR','0x50535052u' not in pc_video),
    ('pause OP fourcc PLNK','0x504c4e4bu' not in pc_video),
    ('pause PSVO remains a narrow high-resolution snapshot bridge',
     pause_psvo_bridge_is_narrow(pc_video)),
    ('pause OP fourcc PRVO','0x5052564fu' not in pc_video),
    ('pause OP fourcc PMEN','0x504d454eu' not in pc_video),
    ('pause OP fourcc PCUR','0x50435552u' not in pc_video),
    ('pause OP fourcc PSLD','0x50534c44u' not in pc_video),
    ('pause OP fourcc PPFX','0x50504658u' not in pc_video),
    ('pause J4 rasterizer','draw_pause_j4' not in pc_video),
    ('pause overlay compositor','overlay_retail_pause' not in pc_video),
    ('host-local CRY bridge API','avp_pc_cry16_to_rgb' not in pc_tree and 'avp_pc_rgb_to_cry_qconv' not in pc_tree),
    ('host-local RMW primitive','jaguar_rmw_blend_cry' not in pc_video),
]
failed=False
for name,ok in checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok
# A6: AVP/Jaguar presentation state must not be flattened back into the X11 host.
gpu_state = root / 'include/jaguar/avp_gpu/state.h'
state_text = gpu_state.read_text() if gpu_state.is_file() else ''
a6_checks = [
    ('AVP/Jaguar renderer state module exists', gpu_state.is_file() and (root/'src/jaguar/avp_gpu/state.c').is_file()),
    ('X11 host embeds one Jaguar-owned renderer state object', 'AvpAvpGpuState gpu;' in pc_video),
    ('panel texture state moved out of X11 struct', 'AvpGpuRgbImage panel_tex[74]' not in pc_video and 'AvpGpuRgbImage panel_tex[74]' in state_text),
    ('pause OP state moved out of X11 struct', 'AvpJaguarPauseOpState pause_op' not in pc_video and 'AvpJaguarPauseOpState pause_op' in state_text),
    ('presentation history moved out of X11 struct', 's32 amp_prev_x[AVP_NUM_AMPS]' not in pc_video and 's32 amp_prev_x[AVP_NUM_AMPS]' in state_text),
]
for name,ok in a6_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok
# A7: normal gameplay rasterization belongs to AVP/Jaguar presentation, not X11.
software_renderer = root / 'src/jaguar/avp_gpu/software_renderer.c'
renderer_text = software_renderer.read_text() if software_renderer.is_file() else ''
a7_checks = [
    ('AVP/Jaguar software renderer exists', software_renderer.is_file()),
    ('wall-family visibility logic moved out of X11', 'wall_visible_code' not in pc_video and 'wall_visible_code' in renderer_text),
    ('maze raycaster moved out of X11', 'cast_ray_hit' not in pc_video and 'cast_ray_hit' in renderer_text),
    ('AAF sprite rasterizer moved out of X11', 'draw_amp_sprite' not in pc_video and 'draw_amp_sprite' in renderer_text),
    ('HUD compositor moved out of X11', 'overlay_hud' not in pc_video and 'overlay_hud' in renderer_text),
    ('first-person weapon rasterizer moved out of X11', 'overlay_first_person' not in pc_video and 'overlay_first_person' in renderer_text),
    ('X11 gameplay entry is a renderer/present wrapper', ('avp_gpu_render_maze(&v->gpu,&v->fb' in pc_video or 'avp_gpu_render_maze(&v->gpu,target' in pc_video)),
]
for name,ok in a7_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok
# A8: MJP/frontend raster composition belongs to AVP/Jaguar presentation.
frontend_renderer = root / 'src/jaguar/avp_gpu/frontend_renderer.c'
frontend_text = frontend_renderer.read_text() if frontend_renderer.is_file() else ''
a8_checks = [
    ('AVP/Jaguar frontend renderer exists', frontend_renderer.is_file()),
    ('frontend image rasterizer moved out of X11', 'frontend_blit_image' not in pc_video and 'frontend_blit_image' in frontend_text),
    ('ROM font rasterizer moved out of X11', 'frontend_font_glyph' not in pc_video and 'frontend_font_glyph' in frontend_text),
    ('title/save composition moved out of X11', 'title_font_glyph' not in pc_video and 'authored_fame_text' in frontend_text and 'SELECT CHARACTER' in frontend_text),
    # R282: the wrapper may target the 320x254 frontend aperture (240 displayed lines).
    ('X11 frontend entry is a renderer/present wrapper', 'avp_gpu_frontend_present(&v->gpu,&v->fb' in pc_video or 'avp_gpu_frontend_present(&v->gpu,&v->frontend_fb' in pc_video),
]
for name,ok in a8_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok
# A10: retail cartridge resource parsing is AVP/Jaguar-owned, not PC-owned.
retail_resources = root / 'src/jaguar/avp_resources/retail_resources.c'
pc_resources_file = root / 'src/platform/pc/pc_resources.c'
retail_text = retail_resources.read_text() if retail_resources.is_file() else ''
pc_res_text = pc_resources_file.read_text() if pc_resources_file.is_file() else ''
a10_checks = [
    ('neutral AVP retail resource module exists', retail_resources.is_file() and (root/'include/avp_resources.h').is_file()),
    ('FILES archive parser moved out of PC', 'int avp_resources_find_files_archive(' in retail_text and 'int avp_resources_find_files_archive(' not in pc_res_text),
    ('AAF decoder moved out of PC', 'avp_resources_decode_aaf' in retail_text and 'decode_aaf' not in pc_res_text),
    ('maze discovery moved out of PC', 'avp_resources_find_mazes' in retail_text and 'find_mazes' not in pc_res_text),
    ('authored AMP/level binding moved out of PC', 'avp_resources_bind_retail_amp_placements' in retail_text and 'avp_resources_bind_retail_level_info' in retail_text),
    ('PC resource file is cache-only', 'avp_pc_prepare_rom_cache' in pc_res_text),
]
for name,ok in a10_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok

# A11: retail presentation asset binding belongs to AVP/Jaguar presentation, not X11.
assets_file = root / 'src/jaguar/avp_gpu/assets.c'
assets_text = assets_file.read_text() if assets_file.is_file() else ''
a11_checks = [
    ('AVP/Jaguar presentation asset binder exists', assets_file.is_file() and (root/'include/jaguar/avp_gpu/assets.h').is_file()),
    ('pause retail asset pointers moved out of X11', 'load_pause_assets' not in pc_video and 'load_pause_assets' in assets_text),
    ('retail panel/HUD/AAF binding moved out of X11', 'avp_resources_decode_aaf' not in pc_video and 'avp_resources_decode_aaf' in assets_text),
    ('retail first-person asset binding moved out of X11', 'fp_pal_valid' not in pc_video and 'fp_pal_valid' in assets_text),
    ('frontend resource decode moved out of X11', 'avp_resources_file_payload' not in pc_video and 'avp_gpu_assets_frontend_load' in assets_text),
    ('X11 retail texture loader is a thin delegate', 'avp_gpu_assets_load_retail(&v->gpu,rom)' in pc_video),
    ('X11 frontend resource loader is a thin delegate', 'avp_gpu_assets_frontend_load(&v->gpu,rom,mode,file_index,role)' in pc_video),
]
for name,ok in a11_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok


# A12: X11 has one host-only path for scaling/copying the finished Jaguar framebuffer.
a12_checks = [
    ('X11 framebuffer presentation is centralized', 'static void present_framebuffer(' in pc_video),
    ('gameplay uses centralized host presenter', 'present_framebuffer(v,v->cfg.debug_immediate_in_game,1);' in pc_video),
    ('pause uses centralized host presenter', 'present_framebuffer(v,0,0);' in pc_video),
    ('frontend uses centralized host presenter', 'present_framebuffer(v,0,1);' in pc_video),
]
for name,ok in a12_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok


# A13: FullSynth/AVPSOUND semantics are Jaguar-hosted; PC audio owns only PCM device I/O.
pc_audio_file = root / 'src/platform/pc/pc_audio.c'
fullsynth_file = root / 'src/jaguar/host/fullsynth_host.c'
pc_audio_text = pc_audio_file.read_text() if pc_audio_file.is_file() else ''
fullsynth_text = fullsynth_file.read_text() if fullsynth_file.is_file() else ''
a13_checks = [
    ('hosted FullSynth semantic module exists', fullsynth_file.is_file() and (root/'include/jaguar/host/fullsynth_host.h').is_file()),
    ('FullSynth descriptor parsing moved out of PC audio', all(x not in pc_audio_text for x in ('static int descriptor(', 'static int load_sfx_descriptor(')) and 'static int load_sfx_descriptor(' in fullsynth_text),
    ('logical Jaguar voice ownership moved out of PC audio', 'logical_handle' not in pc_audio_text and 'logical_handle' in fullsynth_text),
    ('ambient retail descriptor mapping moved out of PC audio', '0x130u+15u*4u' not in pc_audio_text and '0x130u+15u*4u' in fullsynth_text),
    ('PC audio retains ALSA device boundary', 'libasound.so.2' in pc_audio_text),
    ('PC mixer thread consumes Jaguar FullSynth PCM', 'avp_jaguar_fullsynth_mix(buf,512u)' in pc_audio_text),
]
for name,ok in a13_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok


# A14: RDB.DTA cartridge-format knowledge is centralized in neutral AVP resources.
runtime_file = root / 'src/platform/pc/pc_runtime.c'
runtime_text = runtime_file.read_text() if runtime_file.is_file() else ''
a14_checks = [
    ('neutral RDB.DTA decoder exists', 'int avp_resources_decode_rdb(' in retail_text and 'avp_resources_rdb_address' in retail_text),
    ('PC runtime no longer owns RDB gzip offset', '0xe840u' not in runtime_text and 'z_stream' not in runtime_text),
    ('hosted FullSynth no longer owns RDB gzip offset', '0xe840u' not in fullsynth_text and 'z_stream' not in fullsynth_text),
    ('frontend text resolves through neutral RDB mapper', 'avp_resources_rdb_address(pc_rdb_data,pc_rdb_size,address)' in runtime_text),
    ('FullSynth loads RDB through neutral resource decoder', 'avp_resources_decode_rdb(F.rom,&F.rdb,&F.rdbn)' in fullsynth_text),
]
for name,ok in a14_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok


# A15: MJP transition-completion semantics belong to AVP/Jaguar frontend state.
a15_checks = [
    ('PC runtime delegates MJP transition completion', 'return avp_pc_video_frontend_transition_done((AvpPcVideo*)u,phase);' in runtime_text),
    ('PC runtime no longer hardcodes transition complete', 'pc_transition_done(void *u,unsigned phase){(void)u;(void)phase;return 1;}' not in runtime_text),
    ('AVP/Jaguar frontend owns transition completion API', 'int avp_gpu_frontend_transition_done(' in frontend_text),
    ('X11 transition wrapper delegates to AVP/Jaguar frontend', 'avp_gpu_frontend_transition_done(&v->gpu,phase)' in pc_video),
]
for name,ok in a15_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok


# F7: foreground MAZE2 clip/depth semantics stay in Jaguar/AVP presentation.
occlusion_file = root / 'src/jaguar/avp_gpu/maze2_occlusion.c'
occlusion_header = root / 'include/jaguar/avp_gpu/maze2_occlusion.h'
occlusion_text = occlusion_file.read_text() if occlusion_file.is_file() else ''
f7_checks = [
    ('MAZE2 per-pixel occlusion module exists', occlusion_file.is_file() and occlusion_header.is_file()),
    ('Jaguar renderer state owns MAZE2 occlusion mask', 'AvpMaze2Occlusion maze_occlusion;' in state_text),
    ('world sprite rasterizer tests per-pixel occlusion', 'avp_gpu_maze2_occlusion_blocks(occlusion,sx,yy,(float)forward)' in renderer_text),
    ('wall spans preserve nearest per-pixel occlusion depth', ('avp_gpu_maze2_occlusion_mark_span(occlusion,sx,y0,y1,depth)' in renderer_text) or ('if(*owner<=depth)continue;' in renderer_text and '*owner=depth;' in renderer_text and 'for(k=0;k<pc;k++)draw_hit_column' in renderer_text)),
    ('PC/X11 does not own MAZE2 occlusion semantics', 'maze2_occlusion' not in pc_video and 'occlusion_blocks' not in pc_video),
    ('first-person overlay remains outside world occlusion', 'overlay_first_person(state,fb,vblank_counter,smooth_presentation);' in renderer_text),
]
# Keep the first-person overlay itself free of world-occlusion checks.
fp_start=renderer_text.find('static void overlay_first_person(')
fp_end=renderer_text.find('void avp_gpu_hud_message_clear',fp_start)
fp_body=renderer_text[fp_start:fp_end] if fp_start>=0 and fp_end>fp_start else ''
f7_checks.append(('first-person weapon overlay is not clipped by world mask', 'occlusion' not in fp_body))
f7_checks.append(('source-derived first-person overlay descriptors exist', (root/'src/jaguar/avp_gpu/fp_overlay.c').is_file()))
f7_checks.append(('first-person overlay no longer uses host image-centering inference', '160-inf.w/2' not in renderer_text and '200-inf.h' not in renderer_text))
f7_checks.append(('retail OT_USE flash descriptors are source-derived', 'avp_gpu_fp_use_overlay_desc' in renderer_text and 'USE_FLASH2A' in (root/'src/game/mazescrn.c').read_text()))
f7_checks.append(('shotgun/pulse/smart frame0 bind retail use objects', 'AVP_WC_SET_USE1,USE_FLASH1' in (root/'src/game/mazescrn.c').read_text() and 'AVP_WC_SET_USE2,USE_FLASH2B' in (root/'src/game/mazescrn.c').read_text() and 'AVP_WC_SET_USE2,USE_FLASH3B' in (root/'src/game/mazescrn.c').read_text()))
f7_checks.append(('4-bpp use overlays decode high nibble first with transparent zero', 'draw_indexed4_native' in renderer_text and '(b>>4)' in renderer_text and 'if(!pi||pi>=colors' in renderer_text))
f7_checks.append(('vf_pulse parity is driven by host 60 Hz VBL time, not 10 Hz simulation', 'vblank_epoch_ns' in pc_video and '*60ull/1000000000ull' in pc_video))
f7_checks.append(('first-person renderer honors MAZESCRN swing_on',
                  'swing_enabled=swing_on?1u:0u' in (root/'src/game/mazescrn.c').read_text() and
                  'st->swing_enabled?st->swing_x:0' in renderer_text and
                  'st.swing_enabled?st.swing_x:0' in renderer_text))
f7_checks.append(('first-person palette depths match retail OT_OVER/OT_USE CLUT sizes',
                  'state->fp_pal_valid[slot]!=208u' in renderer_text and
                  'state->fp_pal_valid[slot]!=16u' in renderer_text))
for name,ok in f7_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok

# F2: retail MJP GPU fade/cross arithmetic stays in AVP/Jaguar presentation.
frontend_fade = root / 'src/jaguar/avp_gpu/frontend_fade.c'
frontend_fade_text = frontend_fade.read_text() if frontend_fade.is_file() else ''
f2_checks = [
    ('MJP GPU fade/cross primitive module exists', frontend_fade.is_file() and (root/'include/jaguar/avp_gpu/frontend_fade.h').is_file()),
    ('retail RGB16 fraction masks are Jaguar-owned', all(m in frontend_fade_text for m in ('0x7bdfu','0x39cfu','0x18c7u','0x0843u'))),
    ('selection CROSS is driven by frontend event state', 'event==0x53474346u' in frontend_text and 'frontend_blit_select_zoom_cross' in frontend_text and 'avp_gpu_rgb16_cross' in frontend_text),
    ('selection FADEIN is driven by frontend event state', 'event==0x53474649u' in frontend_text and 'frontend_blit_fade' in frontend_text),
    ('PC/X11 owns no MJP fade arithmetic', all(x not in pc_tree for x in ('0x7bdf7bdf','0x39cf39cf','0x18c718c7','0x08430843','avp_gpu_rgb16_fraction','avp_gpu_rgb16_cross'))),
]
for name,ok in f2_checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
    failed|=not ok

if failed:
    sys.exit(1)
print('PASS: A1 Jaguar compatibility ownership seams remain intact')
