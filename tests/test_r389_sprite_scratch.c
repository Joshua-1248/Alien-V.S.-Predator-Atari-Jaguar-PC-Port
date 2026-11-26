#include "jaguar/avp_gpu/sprite_scratch.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"R389 scratch FAIL line %d: %s\n",__LINE__,#e);return 1;}}while(0)
static unsigned uintstate=0x839be0a9u;
static unsigned rnd(void){uintstate^=uintstate<<13;uintstate^=uintstate>>17;uintstate^=uintstate<<5;return uintstate;}
int main(void)
{
    AvpGpuSpriteScratch viewers[4]={{0}};
    uint64_t verified=0,retained=0;
    CHECK(!avp_gpu_sprite_scratch_prepare(&viewers[0],0u));
    CHECK(!avp_gpu_sprite_scratch_prepare(&viewers[0],256u*256u+1u));
    for(unsigned frame=0;frame<1500u;frame++){
        const unsigned vi=frame&3u;
        AvpGpuSpriteScratch *s=&viewers[vi];
        size_t n=1u+(size_t)(rnd()%65536u);
        uint8_t *old_rgb=s->rgb;
        size_t old_capacity=s->capacity;
        CHECK(avp_gpu_sprite_scratch_prepare(s,n));
        CHECK(s->rgb&&s->cry16&&s->alpha && s->capacity>=n);
        if(old_capacity>=n){CHECK(s->rgb==old_rgb);retained++;}
        for(size_t i=0;i<n;i++){
            CHECK(s->rgb[i*3u]==0u && s->rgb[i*3u+1u]==0u && s->rgb[i*3u+2u]==0u);
            CHECK(s->cry16[i]==0u && s->alpha[i]==0u);
            s->rgb[i*3u]=(uint8_t)(i^frame);s->rgb[i*3u+1u]=(uint8_t)(i>>4u);
            s->rgb[i*3u+2u]=(uint8_t)(i>>7u);
            s->cry16[i]=(uint16_t)(i^frame);s->alpha[i]=(uint8_t)(frame+i);
        }
        verified+=n;
    }
    /* A borrowed multiplayer viewport must detach rather than free its
     * parent's scratch pointers. */
    AvpGpuSpriteScratch borrowed=viewers[0];
    avp_gpu_sprite_scratch_detach(&borrowed);
    CHECK(borrowed.rgb==NULL && borrowed.cry16==NULL && borrowed.alpha==NULL && borrowed.capacity==0u);
    CHECK(viewers[0].rgb!=NULL);
    CHECK(avp_gpu_sprite_scratch_prepare(&borrowed,32u));
    avp_gpu_sprite_scratch_destroy(&borrowed);
    for(unsigned i=0;i<4u;i++){
        avp_gpu_sprite_scratch_destroy(&viewers[i]);
        CHECK(viewers[i].rgb==NULL && viewers[i].cry16==NULL &&
              viewers[i].alpha==NULL && viewers[i].capacity==0u);
    }
    printf("R389 sprite scratch: %llu zero-checked texels, %llu retained allocations, viewport ownership PASS\n",
        (unsigned long long)verified,(unsigned long long)retained);
    return 0;
}
