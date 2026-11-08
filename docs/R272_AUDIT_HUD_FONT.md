# Audit: MAZE/HUD.S, MAZE/HUD_MSG.S, AMP/FONT.S vs hud.c / hud_message.c / hud_score.c / font.c

Scope: CPU-visible HUD state (energy bar, scores, tracker click/sound parameters, countdown,
message state machine, cocoons, map flags, HUD brightness, font metrics/print state).
Pixel drawing excluded. weapons.c (PredAvail) comes from MAZESCRN.S, not HUD.S, so it is only
mentioned in the observations at the end. collectables.c is only in scope where it feeds the
HUD message queue.

---

## F1: Pickup and Predator-award messages are suppressed while GAME OVER or the countdown is showing. The original overwrites `new_message` with no check.
- Severity: cosmetic-state (HUD message state: `new_message`, `msg_ptr`, `msg_status`, `msg_info`/msgN_on), ordering
- Confidence: PROVEN

**Original.** Every producer writes `new_message` directly, with no priority test:
- PLAYER.S:831 `give_energy`: `move.l a0,new_message ; clears zero flag - collected`
- PLAYER.S:855 / :884 (`give_wep` / `give_ammo`): `move.l a0,new_message`
- PLAYER.S:679 `move.l #clm_card,new_message`, PLAYER.S:774 `move.l #clm_mtrack,new_message`
- MAZESCRN.S:1450 / :1476 (PredAvail): `move.l 12(a0),new_message` / `move.l 8(a0),new_message`

The only gate anywhere is the countdown re-queue in HUD.S:403-407:
```
		tst.b	msg_status
		beq	.do_msg
		cmp.l	#cdm_stop,msg_ptr
		bne	.no_msg
.do_msg:	move.l	#msg_countdown,new_message
```
So a pickup during the self-destruct countdown replaces the countdown text for the whole pickup
sequence: 1 + 4 + 5 + 5 + 5 + 5 = 25 HUD updates. The countdown is re-queued only after
`msg_status` reaches 0, because during the pickup `msg_ptr != cdm_stop`. A pickup queued in the
same frame as `msg_dead` also wins if it is written last.

**Port.** `hud_message.c:308`:
```c
    /* Critical sequences remain non-interruptible by collection notices. */
    if(avp_hudmsg_critical_active())return;
```
and `platform/pc/pc_runtime.c:68` (Predator honour notices):
```c
    if(steps&&!avp_hudmsg_critical_active())avp_hudmsg_queue(steps);
```
`avp_hudmsg_critical_active()` (`hud_message.c:50-55`) is true whenever `new_message` or the
running sequence is `avp_msg_dead` or `avp_msg_countdown`. During the destruct countdown the
countdown sequence is re-queued every second, so it is effectively always active. The test
`tests/test_hud_message.c:86-102` checks the helper but not any original behaviour.

**Where they differ.** A Marine picks up a MEDKIT while counttime = 90 and the countdown message
is in its `$0000FF00|$7f` phase.
- Original: `new_message = clm_medaid`. The next UpdtHUDMsg shows "MEDKIT / COLLECTED" with
  `msg_info = $FFFF00FF`. The following Countdown ticks find `msg_status != 0` and
  `msg_ptr != cdm_stop`, so they do not re-queue. The countdown text is absent for 25 updates,
  `cdm_time` is not rewritten on those ticks, and `msg_info`/`msg1_on..3_on` follow the pickup
  colours.
- Port: the pickup notice is dropped. The countdown text and colours continue unchanged.

The same applies to Predator COMBI/DISC/CANNON gain/lose notices while the countdown is running.

**Fix.** Delete `hud_message.c:307-308` (the comment and the `if(avp_hudmsg_critical_active())return;`).
In `pc_runtime.c:68`, replace
`if(steps&&!avp_hudmsg_critical_active())avp_hudmsg_queue(steps);` with
`if(steps)avp_hudmsg_queue(steps);`.
Knock-on: `avp_hudmsg_critical_active()` then has no callers apart from the test.
`hud.c::Countdown` still keeps the countdown from overwriting a pickup, through the fixed
condition in F3.

---

## F2: `InitCocoons` clears `time`, `level`, `x` and `y` of every slot. The original clears only `cocoon_frame`, so stale fields of EMPTY slots reach the EEPROM save.
- Severity: cosmetic-state (EEPROM save contents / checksum input; no gameplay read of EMPTY slots)
- Confidence: PROVEN

**Original.** HUD.S:856-863:
```
		clr.w	num_cocoons
		move.w	#MAX_COCOONS-1,d0
		lea	cocoon_data,a1
.init_ccn:	move.w	#COCOON_EMPTY,cocoon_frame(a1)
		add.w	#cocoon_size,a1
		dbra	d0,.init_ccn
```
Only `cocoon_frame` is written. `cocoon_time`, `cocoon_level`, `cocoon_x` and `cocoon_y` keep
whatever the previous game left. `encode_cocoon` (MAZESCRN.S:986-1005) packs x/y/level for every
slot, including EMPTY ones:
```
		move.l	cocoon_x(a1),d0
		and.l	#$3f0000,d0
		...
		move.w	cocoon_level(a1),d1
```

**Port.** `hud.c:113-118`:
```c
    for(unsigned i=0;i<AVP_MAX_COCOONS;i++){
        cocoon_data[i].frame=COCOON_EMPTY;
        cocoon_data[i].time=0;
        cocoon_data[i].level=0;
        cocoon_data[i].x=cocoon_data[i].y=0;
    }
```
`savegame.c:27-38 encode_cocoon` then encodes those zeros.

**Where they differ.** Same session, no reset:
1. Alien game 1 makes 3 cocoons, so slot 0 has x=$12xxxx, y=$0Axxxx, level=2.
2. Game over, then alien game 2 starts. InitHUD runs InitCocoons and one new cocoon is made in
   slot 2.
3. The player saves.

Save long 2 then holds x1=$12, y1=$0A, l1=2, f1=30 (EMPTY) in the original, and x1=0, y1=0, l1=0,
f1=30 in the port. Slot 1 differs the same way. The EEPROM bytes and checksum differ. On load
both sides ignore the EMPTY slots, so gameplay is the same.

**Fix.** In `hud.c` InitCocoons, replace the loop body with only
`cocoon_data[i].frame=COCOON_EMPTY;`. Remove the `.time`, `.level`, `.x` and `.y` assignments.
Keep the static zero-initialisation, which matches the cleared Jaguar BSS at power-on.

---

## F3: The countdown re-queue condition is broader than `msg_ptr == cdm_stop`.
- Severity: ordering (HUD message state)
- Confidence: LIKELY. Unreachable at the default 10 Hz simulation. Reachable when the host
  simulation rate is 5 Hz or lower, or whenever two countdown ticks fall within 5 HUD updates.

**Original.** HUD.S:403-406, quoted in F1. The re-queue is allowed only when `msg_status == 0`,
or when `msg_ptr == cdm_stop`, which is true only while the `$0000FF00|$7f` record is counting
down. It is false on the MSG_NEW frame and during the `$FFFFFF00|4` hold (5 updates).

**Port.** `hud.c:298` `if (msg_status==AVP_HUDMSG_STOP || avp_hudmsg_countdown_active())`, with
`hud_message.c:60-64`:
```c
    return new_message==avp_msg_countdown ||
           (msg_status!=AVP_HUDMSG_STOP && msg_source==avp_msg_countdown);
```
This is also true during the NEW frame and the 4-tick hold. The test at
`tests/test_hud_message.c:91-99` asserts this broader behaviour.

**Where they differ.** A tick lands in update 1-5 after the previous re-queue, for example with
`one_sec = 5` HUD updates.
- Original: no new `new_message`, and `cdm_time` is not rewritten. The text stays at the old
  value until the next tick that lands in the `$7f` phase.
- Port: re-queues and rewrites the text, which restarts the NEW/hold sequence.

**Fix.** In `hud_message.c`, make the predicate the exact original test:
```c
int avp_hudmsg_countdown_active(void)
{
    /* HUD.S: cmp.l #cdm_stop,msg_ptr -- true only while the $7f record runs */
    return msg_ptr==avp_msg_countdown+2;
}
```
Keep `hud.c:298` as `msg_status==AVP_HUDMSG_STOP || avp_hudmsg_countdown_active()`.

This is invariant across the catch-up loop iterations, as in the asm. In the port, `msg_ptr`
points at index 2 exactly while the `$7f` info is being counted. Once that record expires
`msg_ptr` moves to index 3 and `msg_status` is 0, which matches `cdm_stop+4` / status 0.
Update the test.

---

## F4: Weapon-then-ammo collection in the same frame. The port ignores any later ammo notice, but the original's last writer wins.
- Severity: cosmetic-state (which HUD message is shown)
- Confidence: LIKELY. Needs two collectables processed in one frame; reachability not proven.

**Original.** PLAYER.S:854-855 `give_wep`: `move.l a1,a0 ; use full message` /
`move.l a0,new_message`, then it drops through into `give_ammo`. That rewrites the same weapon
message (PLAYER.S:884). Any later, separate CollectIt in the same frame (for example
`COLL_SHOTAMMO`) writes its own message last.

**Port.** `hud_message.c:306`:
```c
    if(ammo && pickup_weapon_pending && new_message==pickup_msg)return;
```
This suppresses every ammo notice of any weapon type until UpdtHUDMsg consumes the queue, not
only the fall-through from `give_wep`.

**Where they differ.** A new PULSE RIFLE and then SHOTGUN ROUNDS are collected in the same frame.
- Original: shows "SHOTGUN ROUNDS COLLECTED" (`clm_sammo`).
- Port: shows "PULSE RIFLE COLLECTED".

**Fix.** Make the suppression apply only to the fall-through call. Delete line 306 and the
`pickup_weapon_pending` state. In `collectables.c:220 avp_give_weapon`, pass the weapon notice as
the fall-through's notice when the weapon was newly given:
```c
    (void)avp_give_ammo(weapon,initial_ammo,
        newly_given ? weapon_notice : ammo_notice, ammo_sfx);
```
Here `newly_given` is set inside the `if (!(cur_weps&...))` block. This mirrors `move.l a1,a0`
before the drop-through. The weapon message is then written twice, as in the original.

---

## Routines verified as matching (CPU state / ordering)
HUD.S
- InitHUD: call order, `show_coords`/`old_meter`/`show_mt`/`invisflag`/`counting` cleared; START_MT=0 in ROM.
- ResetHUD: `click_track=0`, `scope_frame=SCOPE_ANIM(7)`.
- HUD_human: scope_frame increment/wrap (BLS on 0..7 values), GPUBUF6 clamp to SCOPE_LAST, show_mt gate, HUMScores, destruct/counting/InitCountdown/Countdown order.
- TC: MT_MAX−nearest target, BEQ/BGT/BGE tracking with CTRACK, volume/pitch/env/mod-depth formulas (`lsl.l #8;clr.w;swap`, `lsr.l #2+$a7000`, `swap+$2f`, `((MT_MAX−d)<<4)>>16+$51`), trac1 only if nonzero, trac2 always. The signed compares are equivalent because MAZE4.GAS initialises `nearest` to SCAN_DIST = MT_MAX and only lowers it, so the target is ≥ 0.
- InitCountdown: counttime 121, ticktime = frames + 2·one_sec, xtra_c $F180, flash_dir 20, alarm start.
- Countdown: counttime==0 early exit, flash oscillator (BHI $A0 / BHI $60 → reverse), catch-up loop, single sfx after loop with last cue winning, cue thresholds, game_over/xtra_c $8880/KillAlarm at 0, cdm_time digit offsets (+0/+2/+3 = text[5]/[7]/[8]). The condition is covered by F3. The frames/one_sec seam (VBL counter versus simulation ticks at simulation_hz) keeps real-time semantics, and the original freezes `frames` during pause via `save_frames`.
- HUD_alien, HUD_pred/HP2: target = high word of (sum<<6); smooth = old + ((target−old) ASR 1); both clamped to ±21; old_meter = clamped target; PREDScore last.
- UpdtHUD: score<0 clamp, per-species dispatch, ShowHUDMsg, ShowMap, ShowPos order.
- InitNrg: table values per species, rescale = (barwidth<<12)/max_energy − 1 (word), size, frame=0, ftime=4, flash=0.
- UpdtNrg: frame advance (pre-increment frame used for source), flash timer (2+flash, EOR 2), zero/neg energy, 3/8 threshold (LSR), signed BGT, clamp, `mulu;lsl.l #4;swap;+1`.
- extract_cocoon, InitCocoons save decode (`and $7ff; ror 11; |long2>>11`; `long2<<10`), MakeCocoon (descending slot), CheckCocoon, UseCocoon (x/y low word $8000, new_level, ccn_save, energy refill, 2-record shift, slot0 EMPTY, num−1, redraw, sfx usecn), UpdateCocoons (force-draw on time<0, READY skip, decrement/advance). F2 is the exception.
- InitScores, ALScore, PREDScore, HUMScores (shared old_score, show_coords energy zero-extended, acs byte compare).
- HUDBright (BVC saturation to $7F, signed BGE floor at $A0, $A0 → ZeroHUDBright), InitHUDPal (bright 64), ZeroHUDBright leaves hud_bright.
- InitMap (map_shapes values, old_map/map_on cleared), ShowMap GPU path (falling-edge = old & ~map_on, old_map update).

HUD_MSG.S
- InitHUDMsg (new_message=0, msg_info=0 so msg_status=0), UpdtHUDMsg (new-message priority, status countdown, record advance), ShowHUDMsg (only on MSG_NEW, sets status 1).
- All message streams: colours, MARK−1/SPACE/MARK timings, STOP terminators, line assignments (dead, pressure, notsecure, airlocked, jlift, jammed, acdenied, all clm_* including clm_food's mixed $FFFF0000/$FFFFFF00, cheat on/off, stun/cocooned/ccnfail, usecocoon 9/10, newduct/newlevel/escape, PRW 6/7, countdown 4/$7f). Card digits "0N"/"10". nlev_char / nduct_chars offsets.

FONT.S
- init_font: column scan (byte = col>>2, mask = col&3, 9 rows at stride 64), width includes terminator column, blankflag reset only on non-blank column, two blanks end brush with the uncommitted x-offset discarded, entry layout {x, width, brush}, repeat_pad cleared.
- pr_string: repeat_pad guard/set, s_delay/typeflag/cursor counters reset, x/y header, NUL clears repeat_pad, A/B/C edge exit without clearing repeat_pad, $FE/$FD/$FF handling (odd-address skip; host RDB buffer keeps ROM parity because base $20300 is even and the buffer is allocated aligned), sfx typ1b on every second character (EOR then BNE), space +6, wrap at x+w > 319 → y+15/x=0, x += width, teletype loop 3×(VSync+cursor) then hide_cursor.
- cursor/clrcursor/curprint/hide_cursor: 10-count on/off toggling, curprint wrap side-effect on x/y without advancing x.
- box_draw: line sequence and coordinate updates (top w, bottom y+h with w+1, left at saved y, right x+w).

## Not checked / out of scope
- Blitter/GPU drawing (energy bar blits, cocoon blits, meter spans, message glyph blits, print_18/print_88, automap GPU overlay, `motion_tracker` GPU distance metric in MAZE4.GAS). The CPU only consumes GPUBUF1 (nearest), which is GPU code.
  - Presentation only: `hud.c avp_hud_pred_meter_span` returns `pred_meter_previous` as the start of both rows. HP2's second span actually starts at the smoothed value (`move.w d5,d4` after `d5 = smooth`), not at old_meter. This does not affect state.
- xDecPrint/DecPrint/HexPrint/ShowPos: pure bitmap output (debug coordinates).
- InitHUD's QuickMove of hpos/dpos brushes: resource copy.

## Observation outside this assignment (for the MAZESCRN.S auditor)
`weapons.c::PredAvail` (from MAZESCRN.S:1438-1487) differs from the asm:
- The port adds a "fallback" block (`weapons.c` `if (changed) { ... new_wepno = fallback/4 }`) that has no counterpart in the asm. In the original, after `.re_down`, `new_wepno` = the previous record's weapon only when `cur_wepno` equals the removed weapon. For example, holding the cannon when score drops from ≥750000 to <150000 gives `new_wepno = 3` (disc, itself revoked) in the original and 4 in the port.
- The threshold compares are unsigned in the asm (`bhs`/`blo`, terminator tested with `bmi`) but signed in the port (`sc<awards[].score`). They differ if `score` is negative when PredAvail runs. UpdtScrOverlays runs before UpdtHUD's `score<0` clamp, and a negative score compares as huge unsigned, so the original awards everything while the port revokes.
