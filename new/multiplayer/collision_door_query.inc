/* The AI's blocked-movement fallback may align with an open doorway, but
 * only when the door panel actually has enough clearance for that actor.
 * Inspect both authored wall faces: a solid/closed opposite face must not be
 * bypassed. Geometry and crossing still go through ordinary SafePos(). */
int avp_collision_open_door_ahead(s32 xpos,s32 ypos,int dx,int dy,u8 height)
{
    unsigned x=(u32)xpos>>16,y=(u32)ypos>>16;
    unsigned nx=x,ny=y,side,opposite;
    const u8 *wall1,*wall2;
    int door_present=0;
    if(!inside_cell(x,y) || ((dx!=0)+(dy!=0))!=1)return 0;
    if(dx>0){nx=x+1u;side=RIGHT_WALL;opposite=LEFT_WALL;}
    else if(dx<0){if(!x)return 0;nx=x-1u;side=LEFT_WALL;opposite=RIGHT_WALL;}
    else if(dy>0){ny=y+1u;side=BOTTOM_WALL;opposite=TOP_WALL;}
    else {if(!y)return 0;ny=y-1u;side=TOP_WALL;opposite=BOTTOM_WALL;}
    if(!inside_cell(nx,ny))return 0;
    wall1=cell_at(x,y)+side;
    wall2=cell_at(nx,ny)+opposite;
    if(fullbits[*wall1]&AVP_WB_DOOR)door_present=1;
    if(fullbits[*wall2]&AVP_WB_DOOR)door_present=1;
    if(!door_present)return 0;
    return check_wall(wall1,0x8000u,height)==0 &&
           check_wall(wall2,0x8000u,height)==0;
}

