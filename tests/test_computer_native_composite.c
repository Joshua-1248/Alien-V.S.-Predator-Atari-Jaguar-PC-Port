/* R131: exact R129/R130 terminal output vs. finished-row reuse.
 * Synthetic indexed artwork, palette and text; no proprietary ROM assets. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* R129 original per-destination-pixel terminal presentation. */
static void r129_computer_present(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    if(!state||!fb||!state->computer_active)return;
    for(unsigned y=0;y<fb->height;y++){
        unsigned sy=(unsigned)(((uint64_t)y*240u)/fb->height);if(sy>239u)sy=239u;
        for(unsigned x=0;x<fb->width;x++){
            unsigned sx=(unsigned)(((uint64_t)x*320u)/fb->width);if(sx>319u)sx=319u;
            uint32_t col=computer_bg();
            if(state->computer_palette_valid&&state->computer_bg_img&&
               state->computer_bg_img_size>=320u*240u)
                col=state->computer_palette[state->computer_bg_img[(size_t)sy*320u+sx]];
            col=computer_art_pixel(state,state->computer_obj1,(int)sx,(int)sy,col);
            col=computer_art_pixel(state,state->computer_obj2,(int)sx,(int)sy,col);
            {uint32_t fg=state->computer_pixels[(size_t)sy*320u+sx];
             if(fg&0xff000000u)col=fg&0x00ffffffu;}
            fb->pixels[(size_t)y*fb->width+x]=col;
        }
    }
}

/* R130 terminal-row presentation, copied unchanged as a parity oracle.
 * R131 must not change any pixel that this previously shipped function drew. */
static void r130_computer_present(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    uint32_t native[320];
    unsigned spans[321];
    unsigned previous_sy=240u;
    if(!state||!fb||!state->computer_active)return;
    if(!fb->width||!fb->height)return;
    for(unsigned sx=0;sx<=320u;sx++)
        spans[sx]=(unsigned)(((uint64_t)sx*fb->width+319u)/320u);
    for(unsigned y=0;y<fb->height;y++){
        unsigned sy=(unsigned)(((uint64_t)y*240u)/fb->height);
        uint32_t *row=fb->pixels+(size_t)y*fb->width;
        if(sy>239u)sy=239u;
        if(sy!=previous_sy){
            for(unsigned sx=0;sx<320u;sx++){
                uint32_t col;
                if(spans[sx]==spans[sx+1u])continue;
                col=computer_bg();
                if(state->computer_palette_valid&&state->computer_bg_img&&
                   state->computer_bg_img_size>=320u*240u)
                    col=state->computer_palette[state->computer_bg_img[(size_t)sy*320u+sx]];
                col=computer_art_pixel(state,state->computer_obj1,(int)sx,(int)sy,col);
                col=computer_art_pixel(state,state->computer_obj2,(int)sx,(int)sy,col);
                {uint32_t fg=state->computer_pixels[(size_t)sy*320u+sx];
                 if(fg&0xff000000u)col=fg&0x00ffffffu;}
                native[sx]=col;
            }
            previous_sy=sy;
        }
        for(unsigned sx=0;sx<320u;sx++)
            for(unsigned x=spans[sx];x<spans[sx+1u];x++)row[x]=native[sx];
    }
}

static void fill_state(AvpAvpGpuState *s,unsigned variant)
{
    const size_t size=320u*240u;
    s->computer_active=variant!=0u;
    s->computer_palette_valid=variant>=2u;
    s->computer_obj1=104;
    s->computer_obj2=128;
    s->computer_bg_img_size=variant==3u?size-1u:size;
    s->computer_overlay_size[0]=variant==4u?(size_t)192u*127u-1u:(size_t)192u*127u;
    s->computer_overlay_size[24]=(size_t)272u*178u;
    for(unsigned i=0;i<256u;i++)
        s->computer_palette[i]=(uint32_t)((i*4219u+variant*29u)^((i*67u)<<16));
    for(size_t i=0;i<size;i++){
        s->computer_bg_img[i]=(u8)((i*13u+(i/320u)*31u+variant)&255u);
        s->computer_pixels[i]=((i+variant)%19u==0u)?
            (0xff000000u|((u32)(i*134775813u)&0xffffffu)):
            (((i+variant)%37u==0u)?0x12020304u:0u);
    }
    for(size_t i=0;i<192u*127u;i++)
        s->computer_overlay_img[0][i]=(u8)((i%13u)?((i*5u+7u)&255u):0u);
    for(size_t i=0;i<272u*178u;i++)
        s->computer_overlay_img[24][i]=(u8)((i%5u)?((i*7u+variant)&255u):0u);
}

int main(void)
{
    const unsigned sizes[][2]={
        {0,0},{0,17},{17,0},{1,1},{2,1},{1,2},{7,9},{159,117},
        {319,239},{320,240},{320,228},{321,241},{426,300},
        {853,480},{1280,720},{1920,1080},{2560,1440},{3440,1440},
        {3840,2160}
    };
    AvpAvpGpuState *s=calloc(1,sizeof(*s));
    unsigned count=0;
    assert(s);
    s->computer_bg_img=malloc(320u*240u);
    s->computer_overlay_img[0]=malloc(192u*127u);
    s->computer_overlay_img[24]=malloc(272u*178u);
    assert(s->computer_bg_img&&s->computer_overlay_img[0]&&s->computer_overlay_img[24]);

    /* All used native columns must match the previous floor(x*320/width).
     * The empty spans at narrow widths must never write a destination. */
    for(unsigned width=1u;width<=4096u;width++){
        unsigned spans[321];
        for(unsigned i=0;i<=320u;i++)
            spans[i]=(unsigned)(((uint64_t)i*width+319u)/320u);
        assert(spans[0]==0u&&spans[320u]==width);
        for(unsigned i=0;i<320u;i++)
            for(unsigned x=spans[i];x<spans[i+1u];x++)
                assert((unsigned)(((uint64_t)x*320u)/width)==i);
    }

    for(unsigned v=0;v<5u;v++){
        fill_state(s,v);
        for(size_t si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
            unsigned w=sizes[si][0],h=sizes[si][1];
            size_t n=(size_t)w*h;
            u32 *a=malloc((n?n:1u)*sizeof(*a));
            u32 *b=malloc((n?n:1u)*sizeof(*b));
            u32 *c=malloc((n?n:1u)*sizeof(*c));
            AvpJaguarFramebuffer fb={w,h,NULL};
            assert(a&&b&&c);
            for(size_t i=0;i<n;i++)a[i]=b[i]=c[i]=(u32)(i*0x9e3779b9u+v*47u);
            fb.pixels=a;r129_computer_present(s,&fb);
            fb.pixels=b;r130_computer_present(s,&fb);
            fb.pixels=c;avp_gpu_computer_present(s,&fb);
            if(memcmp(a,b,n*sizeof(*a)) || memcmp(b,c,n*sizeof(*b))){
                fprintf(stderr,"R131 terminal mismatch variant %u at %ux%u\n",v,w,h);
                abort();
            }
            count++;
            free(a);free(b);free(c);
        }
    }
    printf("R129/R130/R131 terminal composition parity PASS (%u full frames)\n",count);
    free(s->computer_bg_img);
    free(s->computer_overlay_img[0]);
    free(s->computer_overlay_img[24]);
    free(s);
    return 0;
}
