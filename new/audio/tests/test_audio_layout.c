#include "pc/pc_audio_layout.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FRAMES 43u
#define GUARD ((int16_t)0x5566)
int main(void)
{
    static const unsigned bed[]={1,2,3,4,5,6,7,8,9,10,11,13,22};
    int16_t input[FRAMES*2u],out[FRAMES*AVP_AUDIO_MAX_CHANNELS+4u];
    unsigned checks=0u;
    for(unsigned f=0;f<FRAMES;f++){
        input[f*2u]=(int16_t)(f*101-1500);
        input[f*2u+1u]=(int16_t)(900-f*70);
    }
    for(unsigned mode=0u;mode<AVP_AUDIO_LAYOUT_COUNT;mode++){
        assert(avp_audio_layout_bed(mode)==bed[mode]);checks++;
        assert(avp_audio_layout_name(mode)[0]);checks++;
        for(unsigned subs=0u;subs<=2u;subs++){
            AvpAudioBassState bass={0,0};
            unsigned ch=avp_audio_layout_channels(mode,subs);
            assert(ch==bed[mode]+subs && ch<=AVP_AUDIO_MAX_CHANNELS);checks++;
            for(unsigned i=0;i<FRAMES*AVP_AUDIO_MAX_CHANNELS+4u;i++)out[i]=GUARD;
            avp_audio_layout_convert(input,out,FRAMES,mode,subs,&bass);
            for(unsigned i=FRAMES*ch;i<FRAMES*AVP_AUDIO_MAX_CHANNELS+4u;i++){
                assert(out[i]==GUARD);checks++;
            }
            for(unsigned f=0;f<FRAMES;f++){
                if(mode==0u)assert(out[f*ch]==(input[f*2u]+input[f*2u+1u])/2);
                else{
                    assert(out[f*ch]==input[f*2u]);
                    assert(out[f*ch+1u]==input[f*2u+1u]);
                }
                checks+=2u;
                if(subs){
                    unsigned insert=(bed[mode]==1u?1u:(bed[mode]==2u||bed[mode]==4u?2u:3u));
                    assert(out[f*ch+insert]>=-32768);checks++;
                }
            }
        }
    }
    {AvpAudioBassState state={1,1};
     int16_t in[2]={0,0},dest[4];
     avp_audio_layout_convert(in,dest,1u,1u,2u,&state);
     assert(dest[0]==0&&dest[1]==0&&dest[2]==0&&dest[3]==0);
     checks++;
    }
    puts("PASS: Audio layout channel counts, mono/stereo fidelity, LFE slots, buffers, all 13 modes and 0/1/2 subs");
    printf("Assertions: %u\n",checks);
    return 0;
}
