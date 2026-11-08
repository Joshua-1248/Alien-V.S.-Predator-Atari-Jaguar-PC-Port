/* Optional original AMP loop, with per-mode binding callbacks supplied by the
 * multiplayer library.  The normal UpdateAMPs entry is unchanged.  Keeping
 * the callback implementation outside amp.c avoids cross-archive link order
 * dependencies and preserves single-player when multiplayer is disabled. */
int avp_mp_amp_native_update_scoped(void *ctx,
            int (*enter)(void *, const AvpAmp *, unsigned),
            int (*leave)(void *),
            int (*acid)(void *, const AvpAmp *))
{
    int ok=1;
    if(!ctx || !enter || !leave || !acid || amp_update_active ||
       amp_data!=level1amps || amps_at!=level1amps)return 0;
    /* This is the original UpdateAMPs prologue, preserved byte-for-byte
     * in the single-player function above; it runs ONCE per level. */
    if(marine_pulse_id && --marine_pulse_time==0){
        StopLoop(marine_pulse_id);marine_pulse_id=0;
    }
    if(marine_flame_id && --marine_flame_time==0){
        StopLoop(marine_flame_id);marine_flame_id=0;
    }
    entime=(u16)((entime+1u)&7u);explo1=0;
    amp_update_active=1;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        AvpAmp *a=&amp_data[i];
        cur_amp_no=(u16)(i+1u);
        if(a->host_static || !a->mode)continue;
        if(host_enemies_frozen && hostile_amp_type(a))continue;
        /* Acid is a floor hazard, not a creature selecting a single enemy
         * target. Sample every living player in this bound world on the
         * original tick, independent of target lock or line of sight. The
         * one original ALTFRAME bit still controls the one-shot acid cue. */
        if(a->mode==amp_acid){
            int contacts;
            if(entime!=0)a->oldenergy=a->energy;
            contacts=acid(ctx,a);
            if(contacts<0){ok=0;break;}
            if(contacts){
                if(!(a->flags&(1u<<AMP_ALTFRAME)))amp_sfx(AVP_FX_ACIDX);
                a->flags|=(u16)(1u<<AMP_ALTFRAME);
            }else a->flags&=(u16)~(1u<<AMP_ALTFRAME);
            continue;
        }
        /* Multiplayer-only callback contract: 1 = bind living target,
         * 2 = no living target (skip this enemy mode), 0 = real error.
         * Without the skip, a lone dead player in one of several worlds
         * faults the entire co-op session on the next AMP update. */
        int target_state=enter(ctx,a,i);
        if(target_state==2)continue;
        if(target_state!=1){ok=0;break;}
        /* These two instructions are the original AMP.S per-actor body. */
        if(entime!=0)a->oldenergy=a->energy;
        AvpAmpModeFn fn=a->mode;
        fn(a);
        if(!leave(ctx)){ok=0;break;}
    }
    amp_update_active=0;
    return ok;
}
