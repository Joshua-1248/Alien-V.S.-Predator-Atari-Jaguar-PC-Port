/* Actual projected MovePlayer, with the same retail sine/collision fixture as R464. */
#define main r464_fixture_main
#include "test_analog_velocity_r464.c"
#undef main
#include "pc/pc_double_movement.h"
extern s16 acc_shift;
extern void avp_player_host_movement_speed(int step,int dm_fast);
static void start(int sp,unsigned h,int step,int dm){
    init(sp,h,step);
    if(dm){max_speed*=2;--acc_shift;}
    avp_player_host_movement_speed(step,dm);
}
int main(void){
    unsigned cases=0;double worst=0.0;
    for(int step=0;step<4;step++){
        assert(avp_pc_move_menu_step(step,1)==(step+1)%4);
        assert(avp_pc_move_menu_step(step,-1)==(step+3)%4);
    }
    assert(avp_pc_move_step(-1)==0&&avp_pc_move_step(4)==3);
    for(int sp=0;sp<=8;sp+=4)for(unsigned h=0;h<65536;h+=2048)for(int dir=0;dir<4;dir++){
        init(sp,h,0);joy_cur=pad(sp,dir);ticks(1);int first=metric(x_vel,y_vel);ticks(80);
        int64_t base=0;for(int i=0;i<100;i++){ticks(1);base+=metric(x_vel,y_vel);}unsigned native_max=max_speed;
        assert(base>0);
        for(int dm=0;dm<2;dm++)for(int step=0;step<4;step++){
            int n=avp_pc_move_effective_step(step,dm)+1;
            int cap=3*(int)native_max*n;
            start(sp,h,step,dm);joy_cur=pad(sp,dir);ticks(1);
            assert(abs(metric(x_vel,y_vel)-first*n)<=20);ticks(80);
            int64_t actual=0;for(int i=0;i<100;i++){ticks(1);assert(metric(x_vel,y_vel)<=cap);actual+=metric(x_vel,y_vel);}
            double deviation=fabs((double)actual/(base*n)-1.0);if(deviation>worst)worst=deviation;
            assert(deviation<0.15);
            start(sp,h,step,dm);analog(dir,1);ticks(80);
            s32 vx=x_vel,vy=y_vel;
            assert(metric(vx,vy)<=cap&&metric(vx,vy)>=cap-4);
            start(sp,h,step,dm);joy_cur=pad(sp,dir);analog(dir,1);ticks(80);
            assert(x_vel==vx&&y_vel==vy);
            start(sp,h,step,dm);analog(dir,0.25f);ticks(80);
            assert(abs(metric(x_vel,y_vel)-cap/4)<=4);
            avp_player_host_double_movement(0); /* next actor/reset */
            cases++;
        }
    }
    for(int sp=0;sp<=8;sp+=4){
        init(sp,0,0);const unsigned cap=max_speed;
        for(int step=0;step<4;step++){
            start(sp,0,step,0);analog(0,1);ticks(80);assert(x_vel==(s32)(cap*(step+1)));
            printf("species=%d multiplier=%dx terminal=%d\n",sp,step+1,x_vel);
        }
        start(sp,0,3,0);analog(0,1);ticks(80);
        avp_player_host_double_movement(0);ticks(80);assert(x_vel==(s32)cap);
        start(sp,0,3,0);lift_key=1;analog(0,1);ticks(80);assert(!x_vel&&!y_vel);
    }
    printf("Worst digital sustained mean deviation from scaled native speed: %.3f%%\n",100*worst);
    printf("PASS %u actual MovePlayer cases: 3 species, 32 headings, 4 directions, all four settings, with and without DM fast rule, digital/analog/duplicate/partial\n",cases);
    return 0;
}
