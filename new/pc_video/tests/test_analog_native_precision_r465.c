/* Exercise the same projected player routine as the R464 velocity fixture,
 * feeding raw physical samples through the R465 response. */
#define main r464_fixture_main
#include "test_analog_velocity_r464.c"
#undef main
extern void avp_player_host_analog_aim(int,float,float,float);
extern void avp_player_host_aim_options(int,int);
extern void avp_player_host_live_heading(int,u32);
int main(void){
 unsigned n=0;
 for(int sp=0;sp<=8;sp+=4)for(unsigned h=0;h<65536;h+=2048)for(int dir=0;dir<4;dir++){
  init(sp,h,0);float fine=avp_pc_axis_precision_sample(6881,.2f,0,1);
  analog(dir,fine);ticks(80);int mag=metric(x_vel,y_vel);
  assert(mag>0 && mag<3*(int)max_speed/200);
  s32 vx=x_vel,vy=y_vel;
  init(sp,h,0);joy_cur=pad(sp,dir);analog(dir,fine);ticks(80);
  assert(x_vel==vx&&y_vel==vy);
  analog(dir,avp_pc_axis_precision_sample(32767,.2f,0,1));ticks(80);
  assert(metric(x_vel,y_vel)<=3*(int)max_speed && metric(x_vel,y_vel)>=3*(int)max_speed-4);
  n++;
 }
 for(int sp=0;sp<=8;sp+=4)for(int a=0;a<2;a++){
  init(sp,0,0);u32 want=(u32)llround((a?90.75:31.25)*(4294967296.0/360.0));
  centre_angle=want;avp_player_host_aim_options(1,0);avp_player_host_analog_aim(1,0,1,1);avp_player_host_live_heading(0,0);
  ticks(100);assert(centre_angle==want);
  avp_player_host_analog_aim(1,avp_pc_axis_precision_sample(6881,.2f,0,0),1,1);ticks(1);assert(centre_angle!=want);
  u32 stop=centre_angle;avp_player_host_analog_aim(1,0,1,1);ticks(100);assert(centre_angle==stop);
  avp_player_host_analog_aim(0,0,1,1);avp_player_host_aim_options(0,0);
 }
 printf("PASS %u projected MovePlayer precision cases; six fractional heading and release cases\n",n);return 0;
}
