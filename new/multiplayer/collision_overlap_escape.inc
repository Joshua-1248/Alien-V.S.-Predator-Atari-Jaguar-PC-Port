/* Multiplayer's enemy update can move an active AMP INTO a player's
 * position between player movement ticks. Retail AMPCollisions correctly
 * rejects a NEW penetration, but rejecting even a motion which reduces the
 * ALREADY EXISTING overlap pins the actor until the enemy moves again.
 *
 * This opt-in PC-only escape follows the original collmap lookup, live AMP
 * filter, creature widths and axis-aligned squares above. It does not move
 * the player or change original collision/damage physics; only a candidate
 * that strictly reduces EVERY already-overlapping AMP footprint is allowed.
 * New contacts, non-reducing moves, static props and stale slots are not
 * excused. The host invokes this ONLY in a bound multiplayer actor tick. */
int avp_collision_mp_amp_escape(s32 from_x,s32 from_y,s32 to_x,s32 to_y)
{
    int cx=(int)((u32)to_x>>16),cy=(int)((u32)to_y>>16);
    int reduced=0;
    if(from_x==to_x && from_y==to_y)return 0;
    for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++){
        int x=cx+ox,y=cy+oy;
        u16 n; AvpAmp *a; s32 w,oldx,oldy,newx,newy;
        u64 old_area,new_area;
        if(x<0||y<0||x>=AVP_AMP_GRID_W||y>=AVP_AMP_GRID_H)continue;
        n=collmap[y*AVP_AMP_GRID_W+x];
        if(!n||n>AVP_NUM_AMPS)continue;
        a=&amp_data[n-1u];
        if((!a->mode&&!a->host_static)||
           ((u32)a->xpos>>16)!=(unsigned)x||
           ((u32)a->ypos>>16)!=(unsigned)y||a->creature>=40u)continue;
        w=coll_widths[a->creature];if(!w)continue;
        newx=wrapped_abs_delta(a->xpos,to_x);
        newy=wrapped_abs_delta(a->ypos,to_y);
        if(newx>=w || newy>=w)continue;
        /* Static authored furniture cannot invade an actor; do not permit
         * an overlap escape to phase through tables and other props. */
        if(a->host_static)return 0;
        oldx=wrapped_abs_delta(a->xpos,from_x);
        oldy=wrapped_abs_delta(a->ypos,from_y);
        if(oldx>=w || oldy>=w)return 0; /* newly-entered enemy */
        old_area=(u64)(w-oldx)*(u64)(w-oldy);
        new_area=(u64)(w-newx)*(u64)(w-newy);
        if(new_area>=old_area)return 0;
        reduced=1;
    }
    return reduced;
}

