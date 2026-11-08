#define _POSIX_C_SOURCE 200809L
/* AVP RE24 S28: pointer-free, little-endian GPU entry-state recording.
 * PRIVATE snapshots are restricted to use with an independently supplied
 * cartridge and are excluded from the public release and source archives.
 * This captures the 3D GPU path only. It is NOT a full Jaguar save-state:
 * there is no 68000, DSP, audio, OP framebuffer or cycle-accurate timeline.
 */
#include "view3d_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PAYLOAD (JAGM_RAM_SIZE+8192u+8192u)
#define VERSION 1u
#define VIEW_ENTRY 0xF03000u
static const uint8_t magic[8]={'A','V','P','V','I','E','W','1'};
static const uint8_t jpeg_magic[8]={'A','V','P','J','P','E','G','1'};
typedef struct Buffer {uint8_t *p;size_t cap,pos;int failed;} Buffer;
static void w8(Buffer *b,uint8_t v){if(b->pos>=b->cap){b->failed=1;return;}b->p[b->pos++]=v;}
static void w32(Buffer *b,uint32_t v){for(int i=0;i<4;i++)w8(b,(uint8_t)(v>>(8*i)));}
static void w64(Buffer *b,uint64_t v){for(int i=0;i<8;i++)w8(b,(uint8_t)(v>>(8*i)));}
static uint8_t r8(Buffer *b){if(b->pos>=b->cap){b->failed=1;return 0;}return b->p[b->pos++];}
static uint32_t r32(Buffer *b){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)r8(b)<<(8*i);return v;}
static uint64_t r64(Buffer *b){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)r8(b)<<(8*i);return v;}
static void wbytes(Buffer *b,const void *p,size_t n){if(n>b->cap-b->pos){b->failed=1;return;}memcpy(b->p+b->pos,p,n);b->pos+=n;}
static void rbytes(Buffer *b,void *p,size_t n){if(n>b->cap-b->pos){b->failed=1;return;}memcpy(p,b->p+b->pos,n);b->pos+=n;}
static uint32_t checksum(const uint8_t *p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
uint64_t avp_view3d_rom_fingerprint(const uint8_t *rom,size_t size){uint64_t h=0xcbf29ce484222325ull;for(size_t i=0;i<size;i++){h^=rom[i];h*=0x100000001b3ull;}return h;}
/* Write/read deliberately exclude bus callbacks, host pointers, counters and
 * non-GPU subsystems. The same ordering provides a stable serialized format. */
static void encode(Buffer *b,const JagMachine *m){
    const JRisc *g=&m->gpu;const JagBlitter *x=&m->blit;
    w64(b,m->frames);wbytes(b,m->ram,JAGM_RAM_SIZE);wbytes(b,g->ram,sizeof g->ram);
    for(unsigned k=0;k<2;k++)for(unsigned r=0;r<32;r++)w32(b,g->reg[k][r]);
#define GW(f) w32(b,g->f)
    GW(pc);GW(flags);GW(latches);GW(go);GW(ctrl_misc);GW(mtxc);GW(mtxa);GW(bigend);GW(hidata);GW(remain);GW(divctrl);GW(modulo);
    w64(b,(uint64_t)g->acc);
    GW(mac_active);GW(delay_pending);GW(delay_target);GW(last_pc);GW(cpu_irq);GW(fault);GW(fault_pc);
    GW(preceding_external_read);GW(gpu_hidata_loaded);GW(flags_nops_pending);GW(executing);GW(last_was_div);GW(last_div_dst);
#undef GW
#define BW(f) w32(b,x->f)
    BW(a1_base);BW(a1_flags);BW(a1_clip);BW(a1_pixel);BW(a1_step);BW(a1_fstep);BW(a1_fpixel);BW(a1_inc);BW(a1_finc);
    BW(a2_base);BW(a2_flags);BW(a2_mask);BW(a2_pixel);BW(a2_step);BW(cmd);BW(count);
    w64(b,x->srcd);w64(b,x->dstd);w64(b,x->dstz);w64(b,x->srcz1);w64(b,x->srcz2);w64(b,x->patd);
    BW(iinc);BW(zinc);BW(stop);BW(stopped);
#undef BW
    /* S28 check deliberately captures the subset required by stages 0..2. */
}
static void decode(Buffer *b,JagMachine *m){
    JRisc *g=&m->gpu;JagBlitter *x=&m->blit;
    m->frames=r64(b);rbytes(b,m->ram,JAGM_RAM_SIZE);rbytes(b,g->ram,sizeof g->ram);
    for(unsigned k=0;k<2;k++)for(unsigned r=0;r<32;r++)g->reg[k][r]=r32(b);
#define GR(f) g->f=r32(b)
    GR(pc);GR(flags);GR(latches);GR(go);GR(ctrl_misc);GR(mtxc);GR(mtxa);GR(bigend);GR(hidata);GR(remain);GR(divctrl);GR(modulo);
    g->acc=(int64_t)r64(b);
    GR(mac_active);GR(delay_pending);GR(delay_target);GR(last_pc);GR(cpu_irq);GR(fault);GR(fault_pc);
    GR(preceding_external_read);GR(gpu_hidata_loaded);GR(flags_nops_pending);GR(executing);GR(last_was_div);GR(last_div_dst);
#undef GR
#define BR(f) x->f=r32(b)
    BR(a1_base);BR(a1_flags);BR(a1_clip);BR(a1_pixel);BR(a1_step);BR(a1_fstep);BR(a1_fpixel);BR(a1_inc);BR(a1_finc);
    BR(a2_base);BR(a2_flags);BR(a2_mask);BR(a2_pixel);BR(a2_step);BR(cmd);BR(count);
    x->srcd=r64(b);x->dstd=r64(b);x->dstz=r64(b);x->srcz1=r64(b);x->srcz2=r64(b);x->patd=r64(b);
    BR(iinc);BR(zinc);BR(stop);BR(stopped);
#undef BR
}
static int write_core(const char *path,const JagMachine *m,uint64_t rom_fingerprint,
                      const uint8_t file_magic[8],uint32_t entry,int exclusive){
    if(!path||!m||m->gpu.pc!=entry||!rom_fingerprint)return 0;
    Buffer b={0};b.cap=MAX_PAYLOAD;b.p=malloc(b.cap);if(!b.p)return 0;
    encode(&b,m);
    int ok=!b.failed;FILE *f=NULL;
    if(ok){
        /* Restrict private cartridge-derived memory images to their owner. */
        int fd=open(path,O_WRONLY|O_CREAT|(exclusive?O_EXCL:O_TRUNC),0600);
        if(fd<0)ok=0;
        else if(fchmod(fd,0600)!=0){close(fd);ok=0;}
        else {f=fdopen(fd,"wb");if(!f){close(fd);ok=0;}}
    }
    if(ok){uint8_t header[28];Buffer h={header,sizeof header,0,0};
        wbytes(&h,file_magic,8);w32(&h,VERSION);w32(&h,(uint32_t)b.pos);
        w64(&h,rom_fingerprint);w32(&h,checksum(b.p,b.pos));
        ok=!h.failed&&h.pos==sizeof header&&fwrite(header,1,sizeof header,f)==sizeof header
           &&fwrite(b.p,1,b.pos,f)==b.pos;
    }
    if(f){if(fclose(f))ok=0;if(!ok)remove(path);}
    free(b.p);return ok;
}
static int read_core(const char *path,JagMachine *m,uint64_t rom_fingerprint,
                     const uint8_t file_magic[8],uint32_t entry){
    if(!path||!m||!rom_fingerprint)return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;
    uint8_t header[28];int ok=fread(header,1,sizeof header,f)==sizeof header;
    Buffer h={header,sizeof header,0,0};uint8_t got[8]={0};
    if(ok){rbytes(&h,got,8);uint32_t version=r32(&h),len=r32(&h);uint64_t fp=r64(&h);uint32_t hash=r32(&h);
        ok=!h.failed&&!memcmp(got,file_magic,8)&&version==VERSION&&fp==rom_fingerprint
            &&len>=JAGM_RAM_SIZE+8192u&&len<=MAX_PAYLOAD;
        if(ok){Buffer b={0};b.cap=len;b.p=malloc(len);ok=b.p!=NULL;
            if(ok)ok=fread(b.p,1,len,f)==len&&fgetc(f)==EOF&&checksum(b.p,len)==hash;
            if(ok){
                /* Fail-closed: reject wrong-entry files and decode errors
                 * without partially modifying the caller's machine. */
                JagMachine *tmp=malloc(sizeof *tmp);
                if(!tmp)ok=0;
                else {
                    memcpy(tmp,m,sizeof *tmp);
                    decode(&b,tmp);
                    ok=!b.failed&&b.pos==len&&tmp->gpu.pc==entry;
                    if(ok)memcpy(m,tmp,sizeof *m);
                    free(tmp);
                }
            }
            free(b.p);
        }
    }
    if(fclose(f))ok=0;
    return ok;
}

/* RE24-S29: same pointer-free, checksummed GPU-entry payload, with a
 * deliberately separate magic and restricted GPU PC for JagPEG jobs. These
 * images contain retail-derived writable RAM and must remain PRIVATE. */
int avp_view3d_snapshot_write(const char *path,const JagMachine *m,uint64_t fp){
    return write_core(path,m,fp,magic,VIEW_ENTRY,0);
}
int avp_view3d_snapshot_read(const char *path,JagMachine *m,uint64_t fp){
    return read_core(path,m,fp,magic,VIEW_ENTRY);
}
int avp_jagpeg_snapshot_write(const char *path,const JagMachine *m,uint64_t fp,uint32_t entry){
    if(entry!=0xF03000u && entry!=0xF03A00u)return 0;
    return write_core(path,m,fp,jpeg_magic,entry,1);
}
int avp_jagpeg_snapshot_read(const char *path,JagMachine *m,uint64_t fp,uint32_t entry){
    if(entry!=0xF03000u && entry!=0xF03A00u)return 0;
    return read_core(path,m,fp,jpeg_magic,entry);
}
