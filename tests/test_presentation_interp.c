#include "jaguar/avp_gpu/presentation_interp.h"
#include "player.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    AvpPresentationPose p={0x00108000u,0x00208000u,0x10000000u};
    AvpPresentationPose c={0x0010a000u,0x00209000u,0x11000000u};
    AvpPresentationPose o={0,0,0};
    assert(avp_presentation_interpolate_pose(&p,&c,0.5,&o)==1);
    assert(o.x==0x00109000u);
    assert(o.y==0x00208800u);
    assert((u16)(o.angle>>16)==0x1080u);

    /* Wrap uses the shortest signed 16-bit angular delta. */
    p.angle=0xff000000u;c.angle=0x01000000u;
    assert(avp_presentation_interpolate_pose(&p,&c,0.5,&o)==1);
    assert((u16)(o.angle>>16)==0x0000u);

    /* A sub-16-bit enhanced heading must be smoothly interpolated, not
     * discarded in the host presentation step. */
    p.angle=0x00010000u;c.angle=0x00018000u;
    assert(avp_presentation_interpolate_pose(&p,&c,0.5,&o)==1);
    assert(o.angle==0x00018000u);
    /* Default mode drops fractional renderer bits exactly as R260 did. */
    assert(avp_presentation_interpolate_pose_continuous(&p,&c,0.5,&o)==1);
    assert(o.angle==0x00014000u);
    p.angle=0x00010000u;c.angle=0x00020000u;
    assert(avp_presentation_interpolate_pose_continuous(&p,&c,0.5,&o)==1);
    assert(o.angle==0x00018000u); /* no integer stepping at exact endpoints */
    p.angle=0xffff0000u;c.angle=0x00008000u;
    assert(avp_presentation_interpolate_pose(&p,&c,0.5,&o)==1);
    assert(o.angle==0xffff8000u); /* legacy integer interpolation retains current low word */
    assert(avp_presentation_interpolate_pose_continuous(&p,&c,0.5,&o)==1);
    assert(o.angle==0xffffc000u);

    /* Teleports and authored facing snaps must never be visually traversed. */
    p.x=0x00100000u;p.y=0x00100000u;p.angle=0;
    c.x=0x00110000u;c.y=0x00100000u;c.angle=0;
    assert(avp_presentation_interpolate_pose(&p,&c,0.5,&o)==0);
    assert(o.x==c.x && o.y==c.y && o.angle==c.angle);
    c=p;c.angle=0x40000000u;
    assert(avp_presentation_interpolate_pose(&p,&c,0.5,&o)==0);
    assert(o.angle==c.angle);

    assert(avp_presentation_interpolate_fixed(0,10<<16,0.5,32<<16)==5<<16);
    assert(avp_presentation_interpolate_fixed(0,40<<16,0.5,32<<16)==40<<16);
    assert(avp_presentation_interpolate_small(0,10,0.5,32)==5);
    assert(avp_presentation_interpolate_small(0,40,0.5,32)==40);
    /* Sprite-ID changes during one continuously selected Alien attack
     * must not interrupt host-only positional smoothing. */
    for(s16 weapon=1;weapon<=3;weapon++){
        assert(avp_presentation_alien_overlay_continuity(PT_ALIEN,weapon,1,
                                                         PT_ALIEN,weapon,1));
        assert(avp_presentation_interpolate_fixed(0,40<<16,0.5,-1)==20<<16);
        assert(avp_presentation_interpolate_fixed(0,72<<16,0.5,-1)==36<<16);
        assert(!avp_presentation_alien_overlay_continuity(PT_ALIEN,weapon,0,
                                                          PT_ALIEN,weapon,1));
        assert(!avp_presentation_alien_overlay_continuity(PT_ALIEN,weapon,1,
                                                          PT_ALIEN,weapon,0));
        assert(!avp_presentation_alien_overlay_continuity(PT_ALIEN,weapon,1,
                                                          PT_ALIEN,(s16)(weapon+1),1));
        assert(!avp_presentation_alien_overlay_continuity(PT_HUMAN,weapon,1,
                                                          PT_HUMAN,weapon,1));
    }
    assert(!avp_presentation_alien_overlay_continuity(PT_ALIEN,0,1,PT_ALIEN,0,1));
    /* Host-only frequency selection, sorted order and exact integer
     * nanosecond periods for all 21 choices. No Jaguar VBL involvement. */
    static const unsigned supported[]={
        24u,30u,50u,60u,75u,85u,90u,100u,120u,144u,165u,
        180u,200u,240u,280u,300u,360u,480u,500u,540u,1000u
    };
    assert(avp_presentation_rate_count()==sizeof(supported)/sizeof(supported[0]));
    for(unsigned i=0;i<avp_presentation_rate_count();i++){
        const unsigned hz=supported[i];
        assert(avp_presentation_rate_at(i)==hz);
        assert(avp_presentation_valid_rate(hz)==hz);
        assert(avp_presentation_host_frame_period(hz)==1000000000ull/hz);
    }
    assert(avp_presentation_rate_at(avp_presentation_rate_count())==60u);
    for(unsigned i=0;i<avp_presentation_rate_count();i++){
        const unsigned current=supported[i];
        assert(avp_presentation_rate_neighbor(current,1)==
               supported[(i+1u)%avp_presentation_rate_count()]);
        assert(avp_presentation_rate_neighbor(current,-1)==
               supported[(i+avp_presentation_rate_count()-1u)%avp_presentation_rate_count()]);
        assert(avp_presentation_rate_neighbor(current,0)==current);
    }
    assert(avp_presentation_rate_neighbor(10000u,1)==75u);
    assert(avp_presentation_rate_neighbor(10000u,-1)==50u);
    assert(avp_presentation_host_frame_period(60)==16666666ull);
    assert(avp_presentation_host_frame_period(120)==8333333ull);
    assert(avp_presentation_host_frame_period(240)==4166666ull);
    assert(avp_presentation_host_frame_period(1000)==1000000ull);
    assert(avp_presentation_host_frame_period(0)==16666666ull);
    assert(avp_presentation_host_frame_period(999)==16666666ull);
    {u64 vbl=0,frame60=0,frame120=0;
     const u64 vperiod=avp_presentation_host_frame_period(60);
     const u64 fperiod=avp_presentation_host_frame_period(120);
     for(u64 now=0;now<1000000000ull;now+=1000000ull){
         if(now>=vbl){vbl=avp_presentation_next_display_deadline(vbl,now,vperiod);frame60++;}
         if(now>=frame120){frame120=avp_presentation_next_display_deadline(frame120,now,fperiod);}
     }
     assert(frame60==60u || frame60==61u);
     assert(frame120>=999000000ull && frame120<=1008333333ull);}
    /* A late frame advances to the next deadline on the same grid, not
     * an arbitrary full interval measured from the late completion. */
    assert(avp_presentation_next_display_deadline(100,100,16)==116);
    assert(avp_presentation_next_display_deadline(100,119,16)==132);
    assert(avp_presentation_next_display_deadline(100,150,16)==164);
    assert(avp_presentation_next_display_deadline(200,100,16)==200);
    assert(avp_presentation_next_display_deadline(100,100,0)==100);
    /* A poll must not oversleep an imminent 60 Hz draw or an original game
     * tick.  Older R197 always slept 2ms, even with only 1ns left. */
    assert(avp_presentation_sleep_budget(100,10000,10000,101,2000)==1);
    assert(avp_presentation_sleep_budget(100,101,10000,10000,2000)==1);
    assert(avp_presentation_sleep_budget(100,10000,101,10000,2000)==1);
    assert(avp_presentation_sleep_budget(100,10000,10000,10000,2000)==2000);
    assert(avp_presentation_sleep_budget(100,100,10000,10000,2000)==0);
    assert(avp_presentation_sleep_budget(100,10000,10000,99,2000)==0);
    assert(avp_presentation_sleep_budget(100,10000,10000,101,0)==0);
    assert(avp_presentation_sleep_budget(UINT64_MAX-2,UINT64_MAX,
                                         UINT64_MAX,UINT64_MAX,2000)==2);
    return 0;
}
