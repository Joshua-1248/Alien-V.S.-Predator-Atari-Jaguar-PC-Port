    /* An attacked jammed door can never be shut again, including during its
     * opening animation. The access code always remains JAMMED. */
    if(!force_jammed && host_jammed_forced_ever){
        int opened_jam=jammed_door(door) &&
            (*door || door1.ptr==door || door2.ptr==door);
        if(!opened_jam && (flags&D2_SPECIAL)==D2_SPECIAL){
            /* Double-door panels may address the opposite half from the
             * other side. That half must not be allowed to close either. */
            ptrdiff_t other_off=(door-maze_data)+
                ((panel&0x80u)?-maze_move((side+1u)&3u):maze_move((side+1u)&3u));
            if(other_off>=0 && (size_t)other_off<(size_t)maze_w*maze_h*CELL){
                u8 *other=maze_data+other_off;
                opened_jam=jammed_door(other) &&
                    (*other || door1.ptr==other || door2.ptr==other);
            }
        }
        if(opened_jam){avp_hudmsg_queue(avp_msg_jammed);return 1;}
    }
