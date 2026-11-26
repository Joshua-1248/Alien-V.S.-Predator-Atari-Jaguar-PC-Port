#ifndef AVP_MICRO_CRY_RMW_FAST_H
#define AVP_MICRO_CRY_RMW_FAST_H
#include "jaguar/cry.h"
/* Exact original QCONV reciprocal, indexed by the maximum RGB component.
 * A static table removes the per-pixel integer division. The renderer's
 * existing immutable CRY-to-RGB table supplies the identical output.
 * QCONV itself is read live; no cache depends on a ROM pointer. */
static const u16 avp_micro_qconv_recip[256]={
    0,65280,32640,21760,16320,13056,10880,9325,8160,7253,6528,5934,5440,5021,4662,4352,
    4080,3840,3626,3435,3264,3108,2967,2838,2720,2611,2510,2417,2331,2251,2176,2105,
    2040,1978,1920,1865,1813,1764,1717,1673,1632,1592,1554,1518,1483,1450,1419,1388,
    1360,1332,1305,1280,1255,1231,1208,1186,1165,1145,1125,1106,1088,1070,1052,1036,
    1020,1004,989,974,960,946,932,919,906,894,882,870,858,847,836,826,
    816,805,796,786,777,768,759,750,741,733,725,717,709,701,694,687,
    680,672,666,659,652,646,640,633,627,621,615,610,604,598,593,588,
    582,577,572,567,562,557,553,548,544,539,535,530,526,522,518,514,
    510,506,502,498,494,490,487,483,480,476,473,469,466,462,459,456,
    453,450,447,444,441,438,435,432,429,426,423,421,418,415,413,410,
    408,405,402,400,398,395,393,390,388,386,384,381,379,377,375,373,
    370,368,366,364,362,360,358,356,354,352,350,349,347,345,343,341,
    340,338,336,334,333,331,329,328,326,324,323,321,320,318,316,315,
    313,312,310,309,307,306,305,303,302,300,299,298,296,295,294,292,
    291,290,288,287,286,285,283,282,281,280,278,277,276,275,274,273,
    272,270,269,268,267,266,265,264,263,262,261,260,259,258,257,256,
};
static inline unsigned avp_micro_sat(int value,int max)
{
    return (unsigned)(value<0?0:(value>max?max:value));
}
static inline u32 avp_micro_cry_rmw(u32 dst,u16 src,const u8 *q,size_t n,
                                    const u32 rgb[65536])
{
    if(!src || !q || n<131072u)return dst;
    const unsigned r=(dst>>16)&255u,g=(dst>>8)&255u,b=dst&255u;
    const unsigned y=r>g?(r>b?r:b):(g>b?g:b);
    unsigned c=0;
    if(y){
        const unsigned scale=avp_micro_qconv_recip[y];
        if(r==y)c=q[(((g*scale)>>8)<<8)|((b*scale)>>8)];
        else if(g==y)c=q[0x10000u|(((b*scale)>>8)<<8)|((r*scale)>>8)];
        else c=q[(((g*scale)>>8)<<8)|((r*scale)>>8)]^0xf0u;
    }
    const unsigned s1=(src>>8)&15u,s2=src>>12;
    const int dc1=(int)(s1^8u)-8,dc2=(int)(s2^8u)-8;
    const int dy=(int)((src&255u)^128u)-128;
    const unsigned out=(avp_micro_sat((int)(c>>4)+dc2,15)<<12)|
        (avp_micro_sat((int)(c&15u)+dc1,15)<<8)|avp_micro_sat((int)y+dy,255);
    return rgb[out];
}
#endif
