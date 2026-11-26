#include "local_mp_life.h"
#include "local_mp_death_camera.h"
/* Keep test checks active even with Release/NDEBUG. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
int main(void){
    AvpMpLife a={0},b={0};
    uint32_t px[2]={0x00ff8040u,0x00aa5522u};
    avp_mp_life_begin(&b);
    assert(avp_mp_life_busy(&b) && !avp_mp_life_busy(&a));
    for(int i=0;i<63;i++)assert(!avp_mp_life_display_step(&b));
    assert(b.fade==126 && b.phase==AVP_MP_LIFE_FADE_OUT);
    assert(avp_mp_life_display_step(&b) && b.fade==128);
    avp_mp_life_apply(&b,px,1,1,2);
    assert(px[0]==0 && px[1]==0x00aa5522u);
    avp_mp_life_resumed(&b);
    /* A vent traveler stays absent and immune throughout destination fade-in. */
    for(int i=0;i<63;i++){
        assert(avp_mp_life_busy(&b));
        assert(!avp_mp_life_display_step(&b));
    }
    assert(avp_mp_life_busy(&b) && b.fade==2u);
    assert(!avp_mp_life_display_step(&b));
    assert(b.phase==AVP_MP_LIFE_ALIVE && b.fade==0);
    avp_mp_life_begin(&a);avp_mp_life_wait(&a);
    assert(avp_mp_life_busy(&a) && avp_mp_life_busy(&b)==0);
    /* Rendering must hold the original pose through the fade and allow
     * ordinary walking/turning to be visible again after a successful join. */
    {AvpMpDeathCamera camera={0}; uint32_t x=100,y=200,angle=300;
     avp_mp_death_camera_begin(&camera,x,y,angle);
     x=999;y=999;angle=999;
     avp_mp_death_camera_apply(&camera,&x,&y,&angle);
     assert(x==100 && y==200 && angle==300 && camera.active);
     avp_mp_death_camera_end(&camera);
     x=400;y=500;angle=600;
     avp_mp_death_camera_apply(&camera,&x,&y,&angle);
     assert(x==400 && y==500 && angle==600 && !camera.active);
    }
    /* A duct fade must dim one unchanged source scene; new render contents
     * cannot bleed into the fading viewport. Keep padding and P2 untouched. */
    {AvpMpLife f={0}; AvpMpLifeFrame held={0};
     uint32_t raster[6]={0xffaa3300u,0xffcc4400u,0xdeadbeefu,
                         0xff112233u,0xff778899u,0xdeadbeefu};
     unsigned history=0u;
     assert(avp_mp_life_level_changed(&history,3u));
     assert(!avp_mp_life_level_changed(&history,3u));
     assert(avp_mp_life_level_changed(&history,11u));
     assert(history==11u);
     avp_mp_life_begin(&f);
     assert(avp_mp_life_frame_preserve(&held,&f,raster,2u,2u,3u));
     assert(held.width==2u && held.height==2u);
     raster[0]=0x00112233u;raster[1]=0x00ffffffu;
     raster[3]=0x00ffffffu;raster[4]=0;
     assert(avp_mp_life_frame_preserve(&held,&f,raster,2u,2u,3u));
     assert(raster[0]==0xffaa3300u&&raster[1]==0xffcc4400u);
     assert(raster[3]==0xff112233u&&raster[4]==0xff778899u);
     assert(raster[2]==0xdeadbeefu&&raster[5]==0xdeadbeefu);
     avp_mp_life_wait(&f);
     assert(avp_mp_life_frame_preserve(&held,&f,raster,2u,2u,3u));
     avp_mp_life_resumed(&f);
     raster[0]=0xff998877u;
     assert(avp_mp_life_frame_preserve(&held,&f,raster,2u,2u,3u));
     assert(!held.pixels && raster[0]==0xff998877u);
     avp_mp_life_frame_release(&held);
    }
    puts("independent death fades, camera release, per-view dimming and resume: PASS");
    return 0;
}
