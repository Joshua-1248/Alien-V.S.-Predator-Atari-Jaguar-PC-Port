#include "pc/pc_prototype_textures.h"
#include "jaguar/avp_gpu/bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_TEST_DIRECTORY(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_TEST_DIRECTORY(p) mkdir(p,0700)
#endif

/* Self-contained RGB->CRY test double; tests swap ownership, not QCONV. */
int avp_gpu_build_cry_image(AvpGpuRgbImage *im,const uint8_t *qconv,size_t n)
{
    size_t i,count;uint16_t *p;
    if(!im||!im->rgb||!qconv||n<131072u)return 0;
    count=(size_t)im->width*(size_t)im->height;
    p=(uint16_t*)malloc(count*sizeof(*p));if(!p)return 0;
    for(i=0;i<count;i++)p[i]=(uint16_t)im->rgb[i*3u];
    free(im->cry16);im->cry16=p;return 1;
}
void avp_gpu_free_rgb_image(AvpGpuRgbImage *im)
{if(!im)return;free(im->rgb);free(im->cry16);memset(im,0,sizeof(*im));}
static void die(const char *what){fprintf(stderr,"S105 TEST FAILURE: %s\n",what);exit(1);}
#define CHECK(cond,msg) do{if(!(cond))die(msg);}while(0)
static void write_fixture(const char *dir,unsigned slot,size_t n,unsigned char value)
{
    char path[1024]; FILE *f; size_t i;
    if(snprintf(path,sizeof(path),"%s/%02u.rgb",dir,slot)>=(int)sizeof(path))die("path");
    f=fopen(path,"wb");CHECK(f!=NULL,"cannot create fixture");
    for(i=0;i<n;i++)CHECK(fputc(value,f)!=EOF,"fixture write");
    fclose(f);
}
static uint32_t test_crc32(unsigned char value,size_t n)
{
    uint32_t crc=0xffffffffu;size_t i;unsigned bit;
    for(i=0;i<n;i++){
        crc^=value;
        for(bit=0u;bit<8u;bit++)crc=(crc>>1u)^((crc&1u)?0xedb88320u:0u);
    }
    return crc^0xffffffffu;
}
static void write_frame(const char *dir,unsigned slot,unsigned char value,int corrupt)
{
    char path[1024]; FILE *f;size_t i;
    unsigned char header[16]={'A','V','P','T','E','X','1',0,0,0,0,0,0,0,0,0};
    uint32_t n=AVP_PC_PROTOTYPE_RGB_BYTES;
    uint32_t crc=test_crc32(value,n);
    if(snprintf(path,sizeof(path),"%s/%02u.prgb",dir,slot)>=(int)sizeof(path))die("frame path");
    for(i=0;i<4u;i++){
        header[8u+i]=(unsigned char)(n>>(8u*i));
        header[12u+i]=(unsigned char)(crc>>(8u*i));
    }
    f=fopen(path,"wb");CHECK(f!=NULL,"create framed replacement");
    CHECK(fwrite(header,1u,sizeof(header),f)==sizeof(header),"frame header");
    for(i=0;i<n;i++)CHECK(fputc(value+(unsigned char)(corrupt&&i==10u),f)!=EOF,"frame data");
    CHECK(fclose(f)==0,"close frame");
}
/* An owner's verified S106 local pack can optionally be supplied as argv[2].
 * Covers all retail 0..55 slots under the exact same C loader used by avp_pc,
 * not only the hand-built slot 00 fixture. No proprietary art is compiled in. */
static void test_user_pack(const char *pack)
{
    static const unsigned fallback[5]={38u,39u,40u,41u,49u};
    AvpAvpGpuState *state=(AvpAvpGpuState*)calloc(1,sizeof(*state));
    AvpPcPrototypeTextures swap={0};unsigned i,j;
    CHECK(state!=NULL,"pack state alloc");
    state->qconv=(uint8_t*)malloc(131072u);state->qconv_size=131072u;
    CHECK(state->qconv!=NULL,"pack qconv alloc");
    for(i=0u;i<AVP_PC_PROTOTYPE_SLOTS;i++){
        AvpGpuRgbImage *im=&state->panel_tex[i];
        im->width=im->height=128;
        im->rgb=(uint8_t*)malloc(AVP_PC_PROTOTYPE_RGB_BYTES);
        im->cry16=(uint16_t*)malloc(AVP_PC_PROTOTYPE_CRY_BYTES);
        CHECK(im->rgb!=NULL&&im->cry16!=NULL,"pack buffer alloc");
        memset(im->rgb,17,AVP_PC_PROTOTYPE_RGB_BYTES);
        for(j=0;j<128u*128u;j++)im->cry16[j]=17;
    }
    avp_pc_prototype_textures_capture(&swap,state);
    CHECK(avp_pc_prototype_textures_apply(&swap,state,pack,1)==51u,
          "full validated pack must load 51 images");
    for(i=0u;i<AVP_PC_PROTOTYPE_SLOTS;i++){
        int untouched=0;
        AvpGpuRgbImage *im=&state->panel_tex[i];
        for(j=0;j<5u;j++)if(fallback[j]==i)untouched=1;
        if(untouched){CHECK(im->rgb[0]==17&&im->cry16[0]==17,
                           "retail fallback slot changed");}
        else CHECK(memcmp(im->rgb,swap.retail_rgb[i],AVP_PC_PROTOTYPE_RGB_BYTES)!=0,
                   "valid replacement did not change a source panel");
    }
    CHECK(avp_pc_prototype_textures_apply(&swap,state,pack,0)==0u,
          "full pack disable count");
    for(i=0u;i<AVP_PC_PROTOTYPE_SLOTS;i++){
        CHECK(memcmp(state->panel_tex[i].rgb,swap.retail_rgb[i],
                     AVP_PC_PROTOTYPE_RGB_BYTES)==0,"pack retail RGB restore");
        CHECK(memcmp(state->panel_tex[i].cry16,swap.retail_cry[i],
                     AVP_PC_PROTOTYPE_CRY_BYTES)==0,"pack retail CRY restore");
    }
    avp_pc_prototype_textures_destroy(&swap);
    for(i=0u;i<AVP_PC_PROTOTYPE_SLOTS;i++)avp_gpu_free_rgb_image(&state->panel_tex[i]);
    free(state->qconv);free(state);
    puts("S108 full private pack integration: 51 replacements / 5 retail fallbacks / reversible PASS");
}
int main(int argc,char **argv)
{
    AvpAvpGpuState *state;AvpPcPrototypeTextures swap={0};
    const char *dir;AvpGpuRgbImage *one;uint8_t *address;uint16_t *cry_address;
    int i;
    if(argc!=2&&argc!=3)die("provide writable fixture directory [optional verified 51-panel pack]");
    dir=argv[1];state=(AvpAvpGpuState*)calloc(1,sizeof(*state));CHECK(state,"alloc state");
    one=&state->panel_tex[0];one->width=one->height=128;
    one->rgb=(uint8_t*)malloc(AVP_PC_PROTOTYPE_RGB_BYTES);
    one->cry16=(uint16_t*)malloc(AVP_PC_PROTOTYPE_CRY_BYTES);
    CHECK(one->rgb&&one->cry16,"alloc buffers");
    memset(one->rgb,17,AVP_PC_PROTOTYPE_RGB_BYTES);
    for(i=0;i<128*128;i++)one->cry16[i]=17;
    state->qconv=(uint8_t*)malloc(131072u);state->qconv_size=131072u;
    CHECK(state->qconv,"qconv alloc");
    avp_pc_prototype_textures_capture(&swap,state);
    address=one->rgb;cry_address=one->cry16;
    CHECK(avp_pc_prototype_textures_apply(&swap,state,dir,0)==0u,"disabled count");
    CHECK(one->rgb[0]==17&&one->cry16[0]==17,"disabled original");
    write_fixture(dir,0u,AVP_PC_PROTOTYPE_RGB_BYTES,88u);
    write_fixture(dir,1u,24u,99u); /* malformed data must never override. */
    CHECK(avp_pc_prototype_textures_apply(&swap,state,dir,1)==1u,"enabled count");
    CHECK(one->rgb==address&&one->cry16==cry_address,"buffers moved: co-op borrowing unsafe");
    CHECK(one->rgb[0]==88&&one->rgb[49151]==88&&one->cry16[0]==88,"correct replacement");
    write_frame(dir,0u,99u,0);
    CHECK(avp_pc_prototype_textures_apply(&swap,state,dir,1)==1u,"verified framed count");
    CHECK(one->rgb[0]==99&&one->cry16[0]==99,"framed replacement overrides legacy");
    write_frame(dir,0u,99u,1);
    CHECK(avp_pc_prototype_textures_apply(&swap,state,dir,1)==0u,"corrupt framed image rejected");
    CHECK(one->rgb[0]==17&&one->cry16[0]==17,"corrupt data must fall back to retail not stale legacy");
    CHECK(avp_pc_prototype_textures_apply(&swap,state,dir,0)==0u,"disabled count 2");
    CHECK(one->rgb[0]==17&&one->rgb[49151]==17&&one->cry16[0]==17,"restored pixels and CRY");
    /* S108: the pack beside the binary wins regardless of caller cwd;
     * only the caller-selected env override is exclusive. */
    {
        char exe_dir[1024],exe_path[1024],pack_dir[1024],selected[4096];
        CHECK(snprintf(exe_dir,sizeof(exe_dir),"%s/s108_test_bin",dir)<(int)sizeof(exe_dir),"exe dir");
        CHECK(MAKE_TEST_DIRECTORY(exe_dir)==0,"create fake exe dir");
        CHECK(snprintf(exe_path,sizeof(exe_path),"%s/avp_pc",exe_dir)<(int)sizeof(exe_path),"exe path");
        CHECK(snprintf(pack_dir,sizeof(pack_dir),"%s/prototype_texture_restoration",exe_dir)<(int)sizeof(pack_dir),"pack dir");
        CHECK(MAKE_TEST_DIRECTORY(pack_dir)==0,"create pack dir");
        write_frame(pack_dir,0u,77u,0);
        CHECK(avp_pc_prototype_textures_apply_search(&swap,state,NULL,exe_path,dir,1,
                  selected,sizeof(selected))==1u,"binary-adjacent pack must load");
        CHECK(strcmp(selected,pack_dir)==0&&one->rgb[0]==77&&one->cry16[0]==77,
              "wrong executable-relative source or pixels");
        CHECK(one->rgb==address&&one->cry16==cry_address,"co-op addresses changed on search");
        CHECK(avp_pc_prototype_textures_apply_search(&swap,state,dir,exe_path,pack_dir,1,
                  selected,sizeof(selected))==0u,"bad explicit override cannot silently switch source");
        CHECK(one->rgb[0]==17&&one->cry16[0]==17&&strcmp(selected,dir)==0,
              "bad override does not restore retail");
        CHECK(avp_pc_prototype_textures_apply_search(&swap,state,NULL,"/missing/avp_pc",pack_dir,1,
                  selected,sizeof(selected))==1u,"cwd legacy fallback");
        CHECK(strcmp(selected,pack_dir)==0&&one->rgb[0]==77,"legacy fallback mismatch");
        CHECK(avp_pc_prototype_textures_apply_search(&swap,state,NULL,exe_path,pack_dir,0,
                  selected,sizeof(selected))==0u,"off search count");
        CHECK(!selected[0]&&one->rgb[0]==17&&one->cry16[0]==17,
              "disabled search must restore retail");
        /* Typical source layout: SOURCE/build/avp_pc and
         * SOURCE/prototype_texture_restoration/NN.prgb. */
        {
            char parent_pack[1024],child_panel[1024];
            CHECK(snprintf(parent_pack,sizeof(parent_pack),"%s/prototype_texture_restoration",dir)<(int)sizeof(parent_pack),"parent pack");
            CHECK(MAKE_TEST_DIRECTORY(parent_pack)==0,"parent pack directory");
            write_frame(parent_pack,0u,66u,0);
            CHECK(snprintf(child_panel,sizeof(child_panel),"%s/00.prgb",pack_dir)<(int)sizeof(child_panel),"child panel");
            CHECK(remove(child_panel)==0,"remove child pack frame");
            CHECK(avp_pc_prototype_textures_apply_search(&swap,state,NULL,exe_path,
                  "/missing/current_working_directory",1,selected,sizeof(selected))==1u,
                  "source build parent pack fallback");
            CHECK(strcmp(selected,parent_pack)==0&&one->rgb[0]==66,
                  "source build parent pack selected incorrectly");
            CHECK(avp_pc_prototype_textures_apply_search(&swap,state,NULL,exe_path,
                  parent_pack,0,selected,sizeof(selected))==0u&&one->rgb[0]==17,
                  "parent pack disable restore");
        }
    }
    avp_pc_prototype_textures_destroy(&swap);
    avp_gpu_free_rgb_image(one);free(state->qconv);free(state);
    puts("S108 prototype texture pack: executable-relative, cwd fallback, override, legacy, verified framed RGB, CRC rejection, stable buffers, CRY and retail restore PASS");
    if(argc==3)test_user_pack(argv[2]);
    return 0;
}
