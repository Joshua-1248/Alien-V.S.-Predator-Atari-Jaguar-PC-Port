#include "pc/pc_audio_layout.h"
#include <stddef.h>
/* Generic speaker counts; no proprietary multichannel codec is used. */
static const unsigned bed[]={1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,13u,22u};
static const char *const labels[]={"Mono","Stereo","3.0","Quadraphonic 4.0",
    "5.0 Surround","6.0 Surround","7.0 Surround","Octophonic 8.0",
    "9.0 Surround","10.0 Height","11.0 Height","13.0 Height",
    "22.0 Large Array"};
unsigned avp_audio_layout_bed(unsigned m){return bed[m<AVP_AUDIO_LAYOUT_COUNT?m:1u];}
unsigned avp_audio_layout_channels(unsigned m,unsigned subs){
    unsigned n=avp_audio_layout_bed(m),s=subs>2u?2u:subs;
    return n+s<=AVP_AUDIO_MAX_CHANNELS?n+s:AVP_AUDIO_MAX_CHANNELS;
}
const char *avp_audio_layout_name(unsigned m){return labels[m<AVP_AUDIO_LAYOUT_COUNT?m:1u];}
static int16_t sat16(int32_t v){return (int16_t)(v>32767?32767:v<(-32768)?(-32768):v);}
/* Main channel ordering for standard PCM output:
 * mono C; stereo FL/FR; 3+ FL/FR/FC; quad FL/FR/BL/BR;
 * 5 FL/FR/FC/BL/BR; 6 adds BC; 7 adds SL/SR;
 * 8 adds BC and SL/SR. LFE follows FL/FR/FC if present,
 * or FL/FR in quad, matching standard WAVEFORMATEXTENSIBLE order.
 * Extended height layouts use generic upper-layer fills. */
void avp_audio_layout_convert(const int16_t *in,int16_t *out,unsigned frames,
                              unsigned mode,unsigned subs,AvpAudioBassState *bass)
{
    unsigned beds=avp_audio_layout_bed(mode),s=subs>2u?2u:subs;
    unsigned total=avp_audio_layout_channels(mode,s),insert=(beds==1u?1u:
                                            (beds==2u||beds==4u?2u:3u));
    unsigned f,c;
    AvpAudioBassState local={0,0};
    if(!in||!out)return;
    if(!bass)bass=&local;
    for(f=0;f<frames;f++){
        int32_t l=in[f*2u],r=in[f*2u+1u],mono=(l+r)/2;
        int16_t *dst=out+(size_t)f*total;
        int16_t main[22],lfe[2]={0,0};
        for(c=0u;c<beds;c++){
            int32_t x;
            if(beds==1u)x=mono;
            else if(c==0u)x=l;
            else if(c==1u)x=r;
            else if(beds==4u)x=c==2u?l/3:r/3;
            else if(c==2u)x=mono/2;
            else if(c==3u)x=l/3;
            else if(c==4u)x=r/3;
            else if(beds==6u&&c==5u)x=mono/3; /* rear center */
            else if(beds==8u&&c==5u)x=mono/3; /* rear center */
            else if(c==5u||c==6u)x=(c==5u?l:r)/3;
            else if(beds==8u&&c==7u)x=r/3;
            else x=(c&1u)?r/5:l/5; /* height and extra channels */
            main[c]=sat16(x);
        }
        /* A conservative ~100 Hz low-pass LFE feed derived from the
         * original stereo PCM. Two LFEs permit independent L/R bass. */
        if(l==0&&r==0){bass->left=bass->right=0;}
        else{
            bass->left+=(l-bass->left)/32;
            bass->right+=(r-bass->right)/32;
        }
        if(s>=1u)lfe[0]=sat16((bass->left+bass->right)/4);
        if(s>=2u)lfe[1]=sat16(bass->right/2);
        for(c=0u;c<total;c++){
            if(c<insert)dst[c]=main[c];
            else if(c<insert+s)dst[c]=lfe[c-insert];
            else dst[c]=main[c-s];
        }
    }
}
