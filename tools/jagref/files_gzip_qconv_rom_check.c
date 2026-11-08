/* S39 ROM-only content verification: the Jaguar's actual GZIP-cartridge
 * execution populates FILES #239 QCONV during a real Alien level load.
 * Independently inflate the authenticated cartridge stream and compare
 * every byte to the original machine's loaded DRAM table.
 * Host zlib is an independent TEST ORACLE ONLY, never a live/native
 * replacement for the original cartridge's decompressor side effects. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_jpeg_mode2.h"
#include "m68k.h"
#include <zlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROM_SIZE 4194304u
#define JPEG_ENTRY 0xF03000u
#define JPEG_SIGNATURE 0x981E3F54u
static JagMachine machine;
static uint8_t rom[ROM_SIZE];
static uint32_t rd(void *ctx,uint32_t addr,int width)
{JagMachine*m=ctx;return width==1?jagm_read8(m,addr):width==2?jagm_read16(m,addr):jagm_read32(m,addr);}
static void wr(void *ctx,uint32_t addr,uint32_t value,int width)
{JagMachine*m=ctx;if(width==1)jagm_write8(m,addr,(uint8_t)value);else if(width==2)jagm_write16(m,addr,(uint16_t)value);else jagm_write32(m,addr,value);}
static void on_frame(JagMachine *m,void *p)
{
    static const unsigned press[]={950u,1150u,1400u,1700u,2000u};
    uint32_t mask=0;(void)p;
    for(unsigned i=0;i<5u;i++)if(m->frames>=press[i]&&m->frames<press[i]+6u)mask=0x20000u;
    jagm_set_pad(m,0u,mask);
}
int main(int argc,char **argv)
{
    FILE*f;AvpView3DBus bus={&machine,rd,wr};
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xC00000u,
                           AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpJagFilesPlan plan;
    z_stream z;
    uint8_t *compressed=NULL,*out=NULL;
    uint32_t table,sz;
    int status=1,zr;
    unsigned differences=0;
    if(argc!=2||!(f=fopen(argv[1],"rb")))return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&machine,rom,sizeof rom))return 2;
    machine.on_frame=on_frame;
    if(!jagm_run_until_gpu_pc(&machine,JPEG_ENTRY,1640u,1760u)){
        fputs("original level JPEG GPU did not start\n",stderr);return 1;
    }
    if(jagm_read32(&machine,JPEG_ENTRY)!=JPEG_SIGNATURE ||
       jagm_read8(&machine,AVP_JPEG_MODE_VARIABLE)!=2u ||
       !jagm_read8(&machine,AVP_JPEG_CRY_LOADED)){
        fputs("original GPU not in JPEG level mode with QCONV loaded\n",stderr);return 1;
    }
    table=jagm_read32(&machine,AVP_JPEG_CRY_TABLE_POINTER);
    if(table==0u || (uint64_t)table+AVP_JPEG_CRY_TABLE_BYTES>JAGM_RAM_SIZE){
        fputs("original CRY destination out of range\n",stderr);return 1;
    }
    if(avp_jagfiles_plan(&bus,&cfg,AVP_JPEG_CRY_TABLE_FILE_ID,&plan)!=AVP_JAGFILES_OK||
       plan.tag!=0x475A4950u){fputs("original FILES #239 is not GZIP\n",stderr);return 1;}
    sz=jagm_read32(&machine,plan.header+4u);
    if(sz<18u||sz>1000000u || (uint64_t)plan.payload+sz>cfg.archive_end){
        fputs("original compressed table record has invalid size\n",stderr);return 1;
    }
    compressed=malloc(sz);out=malloc(AVP_JPEG_CRY_TABLE_BYTES);
    if(!compressed||!out)goto done;
    for(uint32_t i=0;i<sz;i++)compressed[i]=jagm_read8(&machine,plan.payload+i);
    if(compressed[0]!=0x1Fu||compressed[1]!=0x8Bu || compressed[2]!=8u){
        fputs("original record is not GZIP DEFLATE\n",stderr);goto done;
    }
    memset(&z,0,sizeof z);
    z.next_in=compressed;z.avail_in=sz;
    z.next_out=out;z.avail_out=AVP_JPEG_CRY_TABLE_BYTES;
    if(inflateInit2(&z,31)!=Z_OK)goto done;
    zr=inflate(&z,Z_FINISH);
    if(zr!=Z_STREAM_END || z.total_out!=AVP_JPEG_CRY_TABLE_BYTES || z.total_in!=sz){
        fprintf(stderr,"independent GZIP oracle did not reproduce expected size: zlib=%d output=%lu input=%lu\n",zr,(unsigned long)z.total_out,(unsigned long)z.total_in);
        inflateEnd(&z);goto done;
    }
    inflateEnd(&z);
    for(uint32_t i=0;i<AVP_JPEG_CRY_TABLE_BYTES;i++)
        if(jagm_read8(&machine,table+i)!=out[i])differences++;
    printf("original Jaguar FILES #239 GZIP level-load QCONV: compressed=%u decoded=%u differing=%u\n",
           sz,AVP_JPEG_CRY_TABLE_BYTES,differences);
    status=differences?1:0;
    if(!status)puts("S39: original cartridge GZIP expansion final 128 KiB QCONV table matched independent DEFLATE test oracle");
done:
    free(compressed);free(out);
    return status;
}
