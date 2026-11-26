#include "jaguar/avp_gpu/assets.h"
#include "avp_resources.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/notice_composite.h"
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "mjp.h"
#include "jaguar/cry.h"
#include "jaguar/object_processor.h"
#include <stdlib.h>
#include <string.h>

static u32 rom_be32(const AvpRomImage *rom,size_t off)
{
    if(!rom||!rom->data||off+4u>rom->size)return 0;
    return ((u32)rom->data[off]<<24)|((u32)rom->data[off+1u]<<16)|
           ((u32)rom->data[off+2u]<<8)|rom->data[off+3u];
}
static const u8 *rom_ptr(const AvpRomImage *rom,u32 jag_addr,size_t need)
{
    size_t off;
    if(!rom||!rom->data||jag_addr<0x800000u)return NULL;
    off=(size_t)(jag_addr-0x800000u);
    if(off>rom->size||need>rom->size-off)return NULL;
    return rom->data+off;
}
static void load_pause_assets(AvpAvpGpuState *state,const AvpRomImage *rom)
{
    u32 palp,bsavep,barp,gamep,hudp,onep,pausedp,pointerp,sliderp,threep,twop;
    const u8 *pause_bsave,*pause_bar,*pause_game,*pause_hud,*pause_one,*pause_paused;
    const u8 *pause_pointer,*pause_slider,*pause_three,*pause_two;
    u8 palette_rgb[16][3];
    unsigned i;
    if(!state||!rom||!rom->exact_retail_sha256||rom->size<=0x15f90u)return;

    /* RDB.SYM pointer variables in the canonical retail link.  Resolve each
       payload through the pointer variable; no retail art is embedded here. */
    palp=rom_be32(rom,0x15f58u); bsavep=rom_be32(rom,0x15f5cu);
    barp=rom_be32(rom,0x15f60u); gamep=rom_be32(rom,0x15f6cu);
    hudp=rom_be32(rom,0x15f70u); onep=rom_be32(rom,0x15f74u);
    pausedp=rom_be32(rom,0x15f78u); pointerp=rom_be32(rom,0x15f7cu);
    sliderp=rom_be32(rom,0x15f84u); threep=rom_be32(rom,0x15f88u);
    twop=rom_be32(rom,0x15f8cu);

    {
        const u8 *pal=rom_ptr(rom,palp,32u);
        if(!pal)return;
        for(i=0;i<16u;i++){
            u16 cry=(u16)(((u16)pal[i*2u]<<8)|pal[i*2u+1u]);
            avp_jaguar_cry16_to_rgb(cry,&palette_rgb[i][0],&palette_rgb[i][1],&palette_rgb[i][2]);
        }
    }
    pause_bsave=rom_ptr(rom,bsavep,128u*24u/2u);
    pause_bar=rom_ptr(rom,barp,160u*24u/2u);
    pause_game=rom_ptr(rom,gamep,96u*24u/2u);
    pause_hud=rom_ptr(rom,hudp,64u*20u/2u);
    pause_one=rom_ptr(rom,onep,32u*24u/2u);
    pause_paused=rom_ptr(rom,pausedp,208u*36u/2u);
    pause_pointer=rom_ptr(rom,pointerp,32u*28u/2u);
    pause_slider=rom_ptr(rom,sliderp,32u*32u/2u);
    pause_three=rom_ptr(rom,threep,32u*24u/2u);
    pause_two=rom_ptr(rom,twop,32u*24u/2u);
    avp_jaguar_pause_op_set_assets(&state->pause_op,pause_paused,pause_pointer,pause_hud,
        pause_bar,pause_slider,pause_game,pause_one,pause_two,pause_three,pause_bsave,palette_rgb);
}

int avp_gpu_assets_load_retail(AvpAvpGpuState *state,const AvpRomImage *rom)
{
    unsigned i,ok=0;
    if(!state||!rom)return 0;
    load_pause_assets(state,rom);
    /* COMPUTER.S :: InitCDisplay loads compbg_pal, compbg_cmp; cobj_tab
       chooses the FILES records 104..132 for fixed illustrations and maps.
       The RDB.DTA table confirms their exact retail file numbers and sizes.
       Decompress once, never embed or redistribute cartridge pixels. */
    {u8 *pal=NULL,*bg=NULL;size_t pn=0,bn=0;char tag[5];
     free(state->computer_bg_img);state->computer_bg_img=NULL;state->computer_bg_img_size=0;
     state->computer_palette_valid=0;
     if(avp_resources_decode_file(rom,102u,tag,&pal,&pn)&&pn==512u){
         for(unsigned c=0;c<256u;c++){
             u16 cry=(u16)(((u16)pal[c*2u]<<8)|pal[c*2u+1u]);u8 r,g,b;
             avp_jaguar_cry16_to_rgb(cry,&r,&g,&b);
             state->computer_palette[c]=((u32)r<<16)|((u32)g<<8)|(u32)b;
         }
         state->computer_palette_valid=1;
     }
     if(avp_resources_decode_file(rom,103u,tag,&bg,&bn)&&bn==320u*240u){
         state->computer_bg_img=bg;state->computer_bg_img_size=bn;bg=NULL;
     }
     free(pal);free(bg);
     for(unsigned ci=0;ci<29u;ci++){
         u8 *d=NULL;size_t dn=0;
         free(state->computer_overlay_img[ci]);state->computer_overlay_img[ci]=NULL;
         state->computer_overlay_size[ci]=0;
         if(avp_resources_decode_file(rom,104u+ci,tag,&d,&dn)){
             state->computer_overlay_img[ci]=d;state->computer_overlay_size[ci]=dn;d=NULL;
         }
         free(d);
     }
    }
    {u8 *qc=NULL;size_t qn=0;char qt[5];free(state->qconv);state->qconv=NULL;state->qconv_size=0;if(avp_resources_decode_file(rom,239u,qt,&qc,&qn)&&qn>=131072u){state->qconv=qc;state->qconv_size=qn;qc=NULL;}free(qc);}
    for(i=0;i<74u;i++){
        char tag[5];const u8 *payload=NULL;size_t n=0;int quality;
        avp_gpu_free_rgb_image(&state->panel_tex[i]);
        if(!avp_resources_file_payload(rom,i,tag,&payload,&n))continue;
        if(tag[0]!='J'||tag[1]!='P')continue;
        quality=(tag[2]-'0')*10+(tag[3]-'0');
        if(quality<1||quality>100)continue;
        if(avp_gpu_decode_jagpeg(payload,n,quality,1,&state->panel_tex[i])){
            if(state->qconv&&state->qconv_size>=131072u)(void)avp_gpu_build_cry_image(&state->panel_tex[i],state->qconv,state->qconv_size);
            ++ok;
        }
    }
    /* LEVELS.S notice_list composites the common base FILES 16 with the
     * semantic notice art. Five numbered SUBLEVEL panels use FILES 69..73.
     * All decoded input lives in panel_tex; no extracted assets are shipped. */
    {
        static const unsigned overlay_ids[13]={62,57,61,59,58,60,56,64,63,67,66,68,65};
        unsigned ni;
        for(ni=0;ni<18u;ni++){
            const unsigned type=ni<13u?ni:0u;
            const AvpGpuRgbImage *num=ni>=13u?&state->panel_tex[69u+ni-13u]:NULL;
            avp_gpu_free_rgb_image(&state->notice_tex[ni]);
            (void)avp_gpu_notice_composite(&state->panel_tex[16u],
                    &state->panel_tex[overlay_ids[type]],num,
                    type==0u?-16:0,&state->notice_tex[ni]);
            if(num&&(!num->rgb||!state->notice_tex[ni].rgb)){
                /* Never silently drop a required floor number. */
                avp_gpu_free_rgb_image(&state->notice_tex[ni]);
            }
        }
    }
    {unsigned j;for(j=74u;j<=86u;j++){u8 *d=NULL;size_t dn=0;char tag[5];if(avp_resources_decode_file(rom,j,tag,&d,&dn)&&dn>=256u){memcpy(state->window_pairs[j-74u],d,256u);state->window_pair_loaded[j-74u]=1;}free(d);}}
    state->predator_scope_valid=0; /* New retail HUD brush resets audio history. */
    {unsigned sp;for(sp=0;sp<3u;sp++){u8 *d=NULL,*pal=NULL,*en=NULL,*sf=NULL;size_t dn=0,pn=0,enn=0,sfn=0;char tag[5];unsigned hf=185u+sp,sff=188u+sp,pf=201u+sp,ef=198u+sp;if(avp_resources_decode_file(rom,hf,tag,&d,&dn)){free(state->hud_base[sp]);state->hud_base[sp]=d;state->hud_base_size[sp]=dn;d=NULL;}if(avp_resources_decode_file(rom,ef,tag,&en,&enn)){free(state->hud_energy[sp]);state->hud_energy[sp]=en;state->hud_energy_size[sp]=enn;en=NULL;}if(avp_resources_decode_file(rom,sff,tag,&sf,&sfn)){free(state->hud_score_font[sp]);state->hud_score_font[sp]=sf;state->hud_score_font_size[sp]=sfn;sf=NULL;}if(avp_resources_decode_file(rom,pf,tag,&pal,&pn)&&pn>=64u){unsigned c;for(c=0;c<32u;c++){u16 cry=(u16)(((u16)pal[c*2u]<<8)|pal[c*2u+1u]);state->hud_pal_cry[sp][c]=cry;avp_jaguar_cry16_to_rgb(cry,&state->hud_pal[sp][c][0],&state->hud_pal[sp][c][1],&state->hud_pal[sp][c][2]);}}free(d);free(en);free(sf);free(pal);}}
    /* Original MAZESCRN.S::StdHUD obtains wepbar_j8=204 and then
       wep_bars copies 44 of the 48 source pixels into each weapon strip. */
    {u8 *d=NULL;size_t dn=0;char tag[5];
     free(state->hud_weapon_bar);state->hud_weapon_bar=NULL;state->hud_weapon_bar_size=0;
     if(avp_resources_decode_file(rom,204u,tag,&d,&dn)&&dn>=48u*8u){
         state->hud_weapon_bar=d;state->hud_weapon_bar_size=dn;d=NULL;
     }
     free(d);
    }
    /* A_GetIm::embryo, immediately following the six motion-tracker brushes.
       The ROM owns all 18 frames; nothing is embedded in the port. */
    {u8 *d=NULL;size_t dn=0;char tag[5];
     free(state->hud_cocoon);state->hud_cocoon=NULL;state->hud_cocoon_size=0;
     if(avp_resources_decode_file(rom,197u,tag,&d,&dn) && dn>=18u*32u*32u){
         state->hud_cocoon=d;state->hud_cocoon_size=dn;d=NULL;
     }
     free(d);
    }
    /* Exact M_GetIm file IDs recovered from retail RDBTXT: allscope=$BF,
       scbase1=$C0, scbase2=$C1, mtrakill=$C2, blips=$C3, scbasex=$C4.
       Keep the artwork ROM-owned and bind decoded bytes at runtime. */
    {struct TrkBind { unsigned file; u8 **dst; size_t *size; } bind[]={
        {191u,&state->tracker_ring,&state->tracker_ring_size},
        {192u,&state->tracker_base1,&state->tracker_base1_size},
        {193u,&state->tracker_base2,&state->tracker_base2_size},
        {194u,&state->tracker_kill,&state->tracker_kill_size},
        {195u,&state->tracker_blips,&state->tracker_blips_size},
        {196u,&state->tracker_basex,&state->tracker_basex_size}
      };
      for(unsigned ti=0;ti<sizeof(bind)/sizeof(bind[0]);ti++){u8 *d=NULL;size_t dn=0;char tag[5];
          free(*bind[ti].dst);*bind[ti].dst=NULL;*bind[ti].size=0;
          if(avp_resources_decode_file(rom,bind[ti].file,tag,&d,&dn)){*bind[ti].dst=d;*bind[ti].size=dn;d=NULL;}
          free(d);
      }}
    {unsigned ai;state->aaf_loaded=0;for(ai=0;ai<63u;ai++){u8*d=NULL;size_t dn=0;free(state->aaf[ai]);state->aaf[ai]=NULL;state->aaf_size[ai]=0;if(avp_resources_decode_aaf(rom,ai,&d,&dn)){state->aaf[ai]=d;state->aaf_size[ai]=dn;++state->aaf_loaded;}}}
    /* RDB.SYM/OVERS.DB prove hmsgfont is the pointer variable at $815C34,
       followed by tinyfont at $815C38.  Resolve those pointer values from the
       user's retail cartridge; the 0x4C0-byte span is exactly 512x19 1bpp. */
    state->hmsg_font=NULL;state->hmsg_font_size=0;
    if(rom->exact_retail_sha256&&rom->size>0x15c3cu){
        u32 hp=((u32)rom->data[0x15c34u]<<24)|((u32)rom->data[0x15c35u]<<16)|((u32)rom->data[0x15c36u]<<8)|rom->data[0x15c37u];
        u32 np=((u32)rom->data[0x15c38u]<<24)|((u32)rom->data[0x15c39u]<<16)|((u32)rom->data[0x15c3au]<<8)|rom->data[0x15c3bu];
        if(hp>=0x800000u&&np>hp&&np<=0x800000u+rom->size){state->hmsg_font=rom->data+(hp-0x800000u);state->hmsg_font_size=(size_t)(np-hp);}
        state->hud_tiny_font=NULL;state->hud_tiny_font_size=0;
        if(np>=0x800000u&&rom->size>0x15c40u){u32 xp=((u32)rom->data[0x15c3cu]<<24)|((u32)rom->data[0x15c3du]<<16)|((u32)rom->data[0x15c3eu]<<8)|rom->data[0x15c3fu];if(xp>np&&xp<=0x800000u+rom->size){state->hud_tiny_font=rom->data+(np-0x800000u);state->hud_tiny_font_size=(size_t)(xp-np);}}
    }
    /* OVERS.DB stores the three 256x14 2bpp computer-font brushes as
       pointer/size descriptors at font_a/font_b/font_c ($815C1C..$815C33). */
    {static const u32 desc[3]={0x815c1cu,0x815c24u,0x815c2cu};
     for(unsigned cf=0;cf<3u;cf++){
         state->computer_font[cf]=NULL;state->computer_font_size[cf]=0;
         (void)avp_resources_rom_descriptor(rom,desc[cf],&state->computer_font[cf],&state->computer_font_size[cf]);
     }}
    {unsigned fi;for(fi=133u;fi<=184u;fi++){unsigned slot=fi-133u;u8*d=NULL;size_t dn=0;char tag[5];free(state->fp_img[slot]);state->fp_img[slot]=NULL;state->fp_size[slot]=0;state->fp_pal_valid[slot]=0;if(avp_resources_decode_file(rom,fi,tag,&d,&dn)){state->fp_img[slot]=d;state->fp_size[slot]=dn;d=NULL;}}
     /* Retail first-person palette resources contain 208 CRY16 entries (416 bytes),
        except the 16-color muzzle-flash palettes (32 bytes). Bind each active image
        to the source-correlated palette rather than converting pixels heuristically.
        Flash3a/b files 139/140 use warm file 147; puffa/b 141/142 use gray
        file 148.  R97 had these two distinct source palettes swapped. */
     {static const unsigned map[][2]={{133,143},{134,143},{135,144},{136,145},{137,146},{138,146},{139,147},{140,147},{141,148},{142,148},{149,153},{150,153},{151,153},{152,153},{154,156},{155,157},{158,164},{159,164},{160,164},{166,168},{167,168},{169,172},{170,172},{171,172},{173,176},{174,176},{175,176},{177,183},{178,183},{179,184},{180,184},{181,184},{182,184}};unsigned mi;for(mi=0;mi<sizeof(map)/sizeof(map[0]);mi++){unsigned img=map[mi][0],pal=map[mi][1],slot=img-133u;u8*pd=NULL;size_t pn=0;char pt[5];if(slot>=52u)continue;if(avp_resources_decode_file(rom,pal,pt,&pd,&pn)){unsigned colors=(unsigned)(pn/2u);if(colors>208u)colors=208u;for(unsigned c=0;c<colors;c++){u16 cry=(u16)(((u16)pd[c*2u]<<8)|pd[c*2u+1u]);avp_jaguar_cry16_to_rgb(cry,&state->fp_pal[slot][c][0],&state->fp_pal[slot][c][1],&state->fp_pal[slot][c][2]);}state->fp_pal_valid[slot]=(unsigned char)colors;}free(pd);}}}
    state->textures_loaded=(int)ok;
    return (int)ok;
}


/* Retail MJP Make_TLi binds $4BF40 (FILES 236) to SCBITOBJ slot 2,
 * and $4BF44 (FILES 237) to slot 4. Both are already 320-pixel-wide
 * RGB16 buffers in the retail GZIP records. The small JagPEG 205/207
 * files are NOT the OP logo pixel buffers; never enlarge them here.
 * Keep all source pixels ROM-owned. The original OP apertures are 320x110
 * and 320x82; they are distinct from these resource allocation heights. */
static int frontend_load_title_op_buffer(AvpAvpGpuState *state,
                                         const AvpRomImage *rom,
                                         unsigned file_index,unsigned role)
{
    const unsigned width=320u;
    /* FILES 233 is retail Legal_Pic ($4BF48): a 320x40 RGB16 bitmap of the
     * copyright notice, blitted by the MJP GPU path (Clear_Bu $1FC66). */
    const unsigned height=(file_index==236u)?122u:(file_index==233u)?40u:76u;
    const size_t pixel_count=(size_t)width*height;
    u8 *source=NULL,*out=NULL;
    size_t n=0u;
    char tag[5];
    if(!avp_resources_decode_file(rom,file_index,tag,&source,&n) ||
       memcmp(tag,"GZIP",4)!=0 || n!=pixel_count*2u){
        free(source);
        return 0;
    }
    out=(u8*)malloc(pixel_count*3u);
    if(!out){free(source);return 0;}
    for(size_t i=0u;i<pixel_count;i++){
        const u16 value=(u16)(((u16)source[2u*i]<<8)|source[2u*i+1u]);
        avp_gpu_rgb16_unpack(value,&out[3u*i],&out[3u*i+1u],&out[3u*i+2u]);
    }
    free(source);
    state->frontend_img[AVP_MJP_TITLE][role].width=(int)width;
    state->frontend_img[AVP_MJP_TITLE][role].height=(int)height;
    state->frontend_img[AVP_MJP_TITLE][role].rgb=out;
    state->frontend_loaded[AVP_MJP_TITLE][role]=1u;
    state->frontend_mode=AVP_MJP_TITLE;
    state->frontend_picture_x=0x12+40; /* retail $4BED4=0 centre */
    return 1;
}

int avp_gpu_assets_frontend_load(AvpAvpGpuState *state,const AvpRomImage *rom,unsigned mode,unsigned file_index,unsigned role)
{
    char tag[5];const u8 *payload=NULL;size_t n=0;int q=80;
    if(!state||!rom||mode>=10u||role>=9u)return 0;
    avp_gpu_frontend_enter_mode(state,mode);
    avp_gpu_free_rgb_image(&state->frontend_img[mode][role]);state->frontend_loaded[mode][role]=0;
    if(!avp_resources_file_payload(rom,file_index,tag,&payload,&n))return 0;
    if(mode==AVP_MJP_TITLE &&
       ((file_index==237u && role==2u)||(file_index==236u && role==3u)||
        (file_index==233u && role==4u)))
        return frontend_load_title_op_buffer(state,rom,file_index,role);
    if(tag[0]!='J'||tag[1]!='P'){
        if(file_index==232u && mode==AVP_MJP_TITLE){
            /* Retail $01FD0A: FILES 232 is a raw 160x40 RGB16 PREDATOR
             * wordmark (pre-composited over its background), blitted
             * opaquely into the FILES 208 title buffer at (153,33). */
            u8 *d=NULL;size_t dn=0;char dt[5];
            if(avp_resources_decode_file(rom,file_index,dt,&d,&dn) && dn==160u*40u*2u){
                AvpGpuRgbImage *im=&state->frontend_img[mode][role];
                im->rgb=(u8*)malloc(160u*40u*3u);
                if(im->rgb){
                    for(size_t i=0;i<160u*40u;i++)
                        avp_gpu_rgb16_unpack((u16)((d[2u*i]<<8)|d[2u*i+1u]),&im->rgb[3u*i],
                                             &im->rgb[3u*i+1u],&im->rgb[3u*i+2u]);
                    im->width=160;im->height=40;
                    state->frontend_loaded[mode][role]=1u;
                }
            }
            free(d);
            return state->frontend_loaded[mode][role]?1:0;
        }
        if(file_index==238u && mode==AVP_MJP_CREDITS){
            /* Retail $01F5BC: FILES 238 is a 320x1373 4-bpp bitmap shown
             * through the CLUT $F00400 = $1084*index (16-step RGB16 grey);
             * index 0 stays RGB16 zero, which the OP treats as transparent. */
            u8 *d=NULL;size_t dn=0;char dt[5];
            if(avp_resources_decode_file(rom,file_index,dt,&d,&dn) && dn==160u*1373u){
                AvpGpuRgbImage *im=&state->frontend_img[mode][role];
                im->rgb=(u8*)malloc(320u*1373u*3u);
                if(im->rgb){
                    for(size_t i=0;i<320u*1373u;i++){
                        const unsigned idx=(i&1u)?(d[i>>1]&15u):(d[i>>1]>>4);
                        avp_gpu_rgb16_unpack((u16)(0x1084u*idx),&im->rgb[3u*i],
                                             &im->rgb[3u*i+1u],&im->rgb[3u*i+2u]);
                    }
                    im->width=320;im->height=1373;
                    state->frontend_loaded[mode][role]=1u;
                }
            }
            free(d);
            return state->frontend_loaded[mode][role]?1:0;
        }
        if(file_index==234u||file_index==235u){
            u8 *d=NULL;size_t dn=0;char dt[5];
            if(avp_resources_decode_file(rom,file_index,dt,&d,&dn)){
                if(file_index==234u&&dn==320u*96u*2u){free(state->avp_font);state->avp_font=d;state->avp_font_size=dn;d=NULL;}
                else if(file_index==235u&&dn==224u*39u*2u){free(state->mjp_font);state->mjp_font=d;state->mjp_font_size=dn;d=NULL;}
            }
            free(d);
        }
        return 1;
    }
    if(tag[2]>='0'&&tag[2]<='9'&&tag[3]>='0'&&tag[3]<='9')q=(tag[2]-'0')*10+(tag[3]-'0');
    if(!avp_gpu_decode_jagpeg(payload,n,q,1,&state->frontend_img[mode][role]))return 0;
    state->frontend_loaded[mode][role]=1;state->frontend_mode=mode;return 1;
}
