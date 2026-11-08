/* R131 wall-column reference versus R132 same-texel reuse.  This is a
 * production-routine pixel/depth comparison using synthetic images only. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Verbatim R131 wall routine, renamed only for differential testing. */
static void r131_draw_wall_seg_shift(AvpJaguarFramebuffer *fb,const PcRayHit *hit,unsigned sx,int y0,int y1,int base_y0,
                                int wall_h,const AvpGpuRgbImage *t,int source_tx,
                                int source_y_shift,int shade,AvpMaze2Occlusion *occlusion,float depth)
{
    int yy;
    if(!t||!t->rgb||wall_h<=0||!hit)return;
    if(y0<hit->clip_lo)y0=hit->clip_lo;
    if(y1>hit->clip_hi)y1=hit->clip_hi;
    if(y0<0)y0=0;
    if(y1>(int)fb->height)y1=(int)fb->height;
    if(y0>=y1)return;
    if(source_tx<0)source_tx=0;
    if(source_tx>127)source_tx=127;
    source_tx=source_tx*t->width/128;
    if(source_tx>=t->width)source_tx=t->width-1;
    if(wall_h>=128){
        AvpWallRowDda rows=avp_wall_row_dda_begin(y0-base_y0,wall_h);
        const int have_depth=occlusion && occlusion->depth &&
                             occlusion->width==fb->width &&
                             occlusion->height==fb->height;
        for(yy=y0;yy<y1;yy++){
            const size_t pixel_index=(size_t)yy*fb->width+sx;
            int source_row,ty;
            if(have_depth){
                float *owner=&occlusion->depth[pixel_index];
                if(*owner<=depth){avp_wall_row_dda_advance(&rows);continue;}
                *owner=depth;
            }
            source_row=avp_wall_row_dda_value(&rows)+source_y_shift;
            if(source_row<0)source_row=0;
            if(source_row>127)source_row=127;
            ty=source_row*t->height/128;
            if(ty<0)ty=0;
            if(ty>=t->height)ty=t->height-1;
            fb->pixels[pixel_index]=sample_tex_shaded(t,source_tx,ty,shade);
            avp_wall_row_dda_advance(&rows);
        }
    }else{
        for(yy=y0;yy<y1;yy++){
            const size_t pixel_index=(size_t)yy*fb->width+sx;
            int source_row,ty;
            if(occlusion && occlusion->depth &&
               occlusion->width==fb->width && occlusion->height==fb->height){
                float *owner=&occlusion->depth[pixel_index];
                if(*owner<=depth)continue;
                *owner=depth;
            }
            source_row=avp_gpu_maze2_host_source_row(yy-base_y0,wall_h,source_y_shift);
            ty=source_row*t->height/128;
            if(ty<0)ty=0;
            if(ty>=t->height)ty=t->height-1;
            fb->pixels[pixel_index]=sample_tex_shaded(t,source_tx,ty,shade);
        }
    }
}

static void init_image(AvpGpuRgbImage *t,unsigned dim,int have_cry)
{
    size_t count=(size_t)dim*dim;
    t->width=(int)dim;t->height=(int)dim;
    t->rgb=(u8*)malloc(count*3u);
    t->cry16=have_cry?(u16*)malloc(count*sizeof(u16)):NULL;
    assert(t->rgb&&(!have_cry||t->cry16));
    for(size_t i=0;i<count;i++){
        t->rgb[i*3u]=(u8)(i*11u);
        t->rgb[i*3u+1u]=(u8)(i*29u+5u);
        t->rgb[i*3u+2u]=(u8)(i*7u+17u);
        if(have_cry)t->cry16[i]=(u16)(i*109u+(i>>2)*37u);
    }
}

int main(void)
{
    const unsigned sizes[][2]={{320u,228u},{853u,480u},{1920u,1080u},{3840u,2160u}};
    const int heights[]={16,47,127,128,129,251,511,1024,4096,16384};
    const int shifts[]={-39,0,37};
    const int shades[]={-130,-11,0,47,260};
    unsigned long long cases=0;
    for(unsigned i=0;i<65536u;i++)k_cry_rgb[i]=(u32)((i*0x9e3779b1u)^0x00a5965au);
    for(unsigned sz=0;sz<sizeof(sizes)/sizeof(sizes[0]);sz++){
        const unsigned w=sizes[sz][0],h=sizes[sz][1];
        const size_t n=(size_t)w*h;
        AvpJaguarFramebuffer before={w,h,NULL},after={w,h,NULL};
        AvpMaze2Occlusion old_oc={w,h,NULL},new_oc={w,h,NULL};
        before.pixels=(u32*)malloc(n*sizeof(u32));
        after.pixels=(u32*)malloc(n*sizeof(u32));
        old_oc.depth=(float*)malloc(n*sizeof(float));
        new_oc.depth=(float*)malloc(n*sizeof(float));
        assert(before.pixels&&after.pixels&&old_oc.depth&&new_oc.depth);
        for(size_t k=0;k<n;k++){
            before.pixels[k]=(u32)(k*2654435761u+sz*17u);
            old_oc.depth[k]=(k%7u<3u)?0.25f:12.0f;
        }
        memcpy(after.pixels,before.pixels,n*sizeof(u32));
        memcpy(new_oc.depth,old_oc.depth,n*sizeof(float));
        for(unsigned texture=0;texture<4u;texture++){
            AvpGpuRgbImage tex={0};
            init_image(&tex,(texture&1u)?128u:16u,(texture&2u)!=0);
            for(unsigned a=0;a<sizeof(heights)/sizeof(heights[0]);a++)
            for(unsigned b=0;b<sizeof(shifts)/sizeof(shifts[0]);b++)
            for(unsigned c=0;c<sizeof(shades)/sizeof(shades[0]);c++)
            for(unsigned oc=0;oc<3u;oc++){
                unsigned col=(unsigned)((a*97u+b*29u+c*13u+oc*7u+texture*31u)%w);
                int base_y0=(a&1u)?-((int)h/2):(int)h/3;
                int y0=(a%3u==0u)?-45:(int)h/12;
                int y1=(a%4u==0u)?(int)h+55:(int)h*3/4;
                PcRayHit hit={0};
                AvpMaze2Occlusion *op=NULL,*np=NULL;
                if(oc==1u){op=&old_oc;np=&new_oc;}
                if(oc==2u){
                    /* Wrong dimensions must disable depth checks in both. */
                    old_oc.width=w+1u;new_oc.width=w+1u;
                    op=&old_oc;np=&new_oc;
                }
                hit.clip_lo=(c&1u)?(int)h/3:-10;
                hit.clip_hi=(c&2u)?(int)h*2/3:(int)h+20;
                r131_draw_wall_seg_shift(&before,&hit,col,y0,y1,base_y0,
                    heights[a],&tex,(int)((col*17u)%160u)-13,
                    shifts[b],shades[c],op,(a&1u)?0.9f:5.5f);
                draw_wall_seg_shift(&after,&hit,col,y0,y1,base_y0,
                    heights[a],&tex,(int)((col*17u)%160u)-13,
                    shifts[b],shades[c],np,(a&1u)?0.9f:5.5f);
                old_oc.width=w;new_oc.width=w;
                cases++;
            }
            free(tex.rgb);free(tex.cry16);
        }
        if(memcmp(before.pixels,after.pixels,n*sizeof(u32))!=0){
            fprintf(stderr,"wall pixel mismatch %ux%u\n",w,h);return 1;
        }
        if(memcmp(old_oc.depth,new_oc.depth,n*sizeof(float))!=0){
            fprintf(stderr,"wall occlusion mismatch %ux%u\n",w,h);return 1;
        }
        free(before.pixels);free(after.pixels);
        free(old_oc.depth);free(new_oc.depth);
    }
    printf("wall texel reuse pixel/occlusion parity PASS (%llu segments)\n",cases);
    return 0;
}
