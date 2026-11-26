/* A successful native movement command is not evidence of retreat. */
#define main r497_original_main
#include "test_r497_tactical_travel.c"
#undef main
static void corridor(AvpMpBotState *s,AvpMpBotObservation *v){
    setup(s,v);memset(cells,0,sizeof cells);
    for(unsigned x=1;x<W-1;x++)cells[(5u*W+x)*8u+5u]=1;
    v->goal=AVP_MP_BOT_SUPPLY;
}
static int oscillate(AvpMpBotState *s,AvpMpBotObservation *v,unsigned ticks){
    for(unsigned i=0;i<ticks;i++){
        v->x=P(5)+(i%2?0u:4096u);
        if(tick(s,v))return 1;
    }
    return 0;
}
int main(void){
    AvpMpBotState s;AvpMpBotObservation v;
    unsigned passed=0;
    for(unsigned k=0;k<4;k++){
        corridor(&s,&v);const unsigned goals[]={AVP_MP_BOT_SUPPLY,AVP_MP_BOT_PATROL,AVP_MP_BOT_LIFT,AVP_MP_BOT_EXIT};v.goal=goals[k];
        assert(oscillate(&s,&v,18));assert(s.tactical_idle==0 && v.retreat_to_refuge && v.target_x>P(5));passed++;
    }
    corridor(&s,&v);for(unsigned i=0;i<20;i++){v.x+=2048u;assert(!tick(&s,&v));}passed++;
    corridor(&s,&v);for(unsigned i=0;i<20;i++){v.x+=128u;assert(!tick(&s,&v));}passed++;
    corridor(&s,&v);assert(!oscillate(&s,&v,8));v.melee_x-=32768u;assert(!oscillate(&s,&v,8));passed++;
    corridor(&s,&v);assert(!oscillate(&s,&v,8));v.tick+=2;assert(!oscillate(&s,&v,8));passed++;
    corridor(&s,&v);v.target_x=P(7);assert(!oscillate(&s,&v,30));passed++;
    corridor(&s,&v);v.deathmatch=0;assert(!oscillate(&s,&v,30));passed++;
    corridor(&s,&v);v.lift_wait=1;assert(!oscillate(&s,&v,30));passed++;
    printf("PASS %u/11 tactical progress and policy scenarios\n",passed);return passed!=11;
}
