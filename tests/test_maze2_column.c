#include "jaguar/avp_gpu/maze2_column.h"
#include <assert.h>

int main(void)
{
    int col=-1;
    /* MAZE2.GAS checks TOP with <, so an exact corner crosses RIGHT
       first. Duct tile corners often have different right/bottom wall codes. */
    assert(avp_gpu_maze2_right_wall_first(0.5,0.5));
    assert(avp_gpu_maze2_right_wall_first(0.25,0.5));
    assert(!avp_gpu_maze2_right_wall_first(0.75,0.5));
    /* No 4-screen-height saturation: the unbounded source scale is clipped
       after projection, never before source texture sampling. */
    assert(avp_gpu_maze2_project_wall_height(1.0,228u)==128);
    assert(avp_gpu_maze2_project_wall_height(0.5,228u)==256);
    assert(avp_gpu_maze2_project_wall_height(0.05,228u)==2560);
    assert(avp_gpu_maze2_project_wall_height(0.05,456u)==5120);
    assert(avp_gpu_maze2_project_wall_height(1.0,0u)==0);
    /* Retail common_plain rounds the two half-heights outwards, so
       an odd-height duct wall cannot shift one pixel at each seam. */
    {int top=-999,bottom=-999;
     assert(avp_gpu_maze2_centered_wall_bounds(228u,101,&top,&bottom));
     assert(top==63 && bottom==165);
     assert(avp_gpu_maze2_centered_wall_bounds(228u,100,&top,&bottom));
     assert(top==64 && bottom==164);
     assert(!avp_gpu_maze2_centered_wall_bounds(0u,100,&top,&bottom));
    }


    /* DDA must reproduce MAZE2's octant/tommy wall-coordinate flips.
       Authored wall-bit 7 reflection is applied only afterwards. */
    assert(avp_gpu_maze2_orient_fraction(0.25,1,+1)==0.25);
    assert(avp_gpu_maze2_orient_fraction(0.25,1,-1)==0.75);
    assert(avp_gpu_maze2_orient_fraction(0.25,0,-1)==0.25);
    assert(avp_gpu_maze2_orient_fraction(0.25,0,+1)==0.75);
    assert(avp_gpu_maze2_y_inc_from_z(0x4000u)==0x5000u);
    assert(avp_gpu_maze2_scale_edge(-32,0x10000u)==-32);
    assert(avp_gpu_maze2_scale_edge(32,0x10000u)==32);
    assert(avp_gpu_maze2_host_edge_y(10,101,-64)==10);
    assert(avp_gpu_maze2_host_edge_y(10,101,64)==111);
    assert(avp_gpu_maze2_host_edge_y(10,101,0)==60);
    assert(avp_gpu_maze2_host_source_row(0,128,0)==0);
    assert(avp_gpu_maze2_host_source_row(127,128,0)==127);
    assert(avp_gpu_maze2_singles_source_col(0x20,0,&col)&&col==0x20);
    assert(!avp_gpu_maze2_singles_source_col(0x60,0,&col));
    assert(avp_gpu_maze2_singles_source_col(0x20,1,&col)&&col==0x5f);
    assert(avp_gpu_maze2_centered_door_source_col(20,10,&col)&&col==30);
    assert(avp_gpu_maze2_centered_door_source_col(100,10,&col)&&col==90);
    assert(!avp_gpu_maze2_centered_door_source_col(60,10,&col));
    /* MAZE2.GAS: common_win stops scanning when the signed source
       window has collapsed (including the same-point pair). */
    {
        u8 pairs[256]={0};
        pairs[0]=(u8)(s8)-24; pairs[1]=(u8)(s8)35;
        assert(avp_gpu_maze2_window_pair_transmits(pairs,0));
        pairs[2]=(u8)(s8)8; pairs[3]=(u8)(s8)8;
        assert(!avp_gpu_maze2_window_pair_transmits(pairs,1));
        pairs[4]=(u8)(s8)30; pairs[5]=(u8)(s8)-30;
        assert(!avp_gpu_maze2_window_pair_transmits(pairs,2));
        assert(!avp_gpu_maze2_window_pair_transmits(pairs,128));
        assert(!avp_gpu_maze2_window_pair_transmits(NULL,0));
    }
    /* A deep wall may be rendered only in the intersection of every
       preceding retail window, not each window independently. */
    {
        int lo=0,hi=228;
        assert(avp_gpu_maze2_clip_intersect(&lo,&hi,48,170));
        assert(lo==48 && hi==170);
        assert(avp_gpu_maze2_clip_intersect(&lo,&hi,76,124));
        assert(lo==76 && hi==124);
        assert(!avp_gpu_maze2_clip_intersect(&lo,&hi,150,180));
        assert(lo==0 && hi==0);
        assert(!avp_gpu_maze2_clip_intersect(&lo,&hi,0,228));
        assert(!avp_gpu_maze2_clip_intersect(NULL,&hi,0,228));
    }
    /* MAZE2 csb/csp/csw doors: a window in a covered CSW half may
       transmit, but a closed panel is not an unrestricted corridor.
       A moving panel reads its shifted window column, not its old one. */
    {
        u8 pairs[256]={0};
        pairs[2u*30u]=(u8)(s8)-20;
        pairs[2u*30u+1u]=(u8)(s8)20;
        pairs[2u*60u]=(u8)(s8)-16;
        pairs[2u*60u+1u]=(u8)(s8)16;
        assert(avp_gpu_maze2_centered_visibility(6,10,20,pairs)==2);
        assert(avp_gpu_maze2_centered_visibility(6,10,20,NULL)==0);
        assert(avp_gpu_maze2_centered_visibility(6,10,21,pairs)==0);
        assert(avp_gpu_maze2_centered_visibility(6,10,60,pairs)==1);
        assert(avp_gpu_maze2_centered_visibility(6,0,20,pairs)==0);
        assert(avp_gpu_maze2_centered_visibility(4,10,60,pairs)==2);
        assert(avp_gpu_maze2_centered_visibility(4,10,60,NULL)==0);
        assert(avp_gpu_maze2_centered_visibility(4,10,20,pairs)==0);
        assert(avp_gpu_maze2_centered_visibility(5,10,60,NULL)==1);
        assert(avp_gpu_maze2_centered_visibility(5,0,60,NULL)==0);
        assert(avp_gpu_maze2_centered_visibility(3,10,60,pairs)==0);
    }
    /* Z_MAX (8) limits physical depth, not the number of adjacent wall
       crossings.  A 45-degree ray within the 90-degree view cone can cross
       16 grid lines; the old eight-slot list lost all subsequent window
       apertures, allowing farther walls to leak past them. */
    {
        const double dx=1.0,dy=1.0,limit=8.0;
        double next_x=0.5/dx,next_y=0.5/dy;
        unsigned crossings=0;
        int lo=0,hi=228;
        while(next_x<limit || next_y<limit){
            if(next_x<=next_y){++crossings;next_x+=1.0/dx;}
            else{++crossings;next_y+=1.0/dy;}
        }
        assert(crossings>8u);
        assert(crossings<AVP_MAZE2_MAX_RAY_PORTALS);
        for(unsigned i=0;i<crossings;i++){
            assert(avp_gpu_maze2_clip_intersect(&lo,&hi,10+(int)i,218-(int)i));
        }
        assert(lo==10+(int)crossings-1);
        assert(hi==218-(int)crossings+1);
    }
    /* Vertical doors: the aperture seen through a door is the same as its
       undrawn strip, not a generic unrestricted room-to-room portal. */
    {
        int lo=777,hi=888;
        assert(!avp_gpu_maze2_vertical_door_aperture(7,0,-64,64,&lo,&hi));
        assert(lo==777 && hi==888);
        assert(avp_gpu_maze2_vertical_door_aperture(7,24,-64,64,&lo,&hi));
        assert(lo==40 && hi==64);
        assert(!avp_gpu_maze2_vertical_door_aperture(8,0,-64,64,&lo,&hi));
        assert(avp_gpu_maze2_vertical_door_aperture(8,12,-64,64,&lo,&hi));
        assert(lo==-12 && hi==12);
        assert(!avp_gpu_maze2_vertical_door_aperture(9,0,-20,30,&lo,&hi));
        assert(avp_gpu_maze2_vertical_door_aperture(9,10,-20,30,&lo,&hi));
        assert(lo==20 && hi==30);
        assert(avp_gpu_maze2_vertical_door_aperture(10,99,-20,30,&lo,&hi));
        assert(lo==-20 && hi==30);
        assert(!avp_gpu_maze2_vertical_door_aperture(11,0,-20,30,&lo,&hi));
        assert(avp_gpu_maze2_vertical_door_aperture(11,8,-20,30,&lo,&hi));
        assert(lo==4 && hi==20);
        assert(!avp_gpu_maze2_vertical_door_aperture(11,8,21,30,&lo,&hi));
        assert(!avp_gpu_maze2_vertical_door_aperture(12,8,-64,64,&lo,&hi));
        assert(!avp_gpu_maze2_vertical_door_aperture(8,8,-64,64,NULL,&hi));
    }
    return 0;
}
