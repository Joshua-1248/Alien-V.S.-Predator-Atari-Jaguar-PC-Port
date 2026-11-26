/* PC-only cheat: immediately despawn hostile AMP records from the currently
 * bound level. Do not invoke any death state: in particular Queen death would
 * advance the campaign, and generator death would damage the player.
 * Original gameplay never calls this without an explicit cheat action. */
unsigned avp_amp_dev_clear_enemies(void)
{
    u8 removed[AVP_NUM_AMPS]={0};
    unsigned n=0;
    int had_generator=0;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        AvpAmp *a=&amp_data[i];
        if((!a->mode&&!a->host_static) || (a->flags&(1u<<AMP_PLAYER)))continue;
        if(!hostile_amp_type(a))continue;
        if(a->creature==AC_GEN)had_generator=1;
        removed[i]=1u;
        release_amp(a);
        ++n;
    }
    /* Clear any remaining ownership entries belonging to despawned actors,
     * including a stale old movement cell. Never erase another actor, item,
     * static geometry or a $ffff map sentinel. */
    for(unsigned i=0;i<AVP_AMP_GRID_W*AVP_AMP_GRID_H;i++){
        const u16 owner=collmap[i];
        if(owner>0u && owner<=AVP_NUM_AMPS && removed[owner-1u])
            collmap[i]=0u;
    }
    for(unsigned i=0;i<sizeof(objmap);i+=2u){
        const unsigned owner=((unsigned)objmap[i]<<8)|objmap[i+1u];
        if(owner>0u && owner<=AVP_NUM_AMPS && removed[owner-1u])
            objmap[i]=objmap[i+1u]=0u;
    }
    if(n){
        /* These IDs are used only by enemy Marine firing loops. Stop the
         * voices now, rather than letting disembodied fire continue. */
        if(marine_pulse_id)StopLoop(marine_pulse_id);
        if(marine_flame_id)StopLoop(marine_flame_id);
        marine_pulse_id=marine_flame_id=0;
        marine_pulse_time=marine_flame_time=0;
    }
    if(had_generator){ngens=0;KillAlar();}
    return n;
}


/* PC-only hazard removal: no death state, blast, hit, score or pickup event.
 * Operates only on the active/bound level, just like Clear Enemies. The
 * Predator-ship electric pyramids are AC_AQSHIELD, maintained by AC_GEN.
 * Release their generators too, or the shields can be recreated. Original
 * death/Generator code is deliberately bypassed (it can damage the player).
 * An Alien's completed death may be a live acid puddle: remove only this
 * explicit mode, never a living Alien or one still playing its death sprite. */
unsigned avp_amp_dev_clear_hazards(void)
{
    u8 removed[AVP_NUM_AMPS]={0};
    unsigned n=0;
    int had_generator=0;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        AvpAmp *a=&amp_data[i];
        if((!a->mode&&!a->host_static) || (a->flags&(1u<<AMP_PLAYER)))continue;
        /* Retail templates 20 (DRUM) and 22 (GAS) both use drum_def.
         * The actual flamethrower ammo is template 15 (CAN/AC_CAN). */
        const int barrel=a->creature==AC_DRUM && (a->astype==20 || a->astype==22);
        if(!barrel && a->creature!=AC_AQSHIELD && a->creature!=AC_GEN &&
           !avp_amp_is_acid_puddle(a))continue;
        if(a->creature==AC_GEN)had_generator=1;
        removed[i]=1u;
        release_amp(a);
        ++n;
    }
    /* Scrub only grid entries owned by removed hazards; never clear an
     * unrelated occupant, item, map sentinel or a surviving enemy. */
    for(unsigned i=0;i<AVP_AMP_GRID_W*AVP_AMP_GRID_H;i++){
        const u16 owner=collmap[i];
        if(owner>0u && owner<=AVP_NUM_AMPS && removed[owner-1u])
            collmap[i]=0u;
    }
    for(unsigned i=0;i<sizeof(objmap);i+=2u){
        const unsigned owner=((unsigned)objmap[i]<<8)|objmap[i+1u];
        if(owner>0u && owner<=AVP_NUM_AMPS && removed[owner-1u])
            objmap[i]=objmap[i+1u]=0u;
    }
    if(had_generator){ngens=0;KillAlar();}
    return n;
}

/* Cheat kills use the authored death states, not free-slot deletion: corpse,
 * collision-grid, generators and queen progression remain coherent. */
unsigned avp_amp_dev_kill_enemies(int type)
{
    unsigned killed=0;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        AvpAmp *a=&amp_data[i];
        if(!a->mode||a->host_static||(a->flags&(1u<<AMP_DEAD))||a->energy<=0)continue;
        if(a->flags&(1u<<AMP_PLAYER))continue;
        if(type>=0 && a->creature!=(u16)type)continue;
        switch(a->creature){
        case AC_EGG:
            /* AMP.S's own destroyed-egg path: retains the visible dead egg,
             * clears its solid grid cell and prevents future Facehugger spawn.
             * This is a cheat, not a weapon hit, so award no score. */
            a->energy=0;
            a->flags&=(u16)~(1u<<AMP_PHIT);
            make_dead_egg(a);
            ++killed;
            break;
        case AC_ALIEN:case AC_ACRAWL:case AC_HUMAN:case AC_PREDATOR:case AC_FACEHUG:
            a->energy=0;host_dev_death=1;amp_begin_death(a);host_dev_death=0;
            ++killed;break;
        case AC_GEN:
            a->energy=0;
            /* Natural generator death releases its associated shield. */
            if(a->mode==genmode)genmode(a);
            ++killed;
            break;
        case AC_AQUEEN:
            a->energy=0;queen_dead_begin(a);++killed;break;
        default:break; /* Pickups, hazards and projectiles are not enemies. */
        }
    }
    return killed;
}
