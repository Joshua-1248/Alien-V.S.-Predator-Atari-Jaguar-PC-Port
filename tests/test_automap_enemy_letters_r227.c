/* RE41 R227: 16.16 subcell positions, upright glyph projection, and
 * filtering of live AMP enemies (not projectiles, corpses or pickups). */
#include "jaguar/avp_gpu/automap_enemy_marker.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void living_mode(AvpAmp *a){(void)a;}
static void check_enemy_letters(void)
{
    static const struct {unsigned type; char glyph;} cases[]={
        {AC_ALIEN,'A'},{AC_ACRAWL,'A'},{AC_AQUEEN,'Q'},
        {AC_FACEHUG,'F'},{AC_EGG,'E'},{AC_HUMAN,'M'},
        {AC_PREDATOR,'P'},{AC_SMART,0},{AC_FIRE,0},{AC_FOOD,0},
        {AC_GEN,0},{AC_COCOON,0},{AC_DEADMED,0}
    };
    AvpAmp a;memset(&a,0,sizeof(a));a.mode=living_mode;a.animseq=AS_STAND;a.energy=20;
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
        a.creature=(u16)cases[i].type;
        assert(avp_gpu_automap_enemy_letter(&a)==cases[i].glyph);
    }
    a.creature=AC_ALIEN;a.mode=NULL;
    assert(!avp_gpu_automap_enemy_letter(&a));
    a.host_static=1;
    assert(avp_gpu_automap_enemy_letter(&a)=='A');
    a.flags=(u16)(1u<<AMP_DEAD);
    assert(!avp_gpu_automap_enemy_letter(&a));
    a.flags=(u16)(1u<<AMP_PLAYER);
    assert(!avp_gpu_automap_enemy_letter(&a)); /* co-op participants are not enemies */
    a.flags=0;a.animseq=AS_DEATH;
    assert(!avp_gpu_automap_enemy_letter(&a));
    a.animseq=AS_STAND;
    assert(avp_gpu_automap_enemy_letter(&a)=='A');
}
static void project(s32 sin,s32 cos,s32 x,s32 y,int *px,int *py,double zoom)
{
    assert(avp_gpu_automap_enemy_screen(10<<16,10<<16,sin,cos,zoom,
        x,y,100,100,px,py));
}
int main(void)
{
    int x,y,x2,y2;
    check_enemy_letters();
    project(0,32767,10<<16,10<<16,&x,&y,1.0);
    assert(x==100 && y==100);
    /* HALF a cell must move the label HALF the normal 8 px per cell:
     * truncating to the tile would incorrectly keep the marker centered. */
    project(0,32767,(10<<16)+32768,10<<16,&x,&y,1.0);
    assert(x==100 && y==96);
    project(0,32767,(10<<16)+65536,10<<16,&x2,&y2,1.0);
    assert(x2==100 && y2==92);
    project(0,32767,(10<<16)+32768,10<<16,&x2,&y2,2.0);
    assert(x2==100 && y2==92); /* zoom doubles the true displacement */
    /* Automap rotates position; label bitmap is a fixed screen-space
     * raster that is independent of heading. */
    project(32767,0,(10<<16)+32768,10<<16,&x2,&y2,1.0);
    assert(x2==104 && y2==100);
    project(0,-32767,(10<<16)+32768,10<<16,&x2,&y2,1.0);
    assert(x2==100 && y2==104);
    /* Smooth presentation samples a fractional entity position using the
     * same helper as the AAF sprites. Never snap markers to map cells. */
    {
        s32 mid=avp_presentation_interpolate_fixed(10<<16,11<<16,0.5,2L<<16);
        assert(mid==(10<<16)+32768);
        project(0,32767,mid,10<<16,&x2,&y2,1.0);
        assert(x2==100 && y2==96);
    }
    /* Negative fractional offsets are preserved too. */
    project(0,32767,(10<<16)-32768,10<<16,&x2,&y2,1.0);
    assert(x2==100 && y2==104);
    assert(!avp_gpu_automap_enemy_screen(0,0,0,0,1.0,0,0,0,0,&x,&y));
    puts("R227 exact subcell automap markers: passed");
    return 0;
}
