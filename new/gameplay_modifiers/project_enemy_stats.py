"""PC stat hooks: only in the generated compilation view."""
def apply_enemy_stats(path,data):
    if path not in ('src/game/amp.c','src/game/mazescrn.c','include/amp.h'):return data
    s=data.decode('utf-8')
    def once(old,new):
        nonlocal s
        if s.count(old)!=1:raise ValueError('Unexpected enemy-stat seam: '+old)
        s=s.replace(old,new)
    if path=='include/amp.h':
        once('#include "../new/gameplay_enhancements/enemy_health_declarations.inc"','#include "../new/gameplay_enhancements/enemy_health_declarations.inc"\n#include "../new/gameplay_modifiers/enemy_stats.h"')
    elif path=='src/game/mazescrn.c':
        once('#include "../../new/audio/pc_facehug_shake_option.inc"','#include "../../new/audio/pc_facehug_shake_option.inc"\n#include "../../new/gameplay_modifiers/facehugger_damage.inc"')
        once('player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,(s16)(player_energy-10));','player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,(s16)(player_energy-avp_stat_scale(10,host_facehugger_damage_units)));')
    else:
        once('#include "../../new/cheats/amp_host_options_state.inc"','#include "../../new/cheats/amp_host_options_state.inc"\n#include "../../new/gameplay_modifiers/enemy_stats.inc"')
        once('#include "../../new/gameplay_modifiers/enemy_stats.inc"','#include "../../new/gameplay_modifiers/enemy_stats.inc"\n#include "../../new/gameplay_modifiers/facehugger_instant_death.inc"')
        once('hugkill=1;amp_cleargrid_c(a);release_amp(a);','hugkill=1;facehugger_attachment_modifier();amp_cleargrid_c(a);release_amp(a);')
        once('a->aux_ptr=NULL;\n    /* The retail collectable mode','a->aux_ptr=NULL;\n    enemy_stat_spawn(a);\n    /* The retail collectable mode')
        once('lockxxx(a);a->energy=1000;a->oldenergy=1000;','lockxxx(a);a->energy=(s16)avp_stat_scale(1000,enemy_stat(AVP_STAT_QUEEN,AVP_STAT_HEALTH));a->oldenergy=a->energy;')
        once('a->timer=(s32)(50-a->energy);','a->timer=(s32)(50-a->energy);\n    if(enemy_stat(AVP_STAT_MARINE,AVP_STAT_HEALTH)!=4u){\n        int maxhp=avp_stat_scale(50,enemy_stat(AVP_STAT_MARINE,AVP_STAT_HEALTH));\n        a->timer=50-(50*(int)a->energy/maxhp);if(a->timer<1)a->timer=1;\n    }')
        # Movement only; do not change the template velocity or projectile aliases.
        start=s.index('static void amp_move_toward(AvpAmp *a,s32 tx,s32 ty)\n{')
        end=s.index('\nstatic void amp_fake_common',start)
        block=s[start:end].replace('a->xvel','enemy_actor_speed(a,a->xvel)').replace('a->yvel','enemy_actor_speed(a,a->yvel)')
        s=s[:start]+block+s[end:]
        once('amp_word_add(player_energy,-f->damage)','amp_word_add(player_energy,-enemy_actor_damage(a,f->damage))')
        once('if(can_hit)player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-100));','if(can_hit)player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-enemy_actor_damage(a,100)));')
        once('amp_word_add(player_energy,-scaled)','amp_word_add(player_energy,-enemy_actor_damage(a,scaled))')
        once('amp_word_add(player_energy,-(a->creature==AC_BOLT?100:25))','amp_word_add(player_energy,-enemy_projectile_damage(a,a->creature==AC_BOLT?100:25))')
        once('amp_word_add(player_energy,-50));\n        const AvpRuntimeOps*o','amp_word_add(player_energy,-enemy_projectile_damage(a,50)));\n        const AvpRuntimeOps*o')
        once('explosion_ex(a->xpos,a->ypos,-38,200,0x10000u)','explosion_ex(a->xpos,a->ypos,-38,(s16)enemy_projectile_damage(a,200),0x10000u)')
        for base in (100,50,10):
            once(f'e=amp_word_add(e,-{base})',f'e=amp_word_add(e,-enemy_projectile_damage(p,{base}))')
        # Capture the original type before the crawler becomes an Alien corpse.
        once('const s16 *seq=NULL;\n    a->flags','const s16 *seq=NULL;\n    unsigned damage_type=enemy_stat_category(a->creature);\n    a->flags')
        once('a->creature==AC_ALIEN && amp_alien_death_spray_in_range(a))\n        player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-50));','a->creature==AC_ALIEN && amp_alien_death_spray_in_range(a))\n        player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-enemy_stat_damage(damage_type,50)));')
    return s.encode('utf-8')
