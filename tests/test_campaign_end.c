#include "maze.h"
#include "player.h"
#include "hud.h"
#include "levels.h"
#include "avp_runtime.h"
#include <assert.h>

/* The retail MAZE.S end-count logic also drives the hosted game loop.
 * Exercise victory, death, destruction and Alien cocoon reuse directly. */
s16 fade_rate,fade_lim,fade_level;
s8 faded,in_fade;
s16 player_type,player_energy,player_dead,game_over,key_lock,destruct_flag,launch_flag,cheat;
u8 wep_fire;
s16 use_cocoon;
s16 cur_level,new_level;
static unsigned cocoons_used,explosion_sfx;
static int cocoon_ready;
int CheckCocoon(void){return cocoon_ready;}
void UseCocoon(void){
    ++cocoons_used;
    new_level=cur_level; /* A legitimate same-level cocoon transfer. */
    player_energy=1000;
}
static void sfx(void *user,unsigned id){(void)user;(void)id;++explosion_sfx;}
int main(void)
{
    AvpRuntimeOps o={0};
    o.play_sfx=sfx;
    avp_runtime_bind(&o);
    player_type=PT_HUMAN;player_energy=1000;game_over=1;
    Process_EndGame();
    assert(end_count==10 && key_lock==-1);
    for(unsigned i=0;i<9;i++)assert(!avp_campaign_advance_end_count());
    assert(end_count==1);
    assert(avp_campaign_advance_end_count() && end_count==0);
    assert(!avp_campaign_advance_end_count());

    game_over=0;key_lock=0;player_energy=0;player_dead=0;
    Process_EndGame();
    assert(end_count==10 && player_dead==-1 && key_lock==-1);
    for(unsigned i=0;i<9;i++)assert(!avp_campaign_advance_end_count());
    assert(avp_campaign_advance_end_count());

    game_over=0;key_lock=0;player_dead=0;player_energy=0;
    player_type=PT_ALIEN;cur_level=14;new_level=0;cocoon_ready=1;
    Process_EndGame();
    assert(end_count==40 && use_cocoon && !player_dead);
    for(unsigned i=0;i<40;i++)assert(!avp_campaign_advance_end_count());
    assert(end_count==0 && cocoons_used==1 && new_level==cur_level);
    assert(player_energy==1000);

    use_cocoon=0;player_type=PT_HUMAN;player_energy=1000;
    game_over=1;key_lock=0;destruct_flag=1;launch_flag=0;explosion_sfx=0;
    Process_EndGame();
    assert(end_count==AVP_EXP_START);
    for(unsigned i=0;i<AVP_EXP_START-1;i++)assert(!avp_campaign_advance_end_count());
    assert(avp_campaign_advance_end_count());
    assert(explosion_sfx==2); /* retail cues at countdown 12 and 11 */
    avp_runtime_reset_ops();
    return 0;
}
