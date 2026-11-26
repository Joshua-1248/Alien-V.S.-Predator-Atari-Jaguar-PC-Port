/* R346: source-backed wall pixel oracle, clipped high-res v3 row maps,
 * native Jaguar CRY lighting and multi-pass shader ordering. SDK-free test. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_pixel_oracle.h"

static void put32(uint8_t *p,uint32_t v)
{
    for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(v>>(i*8u));
}
static void make_column(uint8_t *r,uint32_t slot,uint32_t th,
                        uint32_t encoding,int shade)
{
    put32(r,1u);put32(r+4u,slot);put32(r+8u,1u);
    put32(r+12u,14u);put32(r+16u,1047u);
    put32(r+20u,32768u);put32(r+24u,1080u);
    put32(r+28u,0u);put32(r+32u,1u);
    put32(r+36u,th);put32(r+40u,encoding);
    put32(r+44u,(uint32_t)shade);
}
static uint32_t rgb_pixel(uint32_t texel)
{
    return 0xff000000u|((texel&255u)<<16u)|
        (((texel*3u)&255u)<<8u)|((texel*7u)&255u);
}
static int software_hires_row(uint32_t y,uint32_t height)
{
    /* Original Software renderer starts at the clipped first pixel, then
     * increments its double phase for every visible scanline. */
    double step=(double)height/1080.0;
    double phase=(14.5)*step;
    for(uint32_t t=14u;t<y;t++)phase+=step;
    int row=(int)phase;
    if(row<0)row=0;
    if(row>=(int)height)row=(int)height-1;
    return row;
}
static void check_source_pixels(void)
{
    const size_t v2bytes=32u+3u*48u,bytes=3u*1080u*4u;
    AvpVkWallProjectionStream wire={0};
    AvpVkWallPixelSource sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint32_t *a=calloc(720u,sizeof(uint32_t));
    uint32_t *b=calloc(128u,sizeof(uint32_t));
    uint32_t *c=calloc(1254u,sizeof(uint32_t));
    uint32_t *lut=calloc(65536u,sizeof(uint32_t));
    uint8_t *rgba=malloc(bytes),*bgra=malloc(bytes),*bad=malloc(bytes);
    uint32_t mapped=0u,rows=0u,mismatches=UINT32_MAX;
    assert(a && b && c && lut && rgba && bgra && bad);
    wire.data=calloc(1u,v2bytes);assert(wire.data);
    wire.bytes=wire.capacity=v2bytes;
    put32(wire.data,AVP_VK_WALL_PROJECTION_MAGIC);
    put32(wire.data+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    put32(wire.data+8u,3u);put32(wire.data+12u,1080u);
    put32(wire.data+16u,48u);put32(wire.data+20u,32u);
    put32(wire.data+24u,3u);
    make_column(wire.data+32u,5u,720u,AVP_WORLD_IMAGE_RGB8_ALPHA8,0);
    make_column(wire.data+80u,6u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8,-7);
    make_column(wire.data+128u,7u,1254u,AVP_WORLD_IMAGE_RGB8_ALPHA8,0);
    assert(avp_vk_wall_projection_validate(wire.data,wire.bytes));
    assert(avp_vk_wall_projection_upgrade_exact_rows(&wire,&mapped,&rows));
    assert(mapped>=1u && mapped<=2u);
    assert(rows==mapped*1033u);
    assert(avp_vk_wall_projection_validate(wire.data,wire.bytes));
    sources[5u]=(AvpVkWallPixelSource){a,1u,720u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    sources[6u]=(AvpVkWallPixelSource){b,1u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    sources[7u]=(AvpVkWallPixelSource){c,1u,1254u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(uint32_t y=0;y<720u;y++)a[y]=rgb_pixel(y);
    for(uint32_t y=0;y<1254u;y++)c[y]=rgb_pixel(y);
    for(uint32_t y=0;y<128u;y++)b[y]=0x00ff0000u|(0x5au<<8u)|(y*2u);
    /* Test synthetic LUT follows CRY high byte and shaded low Y byte. */
    for(uint32_t i=0;i<65536u;i++)
        lut[i]=((i&255u)<<16u)|(((i>>8u)&255u)<<8u)|(i&255u);
    assert(avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,rgba,bytes));
    assert(avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_BGRA8,bgra,bytes));
    for(uint32_t y=0;y<1080u;y++)for(uint32_t x=0;x<3u;x++){
        size_t at=((size_t)y*3u+x)*4u;
        uint32_t rgb=0u;
        if(y>=14u && y<1047u){
            if(x==1u){
                int row=(int)(((int64_t)y*2+1)*128/(2*1080));
                int shade=(int)(b[row]&255u)-7;
                if(shade<0)shade=0;
                rgb=lut[0x5a00u|(uint32_t)shade];
            }else {
                uint32_t th=x==0u?720u:1254u;
                uint32_t *src=x==0u?a:c;
                rgb=src[software_hires_row(y,th)]&0xffffffu;
            }
        }
        assert(rgba[at]==(uint8_t)(rgb>>16u));
        assert(rgba[at+1u]==(uint8_t)(rgb>>8u));
        assert(rgba[at+2u]==(uint8_t)rgb);
        assert(rgba[at+3u]==255u);
        assert(bgra[at]==(uint8_t)rgb);
        assert(bgra[at+1u]==rgba[at+1u]);
        assert(bgra[at+2u]==rgba[at]);
        assert(bgra[at+3u]==255u);
    }
    assert(avp_vk_wall_pixel_oracle_compare(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,rgba,bytes,&mismatches) && !mismatches);
    memcpy(bad,rgba,bytes);bad[(200u*3u+2u)*4u]^=1u;
    assert(!avp_vk_wall_pixel_oracle_compare(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes,&mismatches) && mismatches==1u);
    memset(bad,0x7eu,bytes);
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         999u,bad,bytes) && bad[0]==0x7eu);
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes-1u) && bad[0]==0x7eu);
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,NULL,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes) && bad[0]==0x7eu);
    c[0]&=0x00ffffffu; /* source alpha should make complete frame fail */
    /* 0th texel is outside the sampled segment: change a visible one. */
    c[software_hires_row(200u,1254u)]&=0x00ffffffu;
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes) && bad[0]==0x7eu);
    c[software_hires_row(200u,1254u)]|=0xff000000u;
    sources[7u].height--;
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes) && bad[0]==0x7eu);
    sources[7u].height++;
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes-4u,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes) && bad[0]==0x7eu);
    /* Invalid binding must not publish even if its column draws last. */
    sources[7u].words=NULL;
    assert(!avp_vk_wall_pixel_oracle_make(wire.data,wire.bytes,sources,lut,
         AVP_VK_WALL_PIXEL_RGBA8,bad,bytes) && bad[0]==0x7eu);
    free(a);free(b);free(c);free(lut);free(rgba);free(bgra);free(bad);
    avp_vk_wall_projection_destroy(&wire);
}
static void check_projected_shader_slot_order(const char *path)
{
    char text[32768];FILE *fp=fopen(path,"rb");size_t n;
    const char *active,*slot,*encoding;
    assert(fp);
    n=fread(text,1u,sizeof text-1u,fp);
    assert(!ferror(fp) && feof(fp));fclose(fp);text[n]='\0';
    /* R351 fixed reserved GLSL identifier `active`: it is now `wallState`.
     * Keep the genuine invalid-state -> per-slot -> format guard order. */
    active=strstr(text,"if(wallState!=1u)");
    slot=strstr(text,"if(avp_projection.words[base+1u]!=avp_params.wx)discard;");
    if(!slot)
        slot=strstr(text,"if(avp_projection.words[base+1u]!=avp_params.wx) discard;");
    encoding=strstr(text,"if(avp_projection.words[base+10u]!=avp_params.encoding");
    assert(active && slot && encoding && active<slot && slot<encoding);
    assert(strstr(text,"if(version==3u)")!=NULL);
}
int main(int argc,char **argv)
{
    assert(argc==3);
    check_source_pixels();
    check_projected_shader_slot_order(argv[1]);
    check_projected_shader_slot_order(argv[2]);
    puts("R346 source-backed RGB/CRY per-pixel oracle, v3 exact rows, shade, alpha, pass ownership PASS (CPU/static)");
    return 0;
}
