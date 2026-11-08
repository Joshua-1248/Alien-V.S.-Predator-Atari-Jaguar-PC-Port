#include "pc/pc_resources.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Synthetic resource decoder: this regression runs without redistributing
 * or embedding any part of the commercial AVP.jag. */
static const u8 original[2][16]={
    {0,64,0,1,0,0,0,0,0x20,0,0,0,0,0,0,0},
    {0,64,0,1,0,0,0,0,0x20,0,0,0,0,0,0,0}
};
static unsigned fail_file;
int avp_resources_decode_file(const AvpRomImage *rom,unsigned file_index,
                              char tag[5],u8 **decoded,size_t *size)
{
    u8 *buf;
    (void)rom;(void)tag;
    *decoded=NULL;*size=0;
    if(file_index>=2u || file_index==fail_file)return 0;
    buf=(u8*)malloc(sizeof(original[0]));
    if(!buf)return 0;
    memcpy(buf,original[file_index],sizeof(original[0]));
    *decoded=buf;*size=sizeof(original[0]);
    return 1;
}
int main(void)
{
    AvpRomImage rom={0};
    AvpMazeCandidate maps[2]={{0}};
    u8 a[16],b[16];
    rom.data=(u8*)"synthetic";
    maps[0].file_index=0;maps[0].decoded=a;maps[0].decoded_size=sizeof a;
    maps[1].file_index=1;maps[1].decoded=b;maps[1].decoded_size=sizeof b;
    memcpy(a,original[0],sizeof a);memcpy(b,original[1],sizeof b);
    /* New playthrough inherited mutable cache entries from the prior one. */
    a[14]=39;b[14]=41;
    fail_file=1;
    assert(!avp_pc_restore_new_campaign_mazes(&rom,maps,2));
    assert(a[14]==39 && b[14]==41); /* failed refresh is atomic */
    fail_file=~0u;
    assert(avp_pc_restore_new_campaign_mazes(&rom,maps,2));
    assert(memcmp(a,original[0],sizeof a)==0);
    assert(memcmp(b,original[1],sizeof b)==0);
    /* The function isn't called on ordinary level revisits, so the active
       session's door bytes remain mutable/persistent during traversal. */
    a[14]=55;
    assert(a[14]==55);
    assert(avp_pc_restore_new_campaign_mazes(&rom,maps,2));
    assert(a[14]==0);
    assert(!avp_pc_restore_new_campaign_mazes(NULL,maps,2));
    return 0;
}
