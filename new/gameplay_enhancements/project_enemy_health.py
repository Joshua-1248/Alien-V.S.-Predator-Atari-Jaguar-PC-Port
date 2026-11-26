"""Inject narrow opt-in PC hooks in the disposable projection only."""
def apply_enemy_health(path,data):
    if path not in ('src/game/amp.c','include/amp.h'):return data
    s=data.decode('utf-8')
    def once(old,new):
        nonlocal s
        if s.count(old)!=1:raise ValueError('Unexpected enemy-health seam: '+old)
        s=s.replace(old,new)
    if path=='include/amp.h':
        once('#define AVP_NUM_AMPS 300', '#include "../new/gameplay_modifiers/amp_capacity.h"')
        once('typedef struct AvpAmpSavedPlacement { s16 x,y,astype; u16 flags; } AvpAmpSavedPlacement;',
             'typedef struct AvpAmpSavedPlacement { s16 x,y,astype; u16 flags;\n#include "../new/gameplay_enhancements/enemy_health_fields.inc"\n} AvpAmpSavedPlacement;\n#include "../new/gameplay_enhancements/enemy_health_declarations.inc"')
    else:
        once('static AvpAmpSavedPlacement saved_levels[AVP_MAX_LEVEL][AVP_NUM_AMPS+1];',
             'static AvpAmpSavedPlacement saved_levels[AVP_MAX_LEVEL][AVP_NUM_AMPS+1];\nstatic void persistent_health_save(AvpAmpSavedPlacement *,const AvpAmp *);\nstatic void persistent_health_restore(AvpAmp *,const AvpAmpSavedPlacement *);')
        once('        out[n].flags=a->flags;\n        ++n;', '        out[n].flags=a->flags;\n        persistent_health_save(&out[n],a);\n        ++n;')
        once('    out[n].x=-1;out[n].y=0;out[n].astype=0;out[n].flags=0;', '    out[n]=(AvpAmpSavedPlacement){0};out[n].x=-1;')
        once('            if(a->flags&(1u<<AMP_COLLECT))set_static(a);','            if(a->flags&(1u<<AMP_COLLECT))set_static(a);\n            persistent_health_restore(a,p);')
        once('static void dead_gen2(AvpAmp *a)\n{', '#include "../../new/gameplay_enhancements/generator_blast.inc"\nstatic void dead_gen2(AvpAmp *a)\n{')
        once('        a->mode=NULL;a->host_static=0;a->aux_ptr=NULL;\n        player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-50));',
             '        a->mode=NULL;a->host_static=0;a->aux_ptr=NULL;\n        if(!host_generator_blast(a))\n            player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-50));')
        once('static void amp_crawlfight(AvpAmp *a)\n{\n    a->mode=amp_crawlfight;if(a->energy<=0)',
             '#include "../../new/gameplay_enhancements/crawl_death_interrupt.inc"\nstatic void amp_crawlfight(AvpAmp *a)\n{\n    a->mode=amp_crawlfight;if(host_crawl_death_interrupt && a->energy<=0)')
        once('        rand_set();','        rand_set();\n        modifier_initial_population();\n        modifier_vent_facehuggers();')
        once('static unsigned free_count;', 'static unsigned free_count;\nstatic void modifier_initial_population(void);\nstatic void modifier_vent_facehuggers(void);')
        once('static void reset_free_list(void)\n{\n    for(unsigned i=0;i<AVP_NUM_AMPS;++i)free_list[i]=(u16)(AVP_NUM_AMPS-1u-i);\n    free_count=AVP_NUM_AMPS;\n}', '#include "../../new/gameplay_modifiers/amp_capacity.inc"\nstatic void reset_free_list(void)\n{modifier_reset_pool();}')
        once('if(free_count==0) return &level1amps[AVP_NUM_AMPS-1];','if(free_count==0) return &level1amps[host_amp_capacity-1u];')
        once('if(index>=AVP_NUM_AMPS) index=AVP_NUM_AMPS-1;', 'if(index>=host_amp_capacity) index=host_amp_capacity-1u;')
        once('if(index>=AVP_NUM_AMPS || free_count>=AVP_NUM_AMPS)return;', 'if(index>=host_amp_capacity || free_count>=host_amp_capacity)return;')
        once('for(unsigned i=0;i<AVP_NUM_AMPS;i++){\n        AvpAmp *a=&amp_data[i];cur_amp_no=', 'for(unsigned i=0;i<host_amp_capacity;i++){\n        AvpAmp *a=&amp_data[i];cur_amp_no=')
        s+='\n#include "../../new/gameplay_enhancements/enemy_health.inc"\n#include <stdio.h>\n#include "../../new/gameplay_modifiers/enemy_multiplier.inc"\n#include "../../new/gameplay_modifiers/vent_facehuggers.inc"\n' 
    return s.encode('utf-8')
