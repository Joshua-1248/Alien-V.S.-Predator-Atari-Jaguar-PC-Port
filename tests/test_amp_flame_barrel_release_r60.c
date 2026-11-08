/* R60: releasing a pooled flame must not erase the collision owner of
 * the barrel or living enemy it struck. AC_GAS is flamethrower ammunition,
 * not an explosive barrel; test the confirmed DRUM (definition 20) only.
 * Original flame damage remains 10 per projectile. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>

s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;

u32 ccn_xsave,ccn_ysave,x_pos=0x00128000u,y_pos=0x00348000u,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy,player_type=PT_HUMAN,use_cocoon;
volatile u16 seed0,seed1;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return ++seed0;}

static void test_barrel(void)
{
    AvpAmp *barrel,*flame;
    unsigned cell=52u*AVP_AMP_GRID_W+18u;
    InitAMPs();build_level();
    barrel=amp_req();
    barrel->xpos=(s32)x_pos;
    barrel->ypos=(s32)y_pos;
    barrel->creature=AC_DRUM;
    barrel->energy=20;
    barrel->oldenergy=20;
    barrel->flags=(1u<<AMP_KILLABLE);
    barrel->mode=avp_amp_builtin_mode_for_def(20u);
    assert(barrel->mode);
    barrel->mode(barrel); /* register source barrel in collmap */
    assert(collmap[cell]==1u);
    assert(barrel->energy==20);

    avp_amp_set_reset_counter(77);
    for(int i=0;i<2;i++){
        flame=player_weapon(0x4000,0,1,77,0,0);
        assert(flame && flame!=barrel && flame->creature==AC_FIRE);
        flame->xpos=barrel->xpos;
        flame->ypos=barrel->ypos;
        flame->timer=0;
        pre_flame_on(flame);
        assert(flame->mode==flame_on);
        flame_on(flame);
        assert(barrel->energy==(i==0?10:0));
        assert(flame->mode && flame->mode!=flame_on);
        flame->mode(flame); /* original flame impact tail releases AMP */
        assert(flame->mode==NULL);
        /* Before R60 this assertion fails after the first impact: release_amp
         * clears the barrel's slot instead of the projectile's own slot. */
        assert(collmap[cell]==1u);
    }
    barrel->mode(barrel); /* source 20-point drum reaches explosion mode */
    assert(barrel->mode==NULL);
    assert(collmap[cell]==0u);
}

static void test_living_enemy(u16 species,s16 initial_health)
{
    AvpAmp *enemy,*flame;
    const unsigned cell=52u*AVP_AMP_GRID_W+18u;
    InitAMPs();build_level();
    enemy=amp_req();
    enemy->xpos=(s32)x_pos;
    enemy->ypos=(s32)y_pos;
    enemy->creature=species;
    enemy->energy=initial_health;
    enemy->oldenergy=initial_health;
    enemy->flags=(1u<<AMP_KILLABLE);
    enemy->mode=chase_player;
    amp_setgrid(enemy);
    assert(collmap[cell]==1u);
    avp_amp_set_reset_counter(77);
    for(int i=0;i<2;i++){
        flame=player_weapon(0x4000,0,1,77,0,0);
        assert(flame && flame!=enemy && flame->creature==AC_FIRE);
        flame->xpos=enemy->xpos;
        flame->ypos=enemy->ypos;
        flame->timer=0;
        pre_flame_on(flame);
        assert(flame->mode==flame_on);
        flame_on(flame);
        assert(enemy->energy==initial_health-10*(i+1));
        assert(flame->mode && flame->mode!=flame_on);
        flame->mode(flame);
        assert(flame->mode==NULL);
        assert(enemy->mode==chase_player && collmap[cell]==1u);
    }
}

static void test_smart_disc_enemy(void)
{
    AvpAmp *enemy,*disc;
    const unsigned cell=52u*AVP_AMP_GRID_W+18u;
    InitAMPs();build_level();
    enemy=amp_req();
    enemy->xpos=(s32)x_pos;
    enemy->ypos=(s32)y_pos;
    enemy->creature=AC_PREDATOR;
    enemy->energy=300;
    enemy->oldenergy=300;
    enemy->flags=(1u<<AMP_KILLABLE);
    enemy->mode=chase_player;
    amp_setgrid(enemy);
    assert(collmap[cell]==1u);
    avp_amp_set_reset_counter(77);
    for(int i=0;i<2;i++){
        disc=player_weapon(0x4000,0,0,77,0,0);
        assert(disc && disc->creature==AC_SMART);
        disc->xpos=enemy->xpos;
        disc->ypos=enemy->ypos;
        pre_discmove(disc);
        assert(disc->mode==discmove);
        discmove(disc);
        assert(enemy->energy==300-50*(i+1));
        assert(disc->mode && disc->mode!=discmove);
        disc->mode(disc);
        assert(disc->mode==NULL);
        assert(enemy->mode==chase_player && collmap[cell]==1u);
    }
}

int main(void)
{
    assert(AC_GAS!=AC_DRUM);
    test_barrel();
    test_living_enemy(AC_ALIEN,30);
    test_living_enemy(AC_HUMAN,50);
    test_living_enemy(AC_PREDATOR,300);
    test_smart_disc_enemy();
    puts("Jaguar flame/drum/enemy and Smart Disc/enemy collision ownership PASS");
    return 0;
}
