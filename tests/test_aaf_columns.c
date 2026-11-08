#include "jaguar/avp_gpu/aaf_columns.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Retail AAF 47, last impact-puff frame: 15 column offsets, three empty
     * columns at indices 2, 9, 13.  The erroneous legacy renderer read the
     * header's offset words as visible blue pixels at those positions. */
    const unsigned short puff[15]={0x20,0x26,0,0x3a,0x3e,0x56,0x6e,
                                   0x84,0x88,0,0x8e,0x92,0x96,0,0x9a};
    unsigned valid=0;
    for(unsigned i=0;i<15;i++)valid+=avp_gpu_aaf_column_offset_valid(puff[i],15);
    assert(valid==12u);
    assert(!avp_gpu_aaf_column_offset_valid(0,15));
    assert(!avp_gpu_aaf_column_offset_valid(30,15));
    assert(avp_gpu_aaf_column_offset_valid(32,15));
    /* Original flame AAF 39 frame 5 has 45 columns, two empty sentinels. */
    assert(!avp_gpu_aaf_column_offset_valid(0,45));
    assert(avp_gpu_aaf_column_offset_valid(0x5c,45));
    puts("Retail AAF empty-column regression passed");
    return 0;
}
