/* Exercise the production selector; actual movement is tested separately. */
#define main r497_old_main
#include "test_r497_tactical_travel.c"
#undef main
static void acid(AvpMpBotObservation *v,uint32_t x,uint32_t y){
    v->acid[v->acid_count++]=(AvpMpBotAcid){x,y};
    avp_mp_bot_acid_risk_mark(risk,x,y);
}
static int wait_retarget(AvpMpBotState *s,AvpMpBotObservation *v){
    for(unsigned i=0;i<20;i++)if(tick(s,v))return 1;
    return 0;
}
int main(int argc,char **argv){
    (void)argv;
    AvpMpBotState s;AvpMpBotObservation v;
    unsigned pass=0,total=0;
    const unsigned goals[]={AVP_MP_BOT_LIFT,AVP_MP_BOT_EXIT,AVP_MP_BOT_SUPPLY,AVP_MP_BOT_PATROL};
    for(unsigned k=0;k<4;k++){
        setup(&s,&v);memset(cells,0,sizeof cells);
        for(unsigned x=1;x<W-1;x++)cells[(5u*W+x)*8u+5u]=1;
        v.goal=goals[k];acid(&v,P(4),P(5));
        const int result=wait_retarget(&s,&v) && v.target_x>P(5) && v.retreat_to_refuge;
        printf("near-acid goal=%u retreat=%d\n",goals[k],result);pass+=result;total++;
        if(result){
            uint32_t bx=v.target_x,by=v.target_y;
            v.goal=goals[k];v.target_x=P(2);v.target_y=P(5);
            assert(tick(&s,&v) && v.target_x==bx && v.target_y==by);
            /* A newly appeared puddle on the chosen refuge revokes commitment. */
            acid(&v,bx,by);v.goal=goals[k];v.target_x=P(2);v.target_y=P(5);
            assert(!tick(&s,&v) && !s.tactical_until);
        }
    }
    setup(&s,&v);acid(&v,v.x,v.y);assert(!wait_retarget(&s,&v)); /* existing contact escape */
    setup(&s,&v);acid(&v,P(4),P(5));v.target_x=P(7);assert(!wait_retarget(&s,&v)); /* safe goal */
    setup(&s,&v);v.target_x=P(7);acid(&v,P(6),P(5));
    /* Make the ordinary selected path pass through the puddle, while
     * the exact-coordinate refuge check rejects it even without a risk grid. */
    v.map.risk=NULL;
    int result=wait_retarget(&s,&v) && v.retreat_to_refuge && v.target_y!=P(5);
    printf("acid on otherwise-away route=%d\n",result);pass+=result;total++;
    setup(&s,&v);acid(&v,P(4),P(5));for(unsigned i=0;i<4096;i++)risk[i]=3;assert(!wait_retarget(&s,&v));
    setup(&s,&v);acid(&v,P(4),P(5));s.acid_commit_ticks=10;assert(!wait_retarget(&s,&v));
    setup(&s,&v);acid(&v,P(4),P(5));v.deathmatch=0;assert(!wait_retarget(&s,&v));
    setup(&s,&v);acid(&v,P(4),P(5));v.airlock_pressure=1;assert(!wait_retarget(&s,&v));
    printf("%u/%u new tactical outcomes; 6 policy exclusions passed\n",pass,total);
    return argc==2?0:pass!=total;
}
