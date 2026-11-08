#include "jaguar/avp_gpu/host_distance.h"
#include <stdio.h>
static int near(double a,double b){double d=a-b;return d>=-1e-10&&d<=1e-10;}
int main(void)
{
    const double limits[]={8.0,16.0,24.0,32.0,128.0};
    const double factors[]={1.0,0.5,1.0/3.0,0.25};
    int checks=0;
    for(int draw=0;draw<AVP_HOST_DRAW_DISTANCE_CHOICES;draw++){
        if(!near(avp_host_ray_limit(draw),limits[draw]))return 1;
        if(!avp_host_draw_distance_label(draw)[0])return 2;
        checks+=2;
        for(int fog=0;fog<AVP_HOST_FOG_DISTANCE_CHOICES;fog++){
            for(int disable=0;disable<=1;disable++){
                if(!near(avp_host_fog_distance(4.0,fog),4.0*factors[fog]))return 3;
                if(!near(avp_host_fog_distance(12.0,fog),12.0*factors[fog]))return 4;
                if(avp_host_fog_shade_delta(-32,disable)!=(disable?0:-32))return 5;
                if(avp_host_fog_shade_delta(0,disable)!=0)return 6;
                checks+=4;
            }
        }
    }
    for(int i=0;i<AVP_HOST_DRAW_DISTANCE_CHOICES;i++){
        if(avp_host_distance_cycle(i,1,AVP_HOST_DRAW_DISTANCE_CHOICES)!=(i+1)%AVP_HOST_DRAW_DISTANCE_CHOICES)return 7;
        if(avp_host_distance_cycle(i,-1,AVP_HOST_DRAW_DISTANCE_CHOICES)!=(i+AVP_HOST_DRAW_DISTANCE_CHOICES-1)%AVP_HOST_DRAW_DISTANCE_CHOICES)return 8;
        checks+=2;
    }
    for(int i=0;i<AVP_HOST_FOG_DISTANCE_CHOICES;i++){
        if(avp_host_distance_cycle(i,1,AVP_HOST_FOG_DISTANCE_CHOICES)!=(i+1)%AVP_HOST_FOG_DISTANCE_CHOICES)return 9;
        if(avp_host_distance_cycle(i,-1,AVP_HOST_FOG_DISTANCE_CHOICES)!=(i+AVP_HOST_FOG_DISTANCE_CHOICES-1)%AVP_HOST_FOG_DISTANCE_CHOICES)return 10;
        if(!avp_host_fog_distance_label(i)[0])return 11;
        checks+=3;
    }
    if(!near(avp_host_ray_limit(-1),8.0)||!near(avp_host_ray_limit(999),8.0))return 12;
    if(!near(avp_host_fog_distance(8.0,-1),8.0)||!near(avp_host_fog_distance(8.0,999),8.0))return 13;
    checks+=4;
    if(!near(avp_host_ray_limit(1),16.0)||!near(avp_host_fog_distance(8.0,1),4.0))return 14;
    checks+=2;
    puts("host draw/fog distance modes: PASS");
    printf("%d assertions, 5 draw x 4 fog x 2 Disable Fog combinations\n",checks);
    return 0;
}
