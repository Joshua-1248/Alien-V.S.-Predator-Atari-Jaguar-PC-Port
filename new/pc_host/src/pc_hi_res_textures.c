#include "pc/pc_hi_res_textures.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Read just conventional 8-bit truecolor PNGs. This avoids a new optional
 * external image library: the port already requires zlib for ROM assets.
 * Validate CRCs and dimensions, bound allocations and reject interlaced,
 * indexed, truncated or malformed files instead of presenting corrupt art. */
#define HR_MAX_DIM 4096u
#define HR_MAX_FILE (64u * 1024u * 1024u)
/* R307: the former shared 256 MiB cap silently truncated even a modest
 * 44-panel 1254x1254 pack (34 loaded, 10 skipped). It also left no space
 * for the 18 separately composed hi-res notice walls. Keep an explicit
 * finite allocation ceiling, but size it for a complete 74-slot pack plus
 * its sign composites. 1024 MiB is a safety limit, not a pre-allocation. */
#define HR_MAX_MEMORY ((size_t)1024u * 1024u * 1024u)
#define HR_PNG_HEADER 8u
static const unsigned char png_signature[8]={137u,80u,78u,71u,13u,10u,26u,10u};

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24u)|((uint32_t)p[1]<<16u)|((uint32_t)p[2]<<8u)|p[3];
}
static int png_read_file(const char *path,uint8_t **out,size_t *out_n)
{
    FILE *f;long len;uint8_t *bytes;
    *out=NULL;*out_n=0u;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0 || (len=ftell(f))<0 ||
       (unsigned long)len>HR_MAX_FILE || fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    bytes=(uint8_t*)malloc(len?(size_t)len:1u);
    if(!bytes){fclose(f);return 0;}
    if(fread(bytes,1u,(size_t)len,f)!=(size_t)len){free(bytes);fclose(f);return 0;}
    fclose(f);*out=bytes;*out_n=(size_t)len;return 1;
}
static int paeth(int a,int b,int c)
{
    int p=a+b-c,da=abs(p-a),db=abs(p-b),dc=abs(p-c);
    if(da<=db&&da<=dc)return a;
    return db<=dc?b:c;
}
int avp_pc_hi_res_read_png(const char *path,AvpGpuRgbImage *out)
{
    uint8_t *file=NULL,*idat=NULL,*raw=NULL,*rgb=NULL,*alpha=NULL;
    size_t fsz=0u,idat_size=0u,pos=HR_PNG_HEADER,scan_len,row_len;
    unsigned w=0u,h=0u,channels=0u,have_ihdr=0u,have_iend=0u;
    int ok=0;
    if(!png_read_file(path,&file,&fsz) || fsz<HR_PNG_HEADER ||
       memcmp(file,png_signature,HR_PNG_HEADER)!=0)goto done;
    while(pos+12u<=fsz){
        size_t n=(size_t)be32(file+pos);
        const uint8_t *kind=file+pos+4u,*chunk=file+pos+8u;
        uint32_t crc;
        if(n>fsz-pos-12u)goto done;
        crc=(uint32_t)crc32(0L,Z_NULL,0);
        crc=(uint32_t)crc32(crc,kind,(uInt)(n+4u));
        if(crc!=be32(chunk+n))goto done;
        if(memcmp(kind,"IHDR",4u)==0){
            if(have_ihdr || pos!=HR_PNG_HEADER || n!=13u)goto done;
            w=be32(chunk);h=be32(chunk+4u);
            if(w==0u||h==0u||w>HR_MAX_DIM||h>HR_MAX_DIM||
               chunk[8]!=8u||(chunk[9]!=2u&&chunk[9]!=6u)||
               chunk[10]!=0u||chunk[11]!=0u||chunk[12]!=0u)goto done;
            channels=chunk[9]==2u?3u:4u;
            have_ihdr=1u;
        }else if(memcmp(kind,"IDAT",4u)==0){
            uint8_t *more;
            if(!have_ihdr || have_iend || n>HR_MAX_FILE-idat_size)goto done;
            more=(uint8_t*)realloc(idat,idat_size+n);
            if(!more)goto done;
            idat=more;memcpy(idat+idat_size,chunk,n);idat_size+=n;
        }else if(memcmp(kind,"IEND",4u)==0){
            if(!have_ihdr||!idat_size||n!=0u||pos+12u!=fsz)goto done;
            have_iend=1u;break;
        }else if((kind[0]&0x20u)==0u){
            /* Reject unrecognized critical chunks. Ancillary chunks, e.g.
             * iCCP and tEXt, are ignored after their CRCs are verified. */
            goto done;
        }
        pos+=n+12u;
    }
    if(!have_iend)goto done;
    row_len=(size_t)w*channels;
    scan_len=(row_len+1u)*(size_t)h;
    if(scan_len>HR_MAX_FILE+4u*1024u*1024u || (size_t)w*h*3u>HR_MAX_FILE)goto done;
    raw=(uint8_t*)malloc(scan_len);rgb=(uint8_t*)malloc((size_t)w*h*3u);
    if(channels==4u)alpha=(uint8_t*)malloc((size_t)w*h);
    if(!raw||!rgb||(channels==4u&&!alpha))goto done;
    {
        uLongf actual=(uLongf)scan_len;
        if(uncompress(raw,&actual,idat,(uLong)idat_size)!=Z_OK ||
           (size_t)actual!=scan_len)goto done;
    }
    for(unsigned y=0;y<h;y++){
        uint8_t *row=raw+(size_t)y*(row_len+1u)+1u;
        const uint8_t *prev=y?raw+(size_t)(y-1u)*(row_len+1u)+1u:NULL;
        uint8_t filter=row[-1];
        if(filter>4u)goto done;
        for(size_t x=0;x<row_len;x++){
            int a=x>=channels?row[x-channels]:0,b=prev?prev[x]:0;
            int c=prev&&x>=channels?prev[x-channels]:0;
            int value=0;
            switch(filter){
            case 0u:break;
            case 1u:value=a;break;
            case 2u:value=b;break;
            case 3u:value=(a+b)/2;break;
            case 4u:value=paeth(a,b,c);break;
            }
            row[x]=(uint8_t)(row[x]+(uint8_t)value);
        }
        for(unsigned x=0;x<w;x++){
            const uint8_t *p=row+(size_t)x*channels;
            uint8_t *dest=rgb+((size_t)y*w+x)*3u;
            /* Keep straight RGB and exact source alpha. The old loader
             * premultiplied RGBA against black and destroyed transparency.
             * RGB PNGs have no mask and remain completely opaque. */
            memcpy(dest,p,3u);
            if(alpha)alpha[(size_t)y*w+x]=p[3];
        }
    }
    out->width=(int)w;out->height=(int)h;out->rgb=rgb;out->cry16=NULL;out->alpha=alpha;
    rgb=NULL;alpha=NULL;ok=1;
done:
    free(file);free(idat);free(raw);free(rgb);free(alpha);
    return ok;
}
void avp_pc_hi_res_textures_destroy(AvpPcHiResTextures *pack)
{
    unsigned i;
    if(!pack)return;
    for(i=0u;i<AVP_PC_HI_RES_SLOTS;i++){
        free(pack->image[i].rgb);free(pack->image[i].cry16);
        free(pack->image[i].alpha);
    }
    for(i=0u;i<18u;i++){
        free(pack->notice[i].rgb);free(pack->notice[i].cry16);
        free(pack->notice[i].alpha);
    }
    memset(pack,0,sizeof(*pack));
}
static int texture_directory(char *out,size_t capacity,const char *exe)
{
    const char *slash,*back;
    size_t prefix;
    static const char suffix[]="/hi_res_textures";
    if(!out||!capacity||!exe||!*exe)return 0;
    slash=strrchr(exe,'/');back=strrchr(exe,'\\');
    if(!slash||(back&&back>slash))slash=back;
    if(!slash)return 0;
    prefix=(size_t)(slash-exe);
    if(prefix==0u)prefix=1u;
    if(prefix+sizeof(suffix)>capacity)return 0;
    memcpy(out,exe,prefix);memcpy(out+prefix,suffix,sizeof(suffix));
    return 1;
}
unsigned avp_pc_hi_res_textures_load(AvpPcHiResTextures *pack,
    const char *executable_path,const uint8_t *qconv,size_t qconv_size)
{
    char directory[4096],path[4150];size_t used=0u;
    unsigned i;
    if(!pack)return 0u;
    avp_pc_hi_res_textures_destroy(pack);
    if(!qconv||qconv_size<131072u||
       !texture_directory(directory,sizeof(directory),executable_path))return 0u;
    for(i=0u;i<AVP_PC_HI_RES_SLOTS;i++){
        AvpGpuRgbImage candidate={0};
        size_t bytes;
        if(snprintf(path,sizeof(path),"%s/%02u.png",directory,i)>=(int)sizeof(path))continue;
        if(!avp_pc_hi_res_read_png(path,&candidate)){
            /* Do not log normal gaps in sparse packs. Report files which
             * exist but were rejected (unsupported PNG mode, CRC, etc.). */
            FILE *present=fopen(path,"rb");
            if(present){
                fclose(present);
                fprintf(stderr,"Hi-res textures: rejected %02u.png "
                        "(invalid/unsupported PNG or allocation failure)\n",i);
            }
            continue;
        }
        bytes=(size_t)candidate.width*(size_t)candidate.height*(candidate.alpha?6u:5u);
        if(used>HR_MAX_MEMORY || bytes>HR_MAX_MEMORY-used){
            fprintf(stderr,"Hi-res textures: skipped %02u.png "
                    "(decoded texture memory limit: %zu of %zu bytes used)\n",
                    i,used,(size_t)HR_MAX_MEMORY);
            free(candidate.rgb);free(candidate.cry16);free(candidate.alpha);
            continue;
        }
        if(!avp_gpu_build_cry_image(&candidate,qconv,qconv_size)){
            fprintf(stderr,"Hi-res textures: skipped %02u.png "
                    "(CRY conversion or memory allocation failed)\n",i);
            free(candidate.rgb);free(candidate.cry16);free(candidate.alpha);
            continue;
        }
        pack->image[i]=candidate;
        used+=bytes;
        pack->applied++;
    }
    return pack->applied;
}

/* RE45 R306: rebuild the retail 128x128 sign layout at presentation resolution.
 * Original FILES sizes, not arbitrary replacement dimensions, establish
 * lettering/number placement; each source falls back independently. */
static void notice_free(AvpGpuRgbImage *im)
{
    if(!im)return;
    free(im->rgb);free(im->cry16);free(im->alpha);
    memset(im,0,sizeof(*im));
}
static unsigned notice_extent(const AvpGpuRgbImage *orig,const AvpGpuRgbImage *rep)
{
    unsigned sw,sh;
    if(!orig||!rep||!orig->width||!orig->height||
       orig->width<=0||orig->height<=0||rep->width<=0||rep->height<=0)return 128u;
    sw=(unsigned)(((uint64_t)rep->width*128u+(unsigned)orig->width-1u)/(unsigned)orig->width);
    sh=(unsigned)(((uint64_t)rep->height*128u+(unsigned)orig->height-1u)/(unsigned)orig->height);
    return sw>sh?sw:sh;
}
static void notice_layer(AvpGpuRgbImage *dest,const AvpGpuRgbImage *source,
                         int native_width,int native_height,int y_offset)
{
    int left=(128-native_width)/2,top=(128-native_height)/2+y_offset;
    int dw=dest->width,dh=dest->height;
    int x0=(int)((int64_t)left*dw/128),x1=(int)(((int64_t)(left+native_width)*dw+127)/128);
    int y0=(int)((int64_t)top*dh/128),y1=(int)(((int64_t)(top+native_height)*dh+127)/128);
    if(!source||!source->rgb||source->width<=0||source->height<=0||
       native_width<=0||native_height<=0)return;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>dw)x1=dw;
    if(y1>dh)y1=dh;
    for(int y=y0;y<y1;y++){
        const int64_t v=((int64_t)(2*y+1)*128-2*(int64_t)top*dh);
        int sy=(int)((v*source->height)/(2*(int64_t)native_height*dh));
        if(sy<0)sy=0;
        if(sy>=source->height)sy=source->height-1;
        for(int x=x0;x<x1;x++){
            const int64_t u=((int64_t)(2*x+1)*128-2*(int64_t)left*dw);
            int sx=(int)((u*source->width)/(2*(int64_t)native_width*dw));
            size_t di,si;unsigned a;
            if(sx<0)sx=0;
            if(sx>=source->width)sx=source->width-1;
            di=((size_t)y*dw+(unsigned)x)*3u;
            si=(size_t)sy*source->width+(unsigned)sx;
            a=source->alpha?source->alpha[si]:255u;
            if(a==255u)memcpy(dest->rgb+di,source->rgb+si*3u,3u);
            else if(a!=0u)for(unsigned k=0;k<3u;k++)
                dest->rgb[di+k]=(uint8_t)((source->rgb[si*3u+k]*a+
                    dest->rgb[di+k]*(255u-a)+127u)/255u);
        }
    }
}
unsigned avp_pc_hi_res_textures_build_notices(AvpPcHiResTextures *pack,
    const AvpGpuRgbImage original[AVP_PC_HI_RES_SLOTS],
    const uint8_t *qconv,size_t qconv_size)
{
    static const unsigned overlay_ids[13]={62u,57u,61u,59u,58u,60u,56u,64u,63u,67u,66u,68u,65u};
    size_t used=0u;
    if(!pack)return 0u;
    for(unsigned i=0u;i<18u;i++)notice_free(&pack->notice[i]);
    pack->notices_composed=0u;
    if(!original||!original[16].rgb||!qconv||qconv_size<131072u)return 0u;
    for(unsigned i=0u;i<AVP_PC_HI_RES_SLOTS;i++)if(pack->image[i].rgb)
        used+=(size_t)pack->image[i].width*pack->image[i].height*
              (pack->image[i].alpha?6u:5u);
    for(unsigned ni=0;ni<18u;ni++){
        const unsigned type=ni<13u?ni:0u,oid=overlay_ids[type];
        const unsigned nid=ni>=13u?69u+ni-13u:0u;
        const AvpGpuRgbImage *base=NULL;
        const AvpGpuRgbImage *over=NULL,*num=NULL;
        const AvpGpuRgbImage *bo=&original[16u],*oo=&original[oid];
        AvpGpuRgbImage *out=&pack->notice[ni];unsigned size=128u;
        size_t bytes;
        /* Any supplied component makes this composite worth rebuilding. */
        if(!pack->image[16].rgb&&!pack->image[oid].rgb&&
           (ni<13u||!pack->image[nid].rgb))continue;
        if(!oo->rgb||oo->width<=0||oo->height<=0)continue;
        if(ni>=13u&&(!original[nid].rgb||original[nid].width<=0||
                       original[nid].height<=0))continue;
        base=pack->image[16].rgb?&pack->image[16]:bo;
        over=pack->image[oid].rgb?&pack->image[oid]:oo;
        num=ni>=13u?(pack->image[nid].rgb?&pack->image[nid]:&original[nid]):NULL;
        if(pack->image[16].rgb)size=notice_extent(bo,&pack->image[16]);
        if(pack->image[oid].rgb){unsigned n=notice_extent(oo,&pack->image[oid]);if(n>size)size=n;}
        if(ni>=13u&&pack->image[nid].rgb){unsigned n=notice_extent(&original[nid],&pack->image[nid]);if(n>size)size=n;}
        if(size>2048u)size=2048u;
        if(size<=128u)size=129u; /* Never replace a native 128x128 notice with another native atlas. */
        bytes=(size_t)size*size*5u;
        if(used>HR_MAX_MEMORY||bytes>HR_MAX_MEMORY-used){
            fprintf(stderr,"Hi-res textures: skipped notice %u "
                    "(decoded texture memory limit: %zu of %zu bytes used)\n",
                    ni,used,(size_t)HR_MAX_MEMORY);
            continue;
        }
        out->width=(int)size;out->height=(int)size;
        out->rgb=(uint8_t*)malloc((size_t)size*size*3u);
        if(!out->rgb)continue;
        for(unsigned y=0u;y<size;y++){
            unsigned sy=(unsigned)((uint64_t)y*(unsigned)base->height/size);
            for(unsigned x=0u;x<size;x++){
                unsigned sx=(unsigned)((uint64_t)x*(unsigned)base->width/size);
                memcpy(out->rgb+((size_t)y*size+x)*3u,
                       base->rgb+((size_t)sy*base->width+sx)*3u,3u);
            }
        }
        notice_layer(out,over,oo->width,oo->height,type==0u?-16:0);
        if(num)notice_layer(out,num,original[nid].width,original[nid].height,16);
        if(!avp_gpu_build_cry_image(out,qconv,qconv_size)){
            notice_free(out);continue;
        }
        used+=bytes;pack->notices_composed++;
    }
    return pack->notices_composed;
}
