#include "local_mp_transition.h"
#include <stdio.h>
#include <string.h>
#define CK(e) do {if(!(e)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
int main(void)
{
    AvpMpTransitionQueue q={0};AvpMpTransitionRequest a,b;
    CK(!avp_mp_transition_queue(&q,AVP_LOCAL_MP_MAX_ACTORS,3,-1,-1,0));
    CK(!avp_mp_transition_queue(&q,0,0,-1,-1,0));
    CK(!avp_mp_transition_queue(&q,1,16,-1,-1,0));
    for(unsigned p=0;p<4;p++){
        CK(avp_mp_transition_queue(&q,p,(int16_t)(3+p),
                                   p?((int32_t)p<<16):-1,
                                   p?((int32_t)(p+1)<<16):-1,p==3));
        CK(avp_mp_transition_queue(&q,p,(int16_t)(3+p),
                                    p?((int32_t)p<<16):-1,
                                    p?((int32_t)(p+1)<<16):-1,p==3));
        CK(!avp_mp_transition_queue(&q,p,15,-1,-1,0));
    }
    CK(avp_mp_transition_peek(&q,3,&a));
    CK(a.valid&&a.target_level==6&&a.use_cocoon);
    CK(avp_mp_transition_commit(&q,2,&a));
    CK(a.target_level==5&&a.new_x==(2<<16)&&a.new_y==(3<<16));
    CK(!avp_mp_transition_peek(&q,2,&b));
    CK(avp_mp_transition_peek(&q,1,&b)&&b.target_level==4);
    CK(avp_mp_transition_peek(&q,0,&b)&&b.new_x==-1&&b.new_y==-1);
    CK(avp_mp_transition_queue(&q,2,15,-1,-1,0));
    CK(avp_mp_transition_commit(&q,2,&a)&&a.target_level==15);
    CK(!avp_mp_transition_commit(&q,2,&a));
    /* Two original airlock passengers can queue different fractional
     * arrival coordinates yet share the same destination. R13 split them
     * solely because both already owned a pending exit request. */
    {
        AvpMpTransitionQueue group={0};
        CK(avp_mp_transition_queue(&group,0,4,0x220080,0x310000,0));
        CK(avp_mp_transition_queue(&group,1,4,0x230080,0x320000,0));
        CK(avp_mp_transition_peek(&group,0,&a));
        CK(avp_mp_transition_peek(&group,1,&b));
        CK(avp_mp_transition_same_destination(&a,&b));
        CK(!avp_mp_transition_same_destination(&a,NULL));
        b.target_level=5;
        CK(!avp_mp_transition_same_destination(&a,&b));
        b.target_level=4;
        b.use_cocoon=1;
        CK(!avp_mp_transition_same_destination(&a,&b));
        b.use_cocoon=0;
        b.new_y=-1;
        CK(!avp_mp_transition_same_destination(&a,&b));
        /* A successful multi-passenger commit consumes BOTH intents. */
        CK(avp_mp_transition_commit(&group,0,&a));
        CK(avp_mp_transition_commit(&group,1,&b));
        CK(!avp_mp_transition_peek(&group,0,&a));
        CK(!avp_mp_transition_peek(&group,1,&b));
    }
    /* Four additional headless bot actors own separate pending exits. */
    for(unsigned p=4u;p<AVP_LOCAL_MP_MAX_ACTORS;p++){
        CK(avp_mp_transition_queue(&q,p,3,-1,-1,0));
        CK(avp_mp_transition_commit(&q,p,&a));
        CK(a.target_level==3);
    }
    puts("PASS: 8 per-actor original-level exits, no overwrite, coordinates and cocoon flag retained");
    return 0;
}
