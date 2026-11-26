# RE38 R154 — Original-only C source crosswalk (NOT 1:1 behavior completion)

Generated mechanically against the separate original Jaguar C reconstruction.
Scope: functions found in the separate Jaguar readable-C reconstruction `src/game` files;
added `new/` code, bots, multiplayer, editor and PC-specific modules are excluded.
The C reconstruction itself includes some host scaffolding, so this is NOT a strict census
of retail-ROM executable routines; a ROM symbol/address manifest is still required.

**CRITICAL:** The exact-token count below is a conservative SOURCE-IDENTITY CHECK ONLY.
Identical source does not prove original ROM behavior because the C reconstruction itself is not
an instruction-by-instruction runtime oracle. Conversely, modified source may be fully equivalent
(e.g. host-only checks or UB-safe fixed-point arithmetic). Do not call this a game completion percentage.

- Original reconstruction routine names detected: **691**
- Byte-token-identical function bodies: **519**
- Named original functions with changed bodies: **170**
- Original names not found by this conservative parser: **2**

| Original file | Original routines | Identical C bodies | Modified C bodies | Missing names |
|---|---:|---:|---:|---:|
| `amp.c` | 171 | 111 | 59 | 1 |
| `collectables.c` | 10 | 4 | 6 | 0 |
| `collision.c` | 28 | 22 | 6 | 0 |
| `computer.c` | 32 | 27 | 5 | 0 |
| `doors.c` | 36 | 26 | 10 | 0 |
| `eeprom.c` | 11 | 8 | 3 | 0 |
| `files.c` | 27 | 25 | 2 | 0 |
| `font.c` | 15 | 13 | 2 | 0 |
| `gpu_overlays.c` | 8 | 8 | 0 | 0 |
| `hud.c` | 45 | 37 | 8 | 0 |
| `hud_message.c` | 9 | 6 | 3 | 0 |
| `hud_score.c` | 5 | 5 | 0 | 0 |
| `joyed.c` | 2 | 1 | 1 | 0 |
| `levels.c` | 21 | 20 | 1 | 0 |
| `main_game.c` | 15 | 12 | 2 | 1 |
| `maze.c` | 15 | 12 | 3 | 0 |
| `mazescrn.c` | 54 | 37 | 17 | 0 |
| `message.c` | 1 | 1 | 0 | 0 |
| `mjp.c` | 91 | 74 | 17 | 0 |
| `music.c` | 33 | 21 | 12 | 0 |
| `objects.c` | 22 | 21 | 1 | 0 |
| `player.c` | 24 | 14 | 10 | 0 |
| `runtime.c` | 3 | 3 | 0 | 0 |
| `savegame.c` | 6 | 5 | 1 | 0 |
| `sprites.c` | 4 | 4 | 0 | 0 |
| `weapons.c` | 3 | 2 | 1 | 0 |

## Named original-reference functions whose bodies differ

These are audit candidates, **not** verified behavioral mismatches. Host-only
adapters and previously verified fidelity corrections often change C bodies.

- `amp.c`: `UpdateAMPs`, `alien_free_queen_mode`, `alien_gameover_mode`, `amp_acid`, `amp_appear`, `amp_apply_fight_frame`, `amp_begin_death`, `amp_chase_target`, `amp_cleargrid_c`, `amp_closehug`, `amp_crouch_fire`, `amp_disc_prepare`, `amp_generate_running`, `amp_grid_nonzero`, `amp_invisible_frames`, `amp_laser`, `amp_launchfire`, `amp_move_toward`, `amp_number`, `amp_ranged_human`, `amp_ranged_predator`, `amp_recoil`, `amp_release`, `amp_req`, `amp_setgrid`, `amp_spawn_npc_projectile`, `amp_start_hug_mode`, `apply_template`, `collmap_amp`, `dead_gen`, `dead_gen2`, `discmove`, `do_score`, `egg_open_mode`, `exp_handle`, `explosion_ex`, `fire_hug_mode`, `flame_on`, `generate`, `gofight`, `initialise`, `lostplayer`, `make_template_at`, `next_creature`, `player_weapon`, `queen_choose_fight`, `queen_dead_begin`, `queen_fight_mode`, `queen_lock_mode`, `queen_play_death_mode`, `rand_set`, `release_amp`, `restore_level`, `save_level`, `shield2`, `stun_death`, `stun_mode`, `weapon_hit`, `within_player`
- `collectables.c`: `Collectables`, `ResetClct`, `avp_give_ammo`, `avp_give_energy`, `avp_give_weapon`, `sfx`
- `collision.c`: `AMPCollisions`, `Angle`, `Vector`, `check_wall`, `door_for_wall`, `rough_dist`
- `computer.c`: `avp_computer_arm_destruct`, `fixed_viewer`, `podcomp`, `print_page`, `scheme`
- `doors.c`: `DoorKeys`, `InitDoors`, `ResetDoors`, `SaveDoors`, `access_ok`, `airlock_exit`, `amp_grid_hit`, `door_collides`, `init_dcoll`, `lift_exit`
- `eeprom.c`: `Check_EE`, `Default_EE`, `Init_EE`
- `files.c`: `avp_files_bind`, `record_at`
- `font.c`: `line_evt`, `pr_string`
- `hud.c`: `Countdown`, `HP2`, `InitCocoons`, `InitHUD`, `InitNrg`, `TC`, `UpdtNrg`, `UseCocoon`
- `hud_message.c`: `InitHUDMsg`, `ShowHUDMsg`, `UpdtHUDMsg`
- `joyed.c`: `MoveData`
- `levels.c`: `KillBastards`
- `main_game.c`: `NewTitle`, `back_in_runtime`
- `maze.c`: `NextFrame`, `PlayAvP`, `Process_EndGame`
- `mazescrn.c`: `DoPause`, `Init1stOvers`, `InitPXFades`, `InitScrOverlays`, `InitWeps`, `PXFades`, `StdHUD`, `execute_cmd`, `fade_selects`, `fill_weps`, `fire_wep`, `hug_init`, `new_weapon`, `pause_sfx`, `process_shot`, `reset_frame0`, `update_wep`
- `mjp.c`: `Do_Escap`, `Do_Intro`, `Do_Lose`, `Do_Selec`, `Do_Title`, `End_Sele`, `MTEST`, `New_Make`, `Show_Tex`, `fame_edit_name`, `load_mode_resources`, `mjp_scene_end`, `mjp_transition`, `select_expand`, `select_pose`, `title_input_loop`, `title_save_slot`
- `music.c`: `DoEffect`, `FXFadeDo`, `FXFadeUp`, `InitSynt`, `PlacedEf`, `TFX`, `avp_pause_audio_restore`, `avp_pause_audio_save`, `begin_voice`, `fade_tick`, `meta_for`, `unloop_voice`
- `objects.c`: `VSync`
- `player.c`: `BGSounds`, `ExtraKeys`, `InitPlayer`, `MovePlayer`, `Pain`, `PlayerWeapons`, `TestSpark`, `UpdatePlayer`, `do_predo`, `pain_sfx`
- `savegame.c`: `SaveGame`
- `weapons.c`: `PredAvail`

## Names requiring manual mapping (no automatic failure assumption)

- `amp.c::clear_runtime_grids`
- `main_game.c::post_game_frontend`

## Original-code 1:1 status (separate from source-body identity)

Historic 68000 readable-source structural closure is reported as complete for its tracked
84,142-byte scope. A complete instruction/state behavior-equivalence numerator is **not available**.
The source-body counts above MUST NOT be promoted to a behavioral or GPU/DSP percentage.

Known original-path differences still requiring hardware/ROM verification: AMP grid `$FFFF`
ownership and stale-owner guards; full first-visit/revisit AMP lifecycle; GPU renderer pixel
ordering; Blitter busy timing; JERRY audio output; and remaining original state transitions.

R154 adds 10,560 deterministic `do_score` cases including C signed-overflow boundary tests.
These prove the specified 68000 arithmetic contract against an independent arithmetic oracle,
not exhaustive proof for the whole original game.
