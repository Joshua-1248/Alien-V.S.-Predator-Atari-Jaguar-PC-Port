/* RE41 R229: visibly destroyed Facehugger eggs are no longer enemy
 * map contacts. Their persistent static corpse remains world scenery.
 * Both normal and smooth presentation run the same live AMP classification. */
#include "jaguar/avp_gpu/automap_enemy_marker.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void eggwait_mock(AvpAmp *a){(void)a;}
int main(void)
{
    AvpAmp a;
    s32 halfway;
    int x,y;
    memset(&a,0,sizeof a);
    a.creature=AC_EGG;
    a.astype=3;  /* AMP.S DEF_EGG */
    a.mode=eggwait_mock;
    a.animseq=AS_STAND;
    a.energy=20;
    a.xpos=10<<16;
    a.ypos=10<<16;
    assert(avp_gpu_automap_enemy_letter(&a)=='E');
    /* Egg takes lethal damage: hide immediately, before state tick. */
    a.energy=0;
    assert(avp_gpu_automap_enemy_letter(&a)==0);
    /* make_dead_egg: DEF_DEADEGG=56, static, visually standing frame 5,
     * not AMP_DEAD or AS_DEATH. Keeping this E was the original bug. */
    a.mode=NULL; a.host_static=1;
    a.astype=56; a.animframe=5;
    a.animseq=AS_STAND; a.flags=0;
    assert(avp_gpu_automap_enemy_letter(&a)==0);
    /* A saved corpse could carry a stale positive energy value. */
    a.energy=20;
    assert(avp_gpu_automap_enemy_letter(&a)==0);
    /* Fresh live egg (same creature) and other live actors remain shown. */
    a.astype=3; a.mode=eggwait_mock; a.host_static=0;
    assert(avp_gpu_automap_enemy_letter(&a)=='E');
    a.creature=AC_FACEHUG;
    assert(avp_gpu_automap_enemy_letter(&a)=='F');
    a.creature=AC_EGG; a.energy=0;
    assert(avp_gpu_automap_enemy_letter(&a)==0);
    /* Smooth presentation may interpolate coordinates, not vitality.
     * The caller must first check the actor's current classification. */
    halfway=avp_presentation_interpolate_fixed(10<<16,11<<16,0.5,2L<<16);
    assert(halfway==((10<<16)+32768));
    assert(avp_gpu_automap_enemy_screen(10<<16,10<<16,0,32767,1.0,
        halfway,10<<16,100,100,&x,&y));
    assert(x==100 && y==96);
    assert(avp_gpu_automap_enemy_letter(&a)==0);
    puts("R229 destroyed eggs hidden; live eggs and smooth positions preserved");
    return 0;
}
