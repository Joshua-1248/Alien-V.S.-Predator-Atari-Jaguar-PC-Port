/* Rewind the SAME native Jaguar title interpreter and compare replay fields.
 * User-owned ROM is read only; no copyrighted pixels enter the executable. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "pc/pc_title_native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    AvpRomImage rom={0};AvpPcTitleNative *t;
    AvpJaguarFramebuffer fb={320u,254u,NULL};
    u32 *before; FILE *f;
    if(argc!=2){puts("SKIP: original retail Jaguar ROM required");return 0;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    rom.size=4194304u;rom.data=(u8*)malloc(rom.size);
    assert(rom.data&&fread(rom.data,1,rom.size,f)==rom.size);fclose(f);
    rom.exact_retail_sha256=1;
    fb.pixels=(u32*)calloc((size_t)320*254,sizeof(u32));
    before=(u32*)malloc((size_t)320*254*sizeof(u32));
    assert(fb.pixels&&before);
    t=avp_pc_title_native_create(&rom);assert(t);
    for(unsigned i=0;i<313u;i++)avp_pc_title_native_advance_boot(t);
    for(unsigned i=0;i<=36u;i++)assert(avp_pc_title_native_present(t,&fb,i));
    memcpy(before,fb.pixels,(size_t)320*254*sizeof(u32));
    avp_pc_title_native_stop(t);
    assert(avp_pc_title_native_rewind(t)==1);
    for(unsigned i=0;i<=36u;i++)assert(avp_pc_title_native_present(t,&fb,i));
    assert(memcmp(before,fb.pixels,(size_t)320*254*sizeof(u32))==0);
    avp_pc_title_native_destroy(t);
    free(before);free(fb.pixels);free(rom.data);
    puts("R91: native Jaguar title replay pixel-identical after rewind: PASS");
    return 0;
}
