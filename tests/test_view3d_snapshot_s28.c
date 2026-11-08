#define _POSIX_C_SOURCE 200809L
/* Synthetic fixtures only: do not store or publish authentic cartridge scenes. */
#include "../tools/jagref/view3d_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define TESTFILE "/tmp/avp_s28_synthetic_fixture.avpview"
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"S28 snapshot failure line %d: %s\n",__LINE__,#x);remove(TESTFILE);exit(1);}}while(0)
int main(void){
    JagMachine *a=calloc(1,sizeof *a),*b=calloc(1,sizeof *b);
    REQUIRE(a&&b);
    uint8_t rom[]={1,4,9,16,25,36,49};
    uint64_t fp=avp_view3d_rom_fingerprint(rom,sizeof rom);
    REQUIRE(fp!=0&&fp!=avp_view3d_rom_fingerprint(rom,sizeof rom-1));
    a->frames=2199;a->gpu.pc=0xF03000u;a->gpu.flags=0x4002u;
    a->gpu.reg[0][7]=0x12345678u;a->gpu.reg[1][31]=0x89ABCDEFu;
    a->gpu.remain=0x1357u;a->gpu.acc=-97;
    a->gpu.delay_target=0xF03118u;a->gpu.divctrl=1;
    a->gpu.ram[0]=0xE3;a->gpu.ram[8191]=0xF2;
    a->ram[0]=0xAA;a->ram[0x1FFFFF]=0xBB;a->ram[0x7FB0]=0x34;
    a->blit.a1_flags=0xFFFF0001u;a->blit.a2_pixel=0x80000010u;
    a->blit.patd=0xABCD9876543210FEull;a->blit.dstd=0xFFEE00112233AABBull;
    REQUIRE(avp_view3d_snapshot_write(TESTFILE,a,fp));
    struct stat st;REQUIRE(stat(TESTFILE,&st)==0&&((st.st_mode&0777)==0600));
    REQUIRE(!avp_view3d_snapshot_read(TESTFILE,b,fp+1));
    REQUIRE(avp_view3d_snapshot_read(TESTFILE,b,fp));
    REQUIRE(!memcmp(a->ram,b->ram,sizeof a->ram));
    REQUIRE(!memcmp(a->gpu.ram,b->gpu.ram,sizeof a->gpu.ram));
    REQUIRE(!memcmp(a->gpu.reg,b->gpu.reg,sizeof a->gpu.reg));
    REQUIRE(b->frames==2199&&b->gpu.pc==0xF03000u&&b->gpu.acc==-97);
    REQUIRE(b->gpu.flags==a->gpu.flags&&b->gpu.remain==a->gpu.remain&&b->gpu.delay_target==a->gpu.delay_target);
    REQUIRE(b->blit.patd==a->blit.patd&&b->blit.dstd==a->blit.dstd&&b->blit.a1_flags==a->blit.a1_flags);
    /* Corrupt, append, and truncate: all must be rejected. */
    FILE *f=fopen(TESTFILE,"r+b");REQUIRE(f);
    REQUIRE(fseek(f,120,SEEK_SET)==0);int old=fgetc(f);REQUIRE(old>=0);
    REQUIRE(fseek(f,120,SEEK_SET)==0&&fputc(old^1,f)>=0);REQUIRE(fclose(f)==0);
    REQUIRE(!avp_view3d_snapshot_read(TESTFILE,b,fp));
    REQUIRE(avp_view3d_snapshot_write(TESTFILE,a,fp));
    f=fopen(TESTFILE,"ab");REQUIRE(f&&fputc(0xFF,f)>=0);REQUIRE(fclose(f)==0);
    REQUIRE(!avp_view3d_snapshot_read(TESTFILE,b,fp));
    REQUIRE(avp_view3d_snapshot_write(TESTFILE,a,fp));
    f=fopen(TESTFILE,"r+b");REQUIRE(f);
    REQUIRE(fseek(f,0,SEEK_SET)==0&&fputc('!',f)>=0);REQUIRE(fclose(f)==0);
    REQUIRE(!avp_view3d_snapshot_read(TESTFILE,b,fp));
    REQUIRE(avp_view3d_snapshot_write(TESTFILE,a,fp));
    f=fopen(TESTFILE,"r+b");REQUIRE(f);REQUIRE(ftruncate(fileno(f),200)==0);REQUIRE(fclose(f)==0);
    REQUIRE(!avp_view3d_snapshot_read(TESTFILE,b,fp));
    a->gpu.pc=0xF03004u;REQUIRE(!avp_view3d_snapshot_write(TESTFILE,a,fp));
    REQUIRE(remove(TESTFILE)==0);
    free(a);free(b);
    puts("S28 pointer-free synthetic frame snapshot: PASS");return 0;
}
