/* R41: Exercise the real retail wall/floor fog shading formulas with each
 * host distance setting.  R39 passed isolated helper tests but collapsed all
 * nonzero renderer modes to 1.  The source-route regression complements this. */
#include "jaguar/avp_gpu/projection.hpp"
#include "jaguar/avp_gpu/host_distance.h"
#include <cstdio>
int main()
{
    const double distances[]={8.0,16.0,24.0,32.0,128.0};
    int checks=0;
    for(int i=0;i<AVP_HOST_DRAW_DISTANCE_CHOICES;i++){
        if(avp_host_ray_limit(i)!=distances[i])return 1;
        checks++;
    }
    /* All four fog choices must produce distinct real CRY shade deltas
     * at an intermediate distance on both wall and flat surfaces. */
    for(int d=4;d<=8;d+=2){
        int prev_wall=-1000,prev_flat=-1000;
        for(int mode=0;mode<AVP_HOST_FOG_DISTANCE_CHOICES;mode++){
            double effective=avp_host_fog_distance((double)d,mode);
            int wall=avp::retail_wall_intensity_delta(effective);
            int flat=avp::retail_flat_intensity_delta(effective);
            if(mode && (wall<=prev_wall || flat<=prev_flat))return 2;
            if(avp_host_fog_shade_delta(wall,1)!=0 ||
               avp_host_fog_shade_delta(flat,1)!=0)return 3;
            if(avp_host_fog_shade_delta(wall,0)!=wall ||
               avp_host_fog_shade_delta(flat,0)!=flat)return 4;
            prev_wall=wall;prev_flat=flat;
            checks+=4;
        }
    }
    std::printf("PASS real CRY wall/flat fog modes, draw limits, Disable Fog: %d checks\n",checks);
    return 0;
}
