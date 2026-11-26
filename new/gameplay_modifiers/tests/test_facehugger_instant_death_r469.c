/* Exercise the actual projected AMP close-attack routine, not a mock. */
#define main crawl_regression
#include "../../gameplay_enhancements/tests/test_crawl_death_interrupt_r460.c"
#undef main
static AvpAmp *hug_setup(int type,int hp){
    AvpAmp *a=setup(1);player_type=(s16)type;player_energy=(s16)hp;hugkill=0;
    a->creature=AC_FACEHUG;a->astype=4;a->mode=amp_closehug;a->animseq=AS_FIGHT1;a->animframe=0;
    avp_amp_set_attacks_disabled(0);avp_amp_set_facehug_attach_disabled(0);return a;
}
int main(void){
    assert(crawl_regression()==0);assert(!host_facehugger_instant_death);
    const int hp[]={1,50,1000,32767};const int species[]={PT_HUMAN,PT_PREDATOR};
    unsigned cases=0;
    for(unsigned s=0;s<2;s++)for(unsigned i=0;i<4;i++)for(int enabled=0;enabled<2;enabled++)for(int immune=0;immune<2;immune++){
        AvpAmp *a=hug_setup(species[s],hp[i]);unsigned before=free_count;
        avp_amp_set_facehugger_instant_death(enabled);avp_new_damage_gate_set(immune);
        for(int f=1;f<=3;f++){a->mode(a);assert(a->animframe==f&&hugkill==0&&player_energy==hp[i]&&free_count==before);}
        a->mode(a);assert(hugkill==1&&a->mode==NULL&&free_count==before+1);
        assert(player_energy==(enabled&&!immune?0:hp[i]));++cases;
    }
    avp_new_damage_gate_set(0);avp_amp_set_facehugger_instant_death(1);
    for(int disable=0;disable<2;disable++){
        AvpAmp *a=hug_setup(PT_HUMAN,1000);a->animframe=3;
        avp_amp_set_attacks_disabled(!disable);avp_amp_set_facehug_attach_disabled(disable);
        a->mode(a);assert(!hugkill&&player_energy==1000&&a->mode==chase_player);++cases;
    }
    AvpAmp *a=hug_setup(PT_HUMAN,-5);a->animframe=3;a->mode(a);assert(player_energy==-5);++cases;
    a=hug_setup(PT_HUMAN,1000);avp_amp_set_facehugger_instant_death(0);a->mode(a);avp_amp_set_facehugger_instant_death(1);for(int f=0;f<3;f++)a->mode(a);assert(player_energy==0);++cases;
    printf("PASS: %u native attachment cases, final-frame lethality, default behavior, damage gate, disabled attacks, actor release and live toggle\n",cases);return 0;
}
