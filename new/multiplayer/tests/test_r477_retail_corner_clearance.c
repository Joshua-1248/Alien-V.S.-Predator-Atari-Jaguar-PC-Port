/* Compare PC-only lane eligibility against original SafePos on retail maps.
 * No assets in fixture; use the user's ROM. Tests the actual geometric
 * acceptance rule, not a complete live bot navigation session. */
#include "../local_mp_bot.c"
#include "collision.h"
#include "avp_resources.h"
#include "doors.h"
#include <stdio.h>
#include <stdlib.h>
u32 levfx_ID;
int main(int argc,char **argv){
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb"); AvpRomImage rom={0};
    rom.size=4194304u;rom.data=malloc(rom.size);
    if(!f||!rom.data||fread(rom.data,1,rom.size,f)!=rom.size)return 2;
    fclose(f);rom.exact_retail_sha256=1;InitDoors();
    static const unsigned fractions[]={0x0800,0x2000,0x4fff,0x5000,0x8000,0xafff,0xb000,0xe000,0xf800};
    unsigned long checked=0,accepted=0,unsafe=0;
    for(unsigned level=1;level<=15;level++){
        uint8_t *raw=NULL;size_t n=0;
        if(!avp_resources_decode_file(&rom,86u+level,NULL,&raw,&n)||n!=8u+64u*64u*8u)return 2;
        AvpMpBotMap m={raw+8,64,64,fullbits,NULL,10,NULL,NULL,0};
        avp_collision_bind_maze(raw+8,64,64);
        unsigned long before=checked,prev=unsafe;
        for(unsigned y=1;y<63;y++)for(unsigned x=1;x<63;x++){
            if(!m.cells[(y*64+x)*8+5])continue;
            for(unsigned a=0;a<sizeof fractions/sizeof fractions[0];a++)
            for(unsigned b=0;b<sizeof fractions/sizeof fractions[0];b++){
                unsigned px=(x<<16)|fractions[a],py=(y<<16)|fractions[b];
                checked++;
                if(bot_lane_wall_clear(&m,px/65536.0,py/65536.0)){
                    accepted++;
                    if(SafePos(80,(s32)px,(s32)py,40)!=AVP_COLL_SAFE){
                        if(unsafe<10)fprintf(stderr,"unsafe lane level=%u cell=(%u,%u) fraction=(%04x,%04x)\n",level,x,y,fractions[a],fractions[b]);
                        unsafe++;
                    }
                }
            }
        }
        printf("level %u: %lu samples, %lu unsafe accepts\n",level,checked-before,unsafe-prev);free(raw);
    }
    free(rom.data);printf("%lu total samples, %lu accepted, %lu unsafe\n",checked,accepted,unsafe);return !!unsafe;
}
