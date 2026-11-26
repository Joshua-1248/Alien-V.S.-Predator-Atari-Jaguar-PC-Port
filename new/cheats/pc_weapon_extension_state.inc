static int mp_fast_equip;
static int host_unlimited_shooting_range;
/* A separate, optional PC behavior, NOT part of the original Jaguar
 * MAZESCRN.S script. Keep combo progress in each player's native weapon
 * context for co-op and discard the pending combo across level resets. */
static int host_alien_cocoon_infinite_ammo_assist;
static AvpAmp *host_cocoon_target;
static u8 host_cocoon_stage; /* 0 none, 1 landed claw, 2 landed tail */
static u32 host_cocoon_tick,host_cocoon_last_hit_tick;
void avp_mazescrn_set_alien_cocoon_infinite_ammo_assist(int enabled)
{
    host_alien_cocoon_infinite_ammo_assist=enabled!=0;
    if(!host_alien_cocoon_infinite_ammo_assist){
        host_cocoon_target=NULL;
        host_cocoon_stage=0;
    }
}
static void host_cocoon_shot(AvpAmp *hit,s16 weapon,s16 damage)
{
    /* The animation interpreter has already applied the original damage
     * and TestSpark has identified the actual enemy.  Only a successful
     * first Claw on a living enemy Marine can arm the Tail exception.
     * Do not touch PvP avatars, Aliens, Predators, bites, ordinary tails,
     * or any attack when infinite ammo / this preference is disabled. */
    if(!host_alien_cocoon_infinite_ammo_assist||player_type!=PT_ALIEN)
        return;
    if(host_cocoon_stage &&
       host_cocoon_tick-host_cocoon_last_hit_tick>50u){
        host_cocoon_stage=0;
        host_cocoon_target=NULL;
    }
    if(!hit||hit->creature!=AC_HUMAN||
       !(hit->flags&(1u<<AMP_KILLABLE))){
        host_cocoon_stage=0;
        host_cocoon_target=NULL;
        return;
    }
    if(weapon==1&&host_cocoon_stage==1&&hit==host_cocoon_target&&
       !(hit->flags&(1u<<AMP_STUN))&&
       hit->energy+damage>0){
        /* Only the middle Tail's damage is negated. It still connects,
         * makes the normal contact sound and toggles AMP_STUN below.
         * The final Claw keeps its full damage and moves the Marine into
         * the existing stunned-body/cocoon state on the next AI update. */
        hit->energy=(s16)(hit->energy+damage);
        host_cocoon_stage=2;
        host_cocoon_last_hit_tick=host_cocoon_tick;
        return;
    }
    if(weapon==2){
        /* The third, lethal Claw stays original. A further healthy Claw
         * begins a fresh sequence; never carry a dead target forward. */
        host_cocoon_stage=hit->energy>0?1:0;
        host_cocoon_target=host_cocoon_stage?hit:NULL;
        host_cocoon_last_hit_tick=host_cocoon_tick;
    }else{
        host_cocoon_stage=0;
        host_cocoon_target=NULL;
    }
}

void avp_mazescrn_set_unlimited_shooting_range(int enabled)
{host_unlimited_shooting_range=enabled!=0;}
/* A per-player presentation phase, saved alongside the native weapon scope.
 * Fast-Equip advances only original move-out/in command streams twice per
 * 10 Hz tick. It never accelerates attack, ammunition, or projectile scripts. */
static u8 mp_equip_phase;
void avp_mazescrn_mp_fast_equip(int enabled){mp_fast_equip=!!enabled;}
/* Executed only while the dying multiplayer actor's native weapon and
 * reserved audio voice are bound. KillSfx stops an active loop immediately;
 * StopLoop would merely clear its loop flag and let its current tail play. */
void avp_mazescrn_mp_stop_weapon_audio(void)
{
    if(wepfxID && wepfxID!=(u32)-1)KillSfx(wepfxID);
    wepfxID=0;
    wep_fire=0;
    wait_state=0;
}
