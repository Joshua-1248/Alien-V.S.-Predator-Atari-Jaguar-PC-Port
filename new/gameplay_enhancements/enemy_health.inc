/* Included after the native AMP mode definitions, only in the PC projection.
 * Pair with the exact saved placement, never a recycled runtime slot. */
enum { HEALTH_NONE, HEALTH_NORMAL, HEALTH_QUEEN_ACTIVE,
       HEALTH_QUEEN_WAIT, HEALTH_QUEEN_REGEN, HEALTH_QUEEN_DYING, HEALTH_HAZARD };
static int host_persistent_enemy_health;
static int host_persistent_hazard_health;
void avp_amp_set_persistent_hazard_health(int enabled)
{ host_persistent_hazard_health=enabled!=0; }

/* Retail GAS (22) aliases the explosive drum template; CAN (15) is ammo.
 * 43 is the alarm controller, 49 a non-killable shield effect. */
static int persistent_hazard_template(s16 def)
{
    return def==20 || def==22 || (def>=44 && def<=48);
}
void avp_amp_set_persistent_enemy_health(int enabled)
{ host_persistent_enemy_health=enabled!=0; }

static int persistent_enemy_template(s16 def)
{
    return (def>=0 && def<=5) || (def>=29 && def<=37) ||
           (def>=50 && def<=51) || def==58 || def==59;
}
static void persistent_health_save(AvpAmpSavedPlacement *out,const AvpAmp *a)
{
    out->host_saved_energy=0;
    out->host_saved_health_kind=HEALTH_NONE;
    out->host_saved_queen_frame=0;
    if(!a->mode)return;
    if(persistent_hazard_template(a->astype)){
        if(host_persistent_hazard_health){
            out->host_saved_energy=a->energy;
            out->host_saved_health_kind=HEALTH_HAZARD;
        }
        return;
    }
    if(!host_persistent_enemy_health)return;
    if(a->astype==57 && a->mode==queen_wait_card_mode){
        out->host_saved_health_kind=HEALTH_QUEEN_WAIT;
        return;
    }
    if(!persistent_enemy_template(a->astype))return;
    out->host_saved_energy=a->energy;
    out->host_saved_health_kind=HEALTH_NORMAL;
    if(a->creature==AC_AQUEEN){
        if(a->mode==queen_regen_mode){
            out->host_saved_health_kind=HEALTH_QUEEN_REGEN;
            out->host_saved_queen_frame=a->animframe;
        }else if(a->mode==queen_play_death_mode){
            out->host_saved_health_kind=HEALTH_QUEEN_DYING;
            out->host_saved_queen_frame=a->animframe;
        }else if(a->astype==59 || (a->astype==50 && a->mode!=amp_queen_start_mode)){
            out->host_saved_health_kind=HEALTH_QUEEN_ACTIVE;
        }
    }
}
static void persistent_health_restore(AvpAmp *a,const AvpAmpSavedPlacement *p)
{
    if(p->host_saved_health_kind==HEALTH_HAZARD){
        if(host_persistent_hazard_health && persistent_hazard_template(p->astype))
            a->energy=a->oldenergy=p->host_saved_energy;
        return;
    }
    if(!host_persistent_enemy_health || !p->host_saved_health_kind)return;
    if(p->host_saved_health_kind==HEALTH_QUEEN_WAIT && p->astype==57){
        queen_respawns=1;
        a->host_static=0;a->mode=queen_wait_card_mode;
        return;
    }
    if(!persistent_enemy_template(p->astype))return;
    /* Equalize the damage-history baseline: revisiting is not a new hit. */
    a->energy=a->oldenergy=p->host_saved_energy;
    if(a->creature!=AC_AQUEEN)return;
    switch(p->host_saved_health_kind){
    case HEALTH_QUEEN_ACTIVE:
        if(p->astype==59)queen_respawns=1;
        lockxxx(a);break;
    case HEALTH_QUEEN_REGEN:
        queen_respawns=1;
        a->animseq=AS_DEATH;a->animframe=p->host_saved_queen_frame;
        a->mode=queen_regen_mode;break;
    case HEALTH_QUEEN_DYING:
        if(p->astype==59)queen_respawns=1;
        a->animseq=AS_DEATH;a->animframe=p->host_saved_queen_frame;
        a->mode=queen_play_death_mode;break;
    default:break;
    }
}
