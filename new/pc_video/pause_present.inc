/* PC-only pause presentation. The original DoPause and object events remain
 * authoritative; SaveOvers captures the actual displayed resolution. */
static void avp_new_pause_event(AvpPcVideo *v,unsigned event,u32 a,u32 b,u32 c)
{
    AvpNewPauseView *p=&v->pause_view;
    AvpJaguarFramebuffer *target=&v->fb;
    if(event==0x5053564fu){ /* SaveOvers */
        p->hires=v->presenting_world_hires && v->world_fb.pixels;
        target=p->hires?&v->world_fb:&v->fb;
        p->width=target->width;p->height=target->height;p->brightness=hud_bright;
    }else if(p->hires && v->world_fb.pixels){
        target=&v->world_fb;
    }
    avp_jaguar_pause_op_event(&v->gpu.pause_op,event,a,b,c,target->pixels,target->width,target->height);
    if(event==0x5052564fu||event==0x534d4c30u||event==0x524d4c30u){
        p->hires=0;p->valid=0;
    }
}
void avp_pc_video_present_pause(AvpPcVideo *v)
{
    AvpJaguarFramebuffer *target;
    AvpNewPauseView *p;
    AvpJaguarPauseOpState *op;
    if(!v||!avp_jaguar_pause_op_active(&v->gpu.pause_op))return;
    p=&v->pause_view;op=&v->gpu.pause_op;
    target=&v->fb;
    if(p->hires && p->width && p->height){
        if(v->world_fb.width!=p->width || v->world_fb.height!=p->height){
            avp_jaguar_framebuffer_destroy(&v->world_fb);
            (void)avp_jaguar_framebuffer_init(&v->world_fb,p->width,p->height);
        }
        if(v->world_fb.pixels)target=&v->world_fb;
    }
    /* The original slider changes hud_bright; redraw at the frozen camera
     * and VBL only when its value changes, without advancing simulation. */
    if(p->valid && p->maze && p->brightness!=hud_bright && op->base &&
       op->base_count==(size_t)target->width*target->height){
        avp_gpu_render_maze(&v->gpu,target,p->maze,p->mw,p->mh,p->x,p->y,p->angle,
            p->rom_ok,p->level,p->vbl,p->smooth,p->debug);
        memcpy(op->base,target->pixels,op->base_count*sizeof(*op->base));
        p->brightness=hud_bright;
    }
    if(op->base && op->base_count!=(size_t)target->width*target->height &&
       op->base_count==(size_t)p->width*p->height && p->width && p->height){
        /* Allocation failure: preserve the correct pause image at native size. */
        avp_pc_scale_nearest32(op->base,p->width,p->height,target->pixels,target->width,target->height);
    }
    v->presenting_world_hires=target==&v->world_fb;
    avp_jaguar_pause_op_prepare_frame(op,target->pixels,target->width,target->height);
    present_framebuffer(v,0,1);
    XFlush(v->dpy);
}
