#include "pc/pc_prototype_textures.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/notice_composite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void avp_pc_prototype_textures_destroy(AvpPcPrototypeTextures *swap)
{
    unsigned i;
    if(!swap)return;
    for(i=0u;i<AVP_PC_PROTOTYPE_SLOTS;i++){
        free(swap->retail_rgb[i]); free(swap->retail_cry[i]);
    }
    memset(swap,0,sizeof(*swap));
}
void avp_pc_prototype_textures_capture(AvpPcPrototypeTextures *swap,const AvpAvpGpuState *state)
{
    unsigned i;
    if(!swap)return;
    avp_pc_prototype_textures_destroy(swap);
    if(!state)return;
    for(i=0;i<AVP_PC_PROTOTYPE_SLOTS;i++){
        const AvpGpuRgbImage *im=&state->panel_tex[i];
        if(!im->rgb || im->width!=128 || im->height!=128)continue;
        swap->retail_rgb[i]=(uint8_t*)malloc(AVP_PC_PROTOTYPE_RGB_BYTES);
        if(!swap->retail_rgb[i])continue;
        memcpy(swap->retail_rgb[i],im->rgb,AVP_PC_PROTOTYPE_RGB_BYTES);
        if(im->cry16){
            swap->retail_cry[i]=(uint16_t*)malloc(AVP_PC_PROTOTYPE_CRY_BYTES);
            if(swap->retail_cry[i])
                memcpy(swap->retail_cry[i],im->cry16,AVP_PC_PROTOTYPE_CRY_BYTES);
        }
    }
}
/* S106: a self-checking user-local raw RGB pack. Previous S105 packs use
 * unframed .rgb files and remain readable. Corrupt framed panels never
 * fall through to a legacy copy, avoiding an unexpected stale override. */
static uint32_t prototype_crc32(const uint8_t *bytes,size_t length)
{
    uint32_t crc=0xffffffffu;
    size_t i;unsigned bit;
    for(i=0;i<length;i++){
        crc^=(uint32_t)bytes[i];
        for(bit=0u;bit<8u;bit++)
            crc=(crc>>1u)^((crc&1u)?0xedb88320u:0u);
    }
    return crc^0xffffffffu;
}
static uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
          ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static int read_panel(const char *directory,unsigned id,uint8_t rgb[AVP_PC_PROTOTYPE_RGB_BYTES])
{
    static const uint8_t magic[8]={'A','V','P','T','E','X','1',0};
    char path[1024]; FILE *f; int c;size_t n;
    uint8_t header[16];
    if(!directory||!*directory||strlen(directory)>900u)return 0;
    if(snprintf(path,sizeof(path),"%s/%02u.prgb",directory,id)>=(int)sizeof(path))return 0;
    f=fopen(path,"rb");
    if(f){
        n=fread(header,1u,sizeof(header),f);
        if(n!=sizeof(header)||memcmp(header,magic,sizeof(magic))!=0||
           read_u32le(header+8u)!=AVP_PC_PROTOTYPE_RGB_BYTES){fclose(f);return 0;}
        n=fread(rgb,1u,AVP_PC_PROTOTYPE_RGB_BYTES,f);
        c=fgetc(f);fclose(f);
        return n==AVP_PC_PROTOTYPE_RGB_BYTES&&c==EOF&&
               prototype_crc32(rgb,AVP_PC_PROTOTYPE_RGB_BYTES)==read_u32le(header+12u);
    }
    /* S105 compatibility: no checksum in its legacy .rgb container. */
    if(snprintf(path,sizeof(path),"%s/%02u.rgb",directory,id)>=(int)sizeof(path))return 0;
    f=fopen(path,"rb");if(!f)return 0;
    n=fread(rgb,1u,AVP_PC_PROTOTYPE_RGB_BYTES,f);
    c=fgetc(f);fclose(f);
    return n==AVP_PC_PROTOTYPE_RGB_BYTES&&c==EOF;
}
/* No asset pointers change: co-op viewers borrow all original buffers.
 * Notice composites borrow source FILES 16 and its existing overlays. */
static void update_notice(AvpAvpGpuState *s)
{
    static const unsigned overlay_ids[13]={62u,57u,61u,59u,58u,60u,56u,64u,63u,67u,66u,68u,65u};
    unsigned ni;
    if(!s||!s->panel_tex[16].rgb)return;
    for(ni=0;ni<18u;ni++){
        unsigned type=ni<13u?ni:0u;
        const AvpGpuRgbImage *num=ni>=13u?&s->panel_tex[69u+ni-13u]:NULL;
        AvpGpuRgbImage temp={0}; AvpGpuRgbImage *dest=&s->notice_tex[ni];
        if(!dest->rgb||dest->width!=128||dest->height!=128)continue;
        if(avp_gpu_notice_composite(&s->panel_tex[16u],&s->panel_tex[overlay_ids[type]],num,
                                     type==0u?-16:0,&temp)){
            memcpy(dest->rgb,temp.rgb,AVP_PC_PROTOTYPE_RGB_BYTES);
            if(dest->cry16&&temp.cry16)memcpy(dest->cry16,temp.cry16,AVP_PC_PROTOTYPE_CRY_BYTES);
            avp_gpu_free_rgb_image(&temp);
        }
    }
}
unsigned avp_pc_prototype_textures_apply(AvpPcPrototypeTextures *swap,AvpAvpGpuState *state,
                                         const char *directory,int enabled)
{
    uint8_t *pixels;
    unsigned i,applied=0u;
    if(!swap||!state)return 0;
    pixels=(uint8_t*)malloc(AVP_PC_PROTOTYPE_RGB_BYTES);
    for(i=0;i<AVP_PC_PROTOTYPE_SLOTS;i++){
        AvpGpuRgbImage *im=&state->panel_tex[i];
        if(!swap->retail_rgb[i]||!im->rgb||im->width!=128||im->height!=128)continue;
        /* Always restore first, even when the replacement is missing/invalid. */
        memcpy(im->rgb,swap->retail_rgb[i],AVP_PC_PROTOTYPE_RGB_BYTES);
        if(im->cry16&&swap->retail_cry[i])
            memcpy(im->cry16,swap->retail_cry[i],AVP_PC_PROTOTYPE_CRY_BYTES);
        if(!enabled||!pixels||!read_panel(directory,i,pixels))continue;
        /* Do not partially apply a source when the live renderer's native
         * CRY image cannot be regenerated. */
        if(im->cry16 && state->qconv && state->qconv_size>=131072u){
            AvpGpuRgbImage temp={128,128,pixels,NULL};
            if(!avp_gpu_build_cry_image(&temp,state->qconv,state->qconv_size))continue;
            memcpy(im->cry16,temp.cry16,AVP_PC_PROTOTYPE_CRY_BYTES);
            free(temp.cry16);
        }else if(im->cry16)continue;
        memcpy(im->rgb,pixels,AVP_PC_PROTOTYPE_RGB_BYTES);
        ++applied;
    }
    free(pixels);
    update_notice(state);
    swap->applied=applied;
    return applied;
}

/* The original S105/S106 feature used ONLY the process working directory.
 * Launching a GUI shortcut or a Linux executable from another directory
 * therefore silently produced "Enabled (0 panels)" even with a valid pack
 * beside avp_pc. Keep one authoritative pack per attempt, and never merge
 * a stale or corrupt panel from another directory. */
static void prototype_selected_dir(char *out,size_t cap,const char *dir)
{
    if(!out||!cap)return;
    if(!dir)dir="";
    (void)snprintf(out,cap,"%s",dir);
}
static int prototype_exe_pack_dir(char *out,size_t cap,
                                  const char *exe,unsigned up)
{
    const char *slash,*back;size_t base;
    const char *suffix=up?"/../prototype_texture_restoration":
                          "/prototype_texture_restoration";
    size_t tail=strlen(suffix);
    if(!exe||!out||!cap)return 0;
    slash=strrchr(exe,'/');back=strrchr(exe,'\\');
    if(!slash || (back&&back>slash))slash=back;
    if(!slash)return 0;
    base=(size_t)(slash-exe);
    /* Keep the leading slash for executables in the filesystem root. */
    if(base==0u)base=1u;
    if(base>cap-1u || tail>cap-1u-base)return 0;
    memcpy(out,exe,base);memcpy(out+base,suffix,tail+1u);
    return 1;
}
unsigned avp_pc_prototype_textures_apply_search(AvpPcPrototypeTextures *swap,
    AvpAvpGpuState *state,const char *override_directory,const char *executable_path,
    const char *working_pack_directory,int enabled,char *selected_directory,
    size_t selected_capacity)
{
    char candidate[4096];unsigned count=0u;
    prototype_selected_dir(selected_directory,selected_capacity,"");
    if(!enabled){
        (void)avp_pc_prototype_textures_apply(swap,state,NULL,0);
        return 0u;
    }
    if(override_directory&&*override_directory){
        count=avp_pc_prototype_textures_apply(swap,state,override_directory,1);
        prototype_selected_dir(selected_directory,selected_capacity,override_directory);
        return count;
    }
    if(prototype_exe_pack_dir(candidate,sizeof(candidate),executable_path,0u)){
        count=avp_pc_prototype_textures_apply(swap,state,candidate,1);
        if(count){prototype_selected_dir(selected_directory,selected_capacity,candidate);return count;}
    }
    if(prototype_exe_pack_dir(candidate,sizeof(candidate),executable_path,1u)){
        count=avp_pc_prototype_textures_apply(swap,state,candidate,1);
        if(count){prototype_selected_dir(selected_directory,selected_capacity,candidate);return count;}
    }
    if(working_pack_directory&&*working_pack_directory){
        count=avp_pc_prototype_textures_apply(swap,state,working_pack_directory,1);
        if(count){prototype_selected_dir(selected_directory,selected_capacity,working_pack_directory);return count;}
    }
    /* No valid pack: the last attempt already restored the retail image. */
    return 0u;
}
