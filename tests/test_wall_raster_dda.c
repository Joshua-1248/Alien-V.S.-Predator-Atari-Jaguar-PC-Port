#include "jaguar/avp_gpu/wall_raster_dda.h"
#include <assert.h>
#include <stdio.h>
/* Exhaustive signed start offsets plus varied near/far projected heights.
 * Compare against the original host source coordinate formula at every row. */
static int original(int screen_row,int wall_h){
    int64_t num=((int64_t)screen_row*2+1)*128,den=(int64_t)wall_h*2;
    return (int)(num/den);
}
int main(void){
    for(int h=128;h<=32768;h=(h*11)/10+1){
        for(int start=-1600;start<=1600;start+=31){
            AvpWallRowDda d=avp_wall_row_dda_begin(start,h);
            for(int step=0;step<3200;step++){
                int want=original(start+step,h);
                if(avp_wall_row_dda_value(&d)!=want){
                    fprintf(stderr,"DDA mismatch h=%d start=%d step=%d got=%d wanted=%d\n",h,start,step,avp_wall_row_dda_value(&d),want);
                    return 1;
                }
                avp_wall_row_dda_advance(&d);
            }
        }
    }
    puts("wall raster DDA matches original signed source rows");
    return 0;
}
