/* Live pool limit belongs to the current world and is included in PC saves
 * and multiplayer snapshots. The selected modifier itself is a preference. */
static unsigned host_enemy_number_multiplier=1u;
static unsigned host_amp_capacity=AVP_ORIGINAL_NUM_AMPS;
static void modifier_reset_pool(void)
{
    unsigned cap=host_enemy_number_multiplier>1u?AVP_NUM_AMPS:AVP_ORIGINAL_NUM_AMPS;
    if(cap==AVP_ORIGINAL_NUM_AMPS && cur_level>=1 && cur_level<=AVP_MAX_LEVEL &&
       (levels_visit&(u16)(1u<<((unsigned)cur_level-1u)))){
        const AvpAmpSavedPlacement *p=saved_levels[cur_level-1];
        unsigned n=0;
        while(n<AVP_NUM_AMPS && p[n].x!=-1)++n;
        if(n>AVP_ORIGINAL_NUM_AMPS)cap=AVP_NUM_AMPS;
    }
    host_amp_capacity=cap;
    for(unsigned i=0;i<cap;i++)free_list[i]=(u16)(cap-1u-i);
    for(unsigned i=cap;i<AVP_NUM_AMPS;i++)free_list[i]=0;
    free_count=cap;
}
