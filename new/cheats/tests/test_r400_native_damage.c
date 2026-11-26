#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../../src/game/amp.c"
#include "../../../tests/amp_link_stubs.c"

u32 x_pos,y_pos,fire_distance,ccn_xsave,ccn_ysave;
s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u16 invisflag;
s32 score;
s16 player_type,player_energy,use_cocoon;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}

int main(void)
{
    InitAMPs();build_level();
    AvpAmp *a=amp_req();
    assert(a);
    a->xpos=a->ypos=x_pos=y_pos=0x00108000;
    a->mode=amp_acid;
    player_type=PT_HUMAN;
    player_energy=73;
    assert(!avp_new_damage_gate_blocked());
    amp_acid(a);assert(player_energy==70);
    avp_new_damage_gate_set(1);
    for(unsigned i=0;i<20;i++){
        amp_acid(a);assert(player_energy==70);
        assert(avp_amp_acid_contact_player(a)==1);assert(player_energy==70);
    }
    player_energy+=11;assert(player_energy==81);
    amp_acid(a);assert(player_energy==81);
    player_energy=1;assert(avp_amp_acid_contact_player(a)==1);assert(player_energy==1);
    avp_new_damage_gate_set(0);
    assert(avp_amp_acid_contact_player(a)==1);assert(player_energy==-2);
    player_energy=60;avp_new_damage_gate_set(1);amp_acid(a);assert(player_energy==60);
    avp_new_damage_gate_set(0);player_energy=40;amp_acid(a);assert(player_energy==37);
    puts("R400 native AMP acid, lethal damage, healing preservation, scope isolation PASS");
    return 0;
}
