/* R138: Marine facing selection, retaining left/right sprite parity.
 * Exercise the actual host renderer sprite-choice function directly. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s at line %d\n",#c,__LINE__);exit(1);}}while(0)

static void check(u16 facing, double camx, double camy, int sheet, int reflection)
{
    AvpAmp marine;
    PcSpriteChoice c;
    memset(&marine,0,sizeof(marine));
    marine.creature=AC_HUMAN;
    marine.animseq=AS_RUN;
    marine.angle=facing;
    marine.xpos=10*65536;
    marine.ypos=10*65536;
    c=sprite_choice(&marine,camx,camy);
    CHECK(c.aaf==sheet);
    CHECK(c.mirror==reflection);
}

static void check_player(u16 seq, double camx,double camy,int sheet,int reflection)
{
    AvpAmp marine;
    PcSpriteChoice c;
    memset(&marine,0,sizeof(marine));
    marine.creature=AC_HUMAN;
    marine.flags=(u16)(1u<<AMP_PLAYER);
    marine.animseq=seq;
    marine.angle=0x0000;
    marine.xpos=10*65536;
    marine.ypos=10*65536;
    c=sprite_choice(&marine,camx,camy);
    CHECK(c.aaf==sheet);
    CHECK(c.mirror==reflection);
}

int main(void)
{
    /* +X-facing Marine: camera behind, ahead, on either flank. */
    check(0x0000, 8.,10.,56,0);
    check(0x0000,12.,10.,24,0);
    check(0x0000,10., 8.,55,1);
    check(0x0000,10.,12.,55,0);
    /* Rotate by 90,180,270 degrees; front/rear remain the same sheets. */
    check(0xc000,10., 8.,56,0);
    check(0xc000,10.,12.,24,0);
    check(0x8000,12.,10.,56,0);
    check(0x8000, 8.,10.,24,0);
    check(0x4000,10.,12.,56,0);
    check(0x4000,10., 8.,24,0);
    /* Original AMP.S enemy pulse rifle = FIGHT1 / AAF18,
     * enemy standing flamethrower = FIGHT2 / AAF15. These attack sheets
     * are only used for player Marine sprites facing the observer. */
    check_player(AS_FIGHT1,12.,10.,18,0);
    check_player(AS_FIGHT2,12.,10.,15,0);
    check_player(AS_FIGHT1, 8.,10.,56,0);
    check_player(AS_FIGHT2, 8.,10.,56,0);
    check_player(AS_FIGHT1,10., 8.,55,1);
    check_player(AS_FIGHT2,10.,12.,55,0);
    check_player(AS_WALK,12.,10.,24,0); /* stationary front */
    check_player(AS_WALK, 8.,10.,56,0); /* stationary rear */
    check_player(AS_WALK,10., 8.,55,1); /* stationary side */
    /* Do not change the original ENEMY marine animation selection. */
    { AvpAmp enemy={0};
      enemy.creature=AC_HUMAN;enemy.animseq=AS_FIGHT1;
      enemy.xpos=10*65536;enemy.ypos=10*65536;
      CHECK(sprite_choice(&enemy,12.,10.).aaf==18);
      enemy.animseq=AS_FIGHT2;
      CHECK(sprite_choice(&enemy,12.,10.).aaf==15);
    }
    puts("Marine front/rear/flank, player-only standing/pulse firing sheets PASS");
    return 0;
}
