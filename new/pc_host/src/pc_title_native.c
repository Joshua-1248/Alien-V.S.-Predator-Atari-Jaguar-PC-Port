/* R82: original cartridge 68000/GPU/Blitter/OP title scanout, presentation
 * only. This unit does not modify the reconstructed gameplay or distribute
 * any game bytes. Musashi licence is recorded in THIRD_PARTY_NOTICES.md. */
#include "pc/pc_title_native.h"
#include <stdlib.h>
#include <string.h>
#ifdef AVP_PC_NATIVE_TITLE_JAGM
#include "jaguar/jag_machine.h"
#include "m68k.h"
#endif

struct AvpPcTitleNative {
#ifdef AVP_PC_NATIVE_TITLE_JAGM
    JagMachine *machine;
    /* Immutable field-313 checkpoint for instantaneous source-exact title
     * replay. The original 68000 interpreter is process-global; its state
     * must be restored alongside RAM/GPU/TOM/JERRY and the OP buffers. */
    JagMachine *title_checkpoint;
    void *cpu_checkpoint;
#else
    unsigned unused;
#endif
    int active;
};

int avp_pc_title_native_copy_rgb(const uint32_t *source,unsigned width,
    unsigned height,unsigned stride,AvpJaguarFramebuffer *dest)
{
    unsigned y;
    if(!source||!dest||!dest->pixels||width<320u||height<240u||
       stride<width||dest->width<320u||dest->height<254u)return 0;
    memset(dest->pixels,0,(size_t)dest->width*dest->height*sizeof(uint32_t));
    for(y=0u;y<240u;y++){
        uint32_t *out=dest->pixels+(size_t)(y+14u)*dest->width;
        const uint32_t *in=source+(size_t)y*stride;
        memcpy(out,in,320u*sizeof(uint32_t));
    }
    return 1;
}

AvpPcTitleNative *avp_pc_title_native_create(const AvpRomImage *rom)
{
#ifdef AVP_PC_NATIVE_TITLE_JAGM
    AvpPcTitleNative *t;
    if(!rom||!rom->exact_retail_sha256||!rom->data||rom->size!=4194304u)return NULL;
    t=(AvpPcTitleNative*)calloc(1,sizeof(*t));
    if(!t)return NULL;
    t->machine=(JagMachine*)malloc(sizeof(*t->machine));
    if(!t->machine){free(t);return NULL;}
    if(jagm_init(t->machine,rom->data,rom->size)!=0){free(t->machine);free(t);return NULL;}
    t->active=1;
    return t;
#else
    (void)rom;
    return NULL;
#endif
}

void avp_pc_title_native_stop(AvpPcTitleNative *title)
{
    if(title)title->active=0;
}

void avp_pc_title_native_destroy(AvpPcTitleNative *title)
{
    if(!title)return;
#ifdef AVP_PC_NATIVE_TITLE_JAGM
    if(title->machine){free(title->machine->audio);free(title->machine);}
    free(title->title_checkpoint);
    free(title->cpu_checkpoint);
#endif
    free(title);
}

void avp_pc_title_native_advance_boot(AvpPcTitleNative *title)
{
#ifdef AVP_PC_NATIVE_TITLE_JAGM
    if(!title||!title->active||!title->machine)return;
    if(title->machine->frames<313u){
        jagm_set_pad(title->machine,0u,0u);
        jagm_run_frames(title->machine,1u);
        if(title->machine->halted)title->active=0;
        if(title->active && title->machine->frames==313u &&
           !title->title_checkpoint && !title->cpu_checkpoint){
            const unsigned n=m68k_context_size();
            JagMachine *checkpoint=(JagMachine*)malloc(sizeof(*checkpoint));
            void *cpu=malloc(n);
            if(checkpoint && cpu){
                memcpy(checkpoint,title->machine,sizeof(*checkpoint));
                (void)m68k_get_context(cpu);
                title->title_checkpoint=checkpoint;
                title->cpu_checkpoint=cpu;
            }else{free(checkpoint);free(cpu);}
        }
    }
#else
    (void)title;
#endif
}

/* Restore precisely the RAM/68k/GPU/OP state that produced original field
 * 314, instead of simulating 313 power-on fields (~20 seconds on some CPUs).
 * Keeping the SAME machine address preserves the reference bus ctx pointers. */
int avp_pc_title_native_rewind(AvpPcTitleNative *title)
{
#ifdef AVP_PC_NATIVE_TITLE_JAGM
    if(!title||!title->machine||!title->title_checkpoint||
       !title->cpu_checkpoint)return 0;
    if(title->machine->audio &&
       title->machine->audio!=title->title_checkpoint->audio)
        free(title->machine->audio);
    memcpy(title->machine,title->title_checkpoint,sizeof(*title->machine));
    m68k_set_context(title->cpu_checkpoint);
    title->active=1;
    return 1;
#else
    (void)title;
    return 0;
#endif
}

int avp_pc_title_native_present(AvpPcTitleNative *title,
    AvpJaguarFramebuffer *fb,unsigned title_tick)
{
#ifdef AVP_PC_NATIVE_TITLE_JAGM
    uint64_t target;
    if(!title||!title->active||!title->machine||!fb)return 0;
    if(title_tick>492u){title->active=0;return 0;}
    /* At the first PC title field, 313 initial ROM fields have elapsed.
     * Don't let a Fire-induced jump to a later scene stall the UI while
     * emulating hundreds of unpresented Jaguar fields. Host fallback wins. */
    target=314u+(uint64_t)title_tick;
    if(target<title->machine->frames || target>title->machine->frames+2u){
        title->active=0;return 0;
    }
    if(target>title->machine->frames)
        jagm_run_frames(title->machine,(unsigned)(target-title->machine->frames));
    if(title->machine->halted){title->active=0;return 0;}
    return avp_pc_title_native_copy_rgb(title->machine->frame,
        title->machine->frame_w,title->machine->frame_h,JAGM_MAX_W,fb);
#else
    (void)title;(void)fb;(void)title_tick;
    return 0;
#endif
}
