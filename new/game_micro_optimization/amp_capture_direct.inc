/* RE31 R14 opt-in host AMP snapshot capture: included only into amp.c via
 * new/multiplayer/local_mp_amp_native.inc, so all original static world
 * counters and arrays remain authoritative. No original-game source edits.
 *
 * Normal capture first creates a full stack snapshot and then copies it to
 * out after validation. On the game micro-optimization path we validate the
 * live, single-threaded state *before* writing out, then fill out directly.
 * This eliminates one large full-state copy and one large stack scratch.
 * Failure leaves the caller's out buffer untouched, as in the original.
 */
static int avp_game_micro_amp_capture_direct(AvpMpAmpNativeState *out)
{
    if (!out || amp_update_active || amp_data!=level1amps ||
        amps_at!=level1amps || free_count>AVP_NUM_AMPS ||
        cur_amp_no>AVP_NUM_AMPS ||
        !avp_mp_amp_free_list_validator ||
        !avp_mp_amp_free_list_validator(free_list,free_count)) return 0;

    memset(out,0,sizeof(*out));
    out->valid=AVP_MP_AMP_MAGIC;
    memcpy(out->actors,level1amps,sizeof(out->actors));
    memcpy(out->collmap,collmap,sizeof(out->collmap));
    memcpy(out->objmap,objmap,sizeof(out->objmap));
    out->levels_visit=levels_visit;out->discflag=discflag;
    memcpy(out->inventory_table,inventory_table,sizeof(out->inventory_table));
    out->ampcount=ampcount;out->ngens=ngens;out->reset_counter=avp_reset_counter;
    memcpy(out->free_list,free_list,sizeof(out->free_list));
    out->free_count=free_count;
    memcpy(out->slot_generation,slot_generation,sizeof(out->slot_generation));
    out->next_slot_generation=next_slot_generation;
    memcpy(out->saved_levels,saved_levels,sizeof(out->saved_levels));
    out->marine_pulse_id=marine_pulse_id;out->marine_flame_id=marine_flame_id;
    out->marine_pulse_time=marine_pulse_time;out->marine_flame_time=marine_flame_time;
    out->beam_counter=beam_counter;out->beam_weapon_reads=beam_weapon_reads;
    out->cur_amp_no=cur_amp_no;out->entime=entime;out->explo1=explo1;
    out->queen_respawns=queen_respawns;out->queen_end_timer=queen_end_timer;
    return 1;
}
