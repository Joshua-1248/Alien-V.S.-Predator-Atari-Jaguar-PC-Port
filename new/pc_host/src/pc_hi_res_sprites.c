#include "pc/pc_hi_res_sprites.h"
#include "pc/pc_hi_res_textures.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Separate from the 1 GiB world-texture ceiling. Sparse, independent files
 * cannot increase the budget by supplying 16384 individually valid PNGs. */
#define SPRITE_BUDGET ((size_t)512u*1024u*1024u)
static unsigned be16(const uint8_t *p) {return ((unsigned)p[0]<<8u)|p[1];}
static int parent_dir(char *path,size_t cap,const char *exe)
{
    const char *a,*b,*s;size_t n;
    if(!path||!exe||!cap)return 0;
    a=strrchr(exe,'/');b=strrchr(exe,'\\');s=a;
    if(b&&(!s||b>s))s=b;
    if(!s)return 0;
    n=(size_t)(s-exe);
    if(n==0u)n=1u;
    if(n+1u>=cap)return 0;
    memcpy(path,exe,n);path[n]=0;return 1;
}
static void free_image(AvpGpuRgbImage *p)
{
    if(!p)return;
    free(p->rgb);free(p->cry16);free(p->alpha);free(p);
}
void avp_pc_hi_res_sprites_destroy(AvpPcHiResSprites *p)
{
    if(!p)return;
    for(unsigned id=0;id<AVP_HIRES_AAF_SHEETS;id++)
        for(unsigned f=0;f<AVP_HIRES_AAF_FRAMES;f++){
            free_image(p->images.props[id][f]);
            free_image(p->images.enemies[id][f]);
        }
    for(unsigned i=0;i<AVP_HIRES_FILES_COUNT;i++)free_image(p->images.weapons[i]);
    for(unsigned i=0;i<AVP_HIRES_HUD_COUNT;i++)free_image(p->images.hud[i]);
    memset(p,0,sizeof(*p));
}
static int accept_image(AvpPcHiResSprites *p,AvpGpuRgbImage **dest,const char *filename)
{
    AvpGpuRgbImage *im;size_t bytes;
    FILE *file=fopen(filename,"rb");
    if(!file)return 0; /* Missing files in a partial pack are expected. */
    fclose(file);
    im=(AvpGpuRgbImage*)calloc(1u,sizeof(*im));
    if(!im)return 0;
    if(!avp_pc_hi_res_read_png(filename,im)||!im->alpha){
        fprintf(stderr,"Hi-res sprite: rejected %s (requires valid 8-bit RGBA PNG)\n",filename);
        free_image(im);return 0;
    }
    bytes=(size_t)im->width*(size_t)im->height*4u;
    if(bytes>SPRITE_BUDGET-p->decoded_bytes){
        fprintf(stderr,"Hi-res sprite: skipped %s (512 MiB decoded sprite budget)\n",filename);
        free_image(im);return 0;
    }
    p->decoded_bytes+=bytes;
    *dest=im;
    return 1;
}
unsigned avp_pc_hi_res_sprites_load(AvpPcHiResSprites *p,const char *exe,
                                   const uint8_t *const sheets[63],const size_t sizes[63])
{
    char root[4096],name[4200];unsigned loaded=0;
    if(!p)return 0;
    avp_pc_hi_res_sprites_destroy(p);
    if(!sheets||!sizes||!parent_dir(root,sizeof(root),exe))return 0;
    for(unsigned sheet=0;sheet<AVP_HIRES_AAF_SHEETS;sheet++){
        unsigned frames=0;
        if(sheets[sheet]&&sizes[sheet]>=4u)frames=be16(sheets[sheet]);
        if(frames>AVP_HIRES_AAF_FRAMES)frames=AVP_HIRES_AAF_FRAMES;
        if((size_t)frames*4u+4u>sizes[sheet])continue;
        for(unsigned f=0;f<frames;f++){
            if(snprintf(name,sizeof(name),"%s/hi_res_props/%02u_%03u.png",root,sheet,f)<(int)sizeof(name))
                if(accept_image(p,&p->images.props[sheet][f],name)){
                    p->images.prop_count++;loaded++;
                }
            if(snprintf(name,sizeof(name),"%s/hi_res_enemies/%02u_%03u.png",root,sheet,f)<(int)sizeof(name))
                if(accept_image(p,&p->images.enemies[sheet][f],name)){
                    p->images.enemy_count++;loaded++;
                }
        }
    }
    for(unsigned file=AVP_HIRES_FILES_FIRST;file<AVP_HIRES_FILES_FIRST+AVP_HIRES_FILES_COUNT;file++){
        if(snprintf(name,sizeof(name),"%s/hi_res_weapons/%u.png",root,file)<(int)sizeof(name))
            if(accept_image(p,&p->images.weapons[file-AVP_HIRES_FILES_FIRST],name)){
                const AvpGpuRgbImage *im=p->images.weapons[file-AVP_HIRES_FILES_FIRST];
                fprintf(stderr,"Hi-res sprite: weapon FILES.DAT %u loaded (%dx%d RGBA)\n",
                        file,im->width,im->height);
                p->images.weapon_count++;loaded++;
            }
    }
    for(unsigned file=AVP_HIRES_HUD_FIRST;file<AVP_HIRES_HUD_FIRST+AVP_HIRES_HUD_COUNT;file++){
        if(snprintf(name,sizeof(name),"%s/hi_res_hud/%u.png",root,file)<(int)sizeof(name))
            if(accept_image(p,&p->images.hud[file-AVP_HIRES_HUD_FIRST],name)){
                p->images.hud_count++;loaded++;
            }
    }
    return loaded;
}
