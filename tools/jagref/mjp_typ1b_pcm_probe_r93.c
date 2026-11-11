/* Read-only retail MJP TYP1B PCM probe: user-supplied original ROM only.
 * Dumps one unaccompanied effect from the existing PC synthesizer to raw s16le
 * stereo at the Jaguar host DAC rate 20774Hz, for A/B comparison. */
#include "jaguar/host/fullsynth_host.h"
#include "avp_resources.h"
#include "avp_sfx_ids.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
/* Self-contained RDB decode and no-runtime-base stubs. */
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **out,size_t *n)
{
    uLongf len=0x20000u;z_stream z={0};int status;
    *out=(u8*)malloc((size_t)len);if(!*out)return 0;
    z.next_in=(Bytef*)(rom->data+0xe840u);z.avail_in=(uInt)(rom->size-0xe840u);
    z.next_out=*out;z.avail_out=(uInt)len;
    if(inflateInit2(&z,31)!=Z_OK){free(*out);*out=NULL;return 0;}
    status=inflate(&z,Z_FINISH);inflateEnd(&z);
    if(status!=Z_STREAM_END){free(*out);*out=NULL;return 0;}
    *n=z.total_out;return 1;
}
int avp_resources_decode_rdb_text(const AvpRomImage *r,u8 **d,size_t*n)
{(void)r;(void)d;(void)n;return 0;}
int avp_resources_rom_descriptor(const AvpRomImage*r,u32 a,const u8**d,size_t*n)
{(void)r;(void)a;(void)d;(void)n;return 0;}
int main(int argc,char**argv)
{
    AvpRomImage rom={0};FILE*f,*out;long len;int16_t pcm[512*2];unsigned total=0;
    if(argc!=3){fprintf(stderr,"usage: probe /path/to/AVP.jag output.raw\n");return 2;}
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END)||((len=ftell(f))!=4194304L)||fseek(f,0,SEEK_SET)){fclose(f);return 2;}
    rom.size=(size_t)len;rom.data=malloc(rom.size);if(!rom.data){fclose(f);return 2;}
    if(fread(rom.data,1,rom.size,f)!=rom.size){fclose(f);return 2;}fclose(f);
    if(!avp_jaguar_fullsynth_init(&rom))return 2;
    avp_jaguar_fullsynth_play_handle(AVP_FX_TYP1B,0x15u,3u,0,0);
    out=fopen(argv[2],"wb");if(!out)return 2;
    for(unsigned k=0;k<20u;k++){
        avp_jaguar_fullsynth_mix(pcm,512u);
        if(fwrite(pcm,sizeof(pcm),1u,out)!=1)return 2;
        if(avp_jaguar_fullsynth_handle_active(0x15u))++total;
    }
    fclose(out);
    printf("TYP1B approximate remaining-block count %u, output 10240 stereo samples at 20774Hz\n",total);
    avp_jaguar_fullsynth_shutdown();free(rom.data);return 0;
}
