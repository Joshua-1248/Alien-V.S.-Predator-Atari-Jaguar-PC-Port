#define _POSIX_C_SOURCE 200809L
/* Synthetic CPU-independent capture format: no cartridge bytes. */
#include "../tools/jagref/view3d_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define FILE1 "/tmp/avp_s29_synthetic.avpjpeg"
#define T(x) do{if(!(x)){fprintf(stderr,"S29 fixture: line %d: %s\n",__LINE__,#x);remove(FILE1);exit(1);}}while(0)
int main(void){
    JagMachine *a=calloc(1,sizeof *a),*b=calloc(1,sizeof *b);
    uint8_t rom[]={2,3,5,7,11,13};uint64_t fp=avp_view3d_rom_fingerprint(rom,sizeof rom);
    struct stat st;FILE *f;int c;
    T(a&&b&&fp);
    a->frames=1660;a->gpu.pc=0xF03000u;a->gpu.flags=0x4040u;
    a->gpu.reg[1][11]=0x11223344u;a->gpu.ram[0]=0x7C;a->gpu.ram[8191]=0x80;
    a->ram[0xFFFF]=0x88;a->ram[0x1FFFFF]=0x99;
    a->blit.a1_pixel=0x11220033u;a->blit.srcd=0xDEADBEEF01234567ull;
    T(!avp_jagpeg_snapshot_write(FILE1,a,fp,0xF03004u));
    T(avp_jagpeg_snapshot_write(FILE1,a,fp,0xF03000u));
    T(!avp_jagpeg_snapshot_write(FILE1,a,fp,0xF03000u));/* no overwrite */
    T(stat(FILE1,&st)==0&&(st.st_mode&0777)==0600);
    T(!avp_jagpeg_snapshot_read(FILE1,b,fp+1,0xF03000u));
    T(!avp_jagpeg_snapshot_read(FILE1,b,fp,0xF03A00u));
    T(!avp_view3d_snapshot_read(FILE1,b,fp)); /* separate magic */
    T(avp_jagpeg_snapshot_read(FILE1,b,fp,0xF03000u));
    T(memcmp(a->ram,b->ram,sizeof a->ram)==0);
    T(memcmp(a->gpu.ram,b->gpu.ram,sizeof a->gpu.ram)==0);
    T(memcmp(a->gpu.reg,b->gpu.reg,sizeof a->gpu.reg)==0);
    T(b->gpu.pc==a->gpu.pc&&b->frames==a->frames&&b->blit.srcd==a->blit.srcd);
    f=fopen(FILE1,"r+b");T(f);T(fseek(f,100,SEEK_SET)==0);c=fgetc(f);T(c!=EOF);
    T(fseek(f,100,SEEK_SET)==0&&fputc(c^1,f)!=EOF);T(fclose(f)==0);
    T(!avp_jagpeg_snapshot_read(FILE1,b,fp,0xF03000u));
    T(remove(FILE1)==0);
    a->gpu.pc=0xF03A00u;
    T(avp_jagpeg_snapshot_write(FILE1,a,fp,0xF03A00u));
    T(!avp_jagpeg_snapshot_read(FILE1,b,fp,0xF03000u));
    T(avp_jagpeg_snapshot_read(FILE1,b,fp,0xF03A00u));
    T(remove(FILE1)==0);
    free(a);free(b);puts("S29 JagPEG private GPU-entry snapshots: PASS");return 0;
}
