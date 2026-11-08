#include "local_mp_compositor.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int surface_ok(unsigned width, unsigned height, unsigned pitch)
{
    /* Ensure the largest possible indexed element fits size_t. The caller
     * remains responsible for allocating the declared pixel array. */
    return width != 0 && height != 0 && pitch >= width &&
           ((size_t)height - 1u) <= (SIZE_MAX / sizeof(uint32_t)) / pitch &&
           (size_t)width - 1u <= SIZE_MAX / sizeof(uint32_t) -
                                 ((size_t)height - 1u) * pitch;
}

/* Host-only nearest-neighbor compositor.  Preserve the original exact
 * floor(x * (source_width-1) / (view_width-1)) sample positions, including
 * both edges.  Incremental quotient/remainder removes a 64-bit divide from
 * EVERY destination pixel.  Neither Jaguar rasterization nor gameplay is
 * touched; no allocations are introduced on the presentation path. */
static void blit_nearest32(const AvpLocalMpSourceFrame *f,
                           const AvpLocalMpRect *r,
                           const AvpLocalMpOutputFrame *output)
{
    const unsigned dx=r->width-1u;
    const unsigned q=dx ? (f->width-1u)/dx : 0u;
    const unsigned rem=dx ? (f->width-1u)%dx : 0u;
    unsigned prev_sy=~0u;
    const uint32_t *prev_out=NULL;
    unsigned samples[4096];
    const int mapped=r->width<=4096u && r->width!=1u && f->width!=r->width;
    if(mapped){
        unsigned sx=0u;
        uint64_t frac=0u;
        for(unsigned x=0;x<r->width;x++){
            samples[x]=sx;
            sx+=q;
            frac+=rem;
            if(frac>=dx){sx++;frac-=dx;}
        }
    }
    for(unsigned y=0;y<r->height;y++){
        /* Retain the original terminal bottom-line rule for one-row views. */
        const unsigned sy=r->height==1u ? f->height-1u :
            (unsigned)((uint64_t)y*(f->height-1u)/(r->height-1u));
        uint32_t *dst=output->pixels+(size_t)(r->y+y)*output->pitch+r->x;
        const uint32_t *src=f->pixels+(size_t)sy*f->pitch;
        /* Upscaled 1440p views usually sample the same Jaguar row several
         * times. Reuse its exact composed pixels instead of gathering every
         * horizontal sample again. Source frames are immutable during compose. */
        if(sy==prev_sy && prev_out){
            memcpy(dst,prev_out,(size_t)r->width*sizeof(*dst));
        }else if(f->width==r->width){
            memcpy(dst,src,(size_t)r->width*sizeof(*dst));
        }else if(r->width==1u){
            dst[0]=src[0];
        }else if(mapped){
            for(unsigned x=0;x<r->width;x++)dst[x]=src[samples[x]];
        }else{
            unsigned sx=0;
            uint64_t frac=0;
            for(unsigned x=0;x<r->width;x++){
                dst[x]=src[sx];
                sx+=q;
                frac+=rem;
                if(frac>=dx){sx++;frac-=dx;}
            }
        }
        prev_sy=sy;
        prev_out=dst;
    }
}

unsigned avp_local_mp_compose(const AvpLocalMpSetup *setup,
                              const AvpLocalMpSourceFrame frames[AVP_LOCAL_MP_MAX_PLAYERS],
                              const AvpLocalMpOutputFrame *output,
                              uint32_t clear_color)
{
    AvpLocalMpRect rect[AVP_LOCAL_MP_MAX_PLAYERS];
    unsigned p,n;
    if (!setup || !frames || !output || !output->pixels ||
        !surface_ok(output->width, output->height, output->pitch)) return 0;
    n=avp_local_mp_viewports(setup,output->width,output->height,rect);
    if(!n)return 0;
    /* Reject bad inputs without changing even a single destination pixel. */
    for(p=0;p<n;p++){
        if(!frames[p].pixels || frames[p].pixels==output->pixels ||
           !surface_ok(frames[p].width,frames[p].height,frames[p].pitch) ||
           !rect[p].width || !rect[p].height ||
           rect[p].x>output->width-rect[p].width ||
           rect[p].y>output->height-rect[p].height)return 0;
    }
    /* The 1/2/4-player grids and three-player wide-top layout cover every
     * pixel.  Clearing them first just doubles output memory traffic.
     * Only traditional three-player quadrants leave a black/clear region. */
    if(n==3u && setup->three_layout!=AVP_MP_THREE_WIDE_TOP){
        const unsigned x0=output->width/2u,y0=output->height/2u;
        for(unsigned y=y0;y<output->height;y++){
            uint32_t *dst=output->pixels+(size_t)y*output->pitch;
            for(unsigned x=x0;x<output->width;x++)dst[x]=clear_color;
        }
    }
    for(p=0;p<n;p++)blit_nearest32(&frames[p],&rect[p],output);
    return n;
}

/* Original terminal renders one full Jaguar picture. The live game has
 * already composed the four independently rendered HUD/world pictures into
 * output; replacing only this rectangle leaves all teammates visible. */
int avp_local_mp_overlay_player(const AvpLocalMpSetup *setup,unsigned owner,
                                const AvpLocalMpSourceFrame *frame,
                                const AvpLocalMpOutputFrame *output)
{
    AvpLocalMpRect rect[AVP_LOCAL_MP_MAX_PLAYERS];
    unsigned n;
    const AvpLocalMpRect *r;
    if(!setup||!frame||!output||!frame->pixels||!output->pixels||
       frame->pixels==output->pixels||
       !surface_ok(frame->width,frame->height,frame->pitch)||
       !surface_ok(output->width,output->height,output->pitch))return 0;
    n=avp_local_mp_viewports(setup,output->width,output->height,rect);
    if(!n||owner>=n)return 0;
    r=&rect[owner];
    if(!r->width||!r->height||r->x>output->width||
       r->y>output->height||r->width>output->width-r->x||
       r->height>output->height-r->y)return 0;
    blit_nearest32(frame,r,output);
    return 1;
}
