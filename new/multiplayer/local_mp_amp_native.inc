/* Optional tail of original AMP.S C translation. No original AI code is
 * altered. Scoped only between complete UpdateAMPs ticks. */
#include "../../new/multiplayer/local_mp_amp_native.h"
#define AVP_MP_AMP_MAGIC 0x414D5057u

/* Host-only callback seam: the PC optional enhancement installs its
 * own read-only validator.  No symbol from the PC module is required by
 * the original AMP compilation or standalone Jaguar-side tests. */
static int (*avp_mp_amp_free_list_validator)(const u16 *,unsigned);
void avp_mp_amp_native_set_free_list_validator(int (*fn)(const u16 *,unsigned))
{
    avp_mp_amp_free_list_validator=fn;
}

static int avp_mp_amp_state_valid(const AvpMpAmpNativeState *s)
{
    if (!s || s->valid!=AVP_MP_AMP_MAGIC ||
        s->free_count>AVP_NUM_AMPS ||
        s->cur_amp_no>AVP_NUM_AMPS) return 0;
    /* Optional PC-only validator: exact original duplicate/range decision,
     * linear rather than quadratic for the 300-entry free list. Keep the
     * original loop when Game Micro-Optimization is Off or unsupported. */
    if(avp_mp_amp_free_list_validator)
        return avp_mp_amp_free_list_validator(s->free_list,s->free_count);
    /* Original allocator validity rules, retained as the default path. */
    for (unsigned i=0;i<s->free_count;i++) {
        if (s->free_list[i]>=AVP_NUM_AMPS) return 0;
        for (unsigned j=0;j<i;j++)
            if (s->free_list[i]==s->free_list[j]) return 0;
    }
    return 1;
}

/* Opt-in fast capture is implemented separately under /new. */
static int avp_mp_amp_direct_capture_enabled;
void avp_mp_amp_native_set_direct_capture(int enabled)
{
    avp_mp_amp_direct_capture_enabled=enabled!=0;
}
#include "../../new/game_micro_optimization/amp_capture_direct.inc"

int avp_mp_amp_native_capture(AvpMpAmpNativeState *out)
{
    AvpMpAmpNativeState s;
    if (avp_mp_amp_direct_capture_enabled)
        return avp_game_micro_amp_capture_direct(out);
    if (!out || amp_update_active || amp_data!=level1amps ||
        amps_at!=level1amps) return 0;
    memset(&s,0,sizeof(s));
    s.valid=AVP_MP_AMP_MAGIC;
    memcpy(s.actors,level1amps,sizeof(s.actors));
    memcpy(s.collmap,collmap,sizeof(s.collmap));
    memcpy(s.objmap,objmap,sizeof(s.objmap));
    s.levels_visit=levels_visit;s.discflag=discflag;
    memcpy(s.inventory_table,inventory_table,sizeof(s.inventory_table));
    s.ampcount=ampcount;s.ngens=ngens;s.reset_counter=avp_reset_counter;
    memcpy(s.free_list,free_list,sizeof(s.free_list));
    s.free_count=free_count;
    memcpy(s.slot_generation,slot_generation,sizeof(s.slot_generation));
    s.next_slot_generation=next_slot_generation;
    memcpy(s.saved_levels,saved_levels,sizeof(s.saved_levels));
    s.marine_pulse_id=marine_pulse_id;s.marine_flame_id=marine_flame_id;
    s.marine_pulse_time=marine_pulse_time;s.marine_flame_time=marine_flame_time;
    s.beam_counter=beam_counter;s.beam_weapon_reads=beam_weapon_reads;
    s.cur_amp_no=cur_amp_no;s.entime=entime;s.explo1=explo1;
    s.queen_respawns=queen_respawns;s.queen_end_timer=queen_end_timer;
    if (!avp_mp_amp_state_valid(&s)) return 0;
    *out=s;
    return 1;
}

int avp_mp_amp_native_restore(const AvpMpAmpNativeState *s)
{
    if (amp_update_active || !avp_mp_amp_state_valid(s) ||
        amp_data!=level1amps || amps_at!=level1amps) return 0;
    memcpy(level1amps,s->actors,sizeof(s->actors));
    memcpy(collmap,s->collmap,sizeof(s->collmap));
    memcpy(objmap,s->objmap,sizeof(s->objmap));
    levels_visit=s->levels_visit;discflag=s->discflag;
    memcpy(inventory_table,s->inventory_table,sizeof(s->inventory_table));
    ampcount=s->ampcount;ngens=s->ngens;avp_reset_counter=s->reset_counter;
    memcpy(free_list,s->free_list,sizeof(s->free_list));
    free_count=s->free_count;
    memcpy(slot_generation,s->slot_generation,sizeof(s->slot_generation));
    next_slot_generation=s->next_slot_generation;
    memcpy(saved_levels,s->saved_levels,sizeof(s->saved_levels));
    marine_pulse_id=s->marine_pulse_id;marine_flame_id=s->marine_flame_id;
    marine_pulse_time=s->marine_pulse_time;marine_flame_time=s->marine_flame_time;
    beam_counter=s->beam_counter;beam_weapon_reads=s->beam_weapon_reads;
    cur_amp_no=s->cur_amp_no;entime=s->entime;explo1=s->explo1;
    queen_respawns=s->queen_respawns;queen_end_timer=s->queen_end_timer;
    return 1;
}
