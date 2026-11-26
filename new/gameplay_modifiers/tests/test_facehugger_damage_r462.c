#include <stdio.h>
#define main wristblade_regression
#include "../../../tests/test_pred_wristblade.c"
#undef main
#include "../enemy_stats.h"
int main(void){
    assert(wristblade_regression()==0);
    for(unsigned n=1;n<=16;n++){
        player_energy=1000;hugkill=1;hug_recov=0;hug_throw=9;unstick_dir=0;
        joy_edge=0xffffffffu;avp_mazescrn_set_facehugger_damage(n);
        update_wep();assert(player_energy==1000-avp_stat_scale(10,n));assert(hug_throw==9);
    }
    avp_mazescrn_set_facehugger_damage(0);player_energy=1000;update_wep();assert(player_energy==990);
    puts("PASS: original wristblade and shake-off behavior, all 16 facehugger damage values and invalid preference");return 0;
}
