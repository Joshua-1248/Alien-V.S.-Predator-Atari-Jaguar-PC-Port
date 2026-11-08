/* R277: retail Check_EE ($0201E4) is EORI.L #$1234FAC3; retail Init_High
 * defaults ($2A954) store 600000 ($000927C0) as the third score.  Saves made
 * by earlier PC builds (key $1234FAC2) must still load (not be wiped). */
#include "eeprom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u16 ee[64];
static u16 rd(u16 i){return ee[i];}
static void wr(u16 i,u16 v){ee[i]=v;}
static u32 be32(const u8*p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static void put32(u16 *w,unsigned l,u32 v){w[l*2]=(u16)(v>>16);w[l*2+1]=(u16)v;}
int main(void){
    u32 sum=0;
    avp_eeprom_bind(rd,wr,NULL);
    /* A genuine retail cartridge image: arbitrary data + retail key. */
    memset(ee,0,sizeof(ee));
    for(unsigned l=0;l<31;l++){put32(ee,l,0x01010101u*l);sum+=0x01010101u*l;}
    put32(ee,31,sum^0x1234fac3u);
    Init_EE();
    assert(be32(cartcopy+4)==0x01010101u);           /* accepted, not defaulted */
    assert(Check_EE()==(sum^0x1234fac3u));
    /* A save written by R001-R276 (legacy key) is preserved too. */
    memset(ee,0,sizeof(ee));
    for(unsigned l=0;l<31;l++)put32(ee,l,0x01010101u*l);
    put32(ee,31,sum^0x1234fac2u);
    Init_EE();
    assert(be32(cartcopy+4)==0x01010101u);
    /* Corrupt image -> retail defaults; third Hall-of-Fame score 600000. */
    memset(ee,0xff,sizeof(ee));
    Init_EE();
    {int found=0;for(unsigned o=0;o+4<=128;o+=4)if(be32(cartcopy+o)==0x000927c0u)found=1;
     for(unsigned o=0;o+4<=128;o+=4)assert(be32(cartcopy+o)!=0x000a2990u);
     assert(found);}
    assert(be32(cartcopy+124)==Check_EE());
    puts("R277 retail EEPROM key / default Hall of Fame PASS");
    return 0;
}
