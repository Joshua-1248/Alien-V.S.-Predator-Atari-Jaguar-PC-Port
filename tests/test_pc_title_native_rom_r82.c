/* Optional ROM-backed R82 title bridge test. No proprietary data in source.
 * Usage: test_pc_title_native_rom_r82 /private/path/AVP.jag
 * Without a ROM argument the test reports SKIP and returns success. */
#include "pc/pc_title_native.h"
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const unsigned sample_fields[]={317u,330u,349u,360u,386u,396u,402u,432u,
                                     462u,486u,488u,506u,539u};
int main(int argc,char **argv)
{
    AvpRomImage rom={0};AvpPcTitleNative *title;JagMachine *reference;
    AvpJaguarFramebuffer fb={320u,254u,NULL};
    unsigned next=0,checks=0;
    uint32_t *expected=NULL;
    FILE *f;
    if(argc<2){puts("SKIP: provide a local retail ROM path for exact frame test");return 0;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    rom.data=(u8*)malloc(4194304u);rom.size=4194304u;
    if(!rom.data||fread(rom.data,1,rom.size,f)!=rom.size){fclose(f);return 2;}
    fclose(f);
    /* User-provided exact retail SHA-256 checked externally by test runner. */
    rom.exact_retail_sha256=1;
    reference=(JagMachine*)malloc(sizeof(*reference));
    fb.pixels=(u32*)calloc((size_t)fb.width*fb.height,sizeof(u32));
    expected=(uint32_t*)malloc((sizeof(sample_fields)/sizeof(sample_fields[0]))*76800u*sizeof(uint32_t));
    assert(reference&&fb.pixels&&expected);
    assert(jagm_init(reference,rom.data,rom.size)==0);
    /* Musashi owns static CPU registers: NEVER interleave independent
     * JagMachine instances. Capture once, reinitialize for playback. */
    for(unsigned field=1u;field<=806u;field++){
        jagm_run_frames(reference,1u);
        if(next<sizeof(sample_fields)/sizeof(sample_fields[0]) && field==sample_fields[next]){
            assert(reference->frame_w>=320u&&reference->frame_h>=240u);
            for(unsigned y=0;y<240u;y++)
                memcpy(expected+next*76800u+y*320u,
                       reference->frame+(size_t)y*JAGM_MAX_W,320u*sizeof(uint32_t));
            ++next;
        }
    }
    assert(next==sizeof(sample_fields)/sizeof(sample_fields[0]));
    free(reference->audio);free(reference);reference=NULL;
    title=avp_pc_title_native_create(&rom);
    assert(title);
    for(unsigned fno=0;fno<313u;fno++) avp_pc_title_native_advance_boot(title);
    next=0u;
    for(unsigned tick=0;tick<493u;tick++){
        const unsigned field=314u+tick;
        assert(avp_pc_title_native_present(title,&fb,tick)==1);
        if(next<sizeof(sample_fields)/sizeof(sample_fields[0]) && field==sample_fields[next]){
            for(unsigned y=0;y<240u;y++)for(unsigned x=0;x<320u;x++){
                assert(fb.pixels[(size_t)(y+14u)*320u+x] ==
                       expected[next*76800u+y*320u+x]);
                ++checks;
            }
            ++next;
        }
    }
    assert(checks==(unsigned)(sizeof(sample_fields)/sizeof(sample_fields[0]))*76800u);
    assert(avp_pc_title_native_present(title,&fb,493u)==0);
    avp_pc_title_native_destroy(title);
    free(expected);
    free(fb.pixels);free(rom.data);
    printf("R82 original-ROM title scanout: %u/%u pixels match across %u fields\n",
           checks,checks,next);
    return 0;
}
