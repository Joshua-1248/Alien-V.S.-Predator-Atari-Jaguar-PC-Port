/* PC-only nearest-neighbour coverage checks. Synthetic inputs only. */
#include "jaguar/avp_gpu/sprite_sampling.h"
#include <assert.h>

int main(void)
{
    /* At 0.5x, each destination pixel can sample only one source texel.
       Legacy per-texel ceil/floor spans double-covered these pixels. */
    assert(avp_gpu_sprite_sample_coord(80,80.0,0.5)==1);
    /* Small animated AAF sprites use the same mapping, not special-cased
       spark geometry. Offset is projected before inverse-scale sampling. */
    assert(avp_gpu_sprite_sample_coord(81,80.0+3.0*0.5,0.5)==0);
    assert(avp_gpu_sprite_sample_coord(82,80.0+3.0*0.5,0.5)==2);
    assert(avp_gpu_sprite_sample_coord(81,80.0,0.5)==3);
    assert(avp_gpu_sprite_sample_coord(79,80.0,0.5)==-1);
    /* Integer scales repeat the appropriate texel and never jump ahead. */
    assert(avp_gpu_sprite_sample_coord(80,80.0,2.0)==0);
    assert(avp_gpu_sprite_sample_coord(81,80.0,2.0)==0);
    assert(avp_gpu_sprite_sample_coord(82,80.0,2.0)==1);
    assert(avp_gpu_sprite_sample_coord(83,80.0,2.0)==1);
    /* Noninteger MAGNIFICATION also overlaps source-rectangle writes:
     * at 1.5x, rectangle expansion maps destination 81 to both columns 0
     * and 1.  If column 1 is transparent, column 0 must not leak through.
     * Destination-centred Jaguar sampling chooses column 1 exclusively. */
    assert(avp_gpu_sprite_sample_coord(80,80.0,1.5)==0);
    assert(avp_gpu_sprite_sample_coord(81,80.0,1.5)==1);
    assert(avp_gpu_sprite_sample_coord(82,80.0,1.5)==1);
    assert(avp_gpu_sprite_sample_coord(83,80.0,1.5)==2);
    /* Projected Y offset is scaled just like the sprite data. */
    assert(avp_gpu_sprite_sample_coord(101,100.0+4.0*1.5,1.5)==-3);
    assert(avp_gpu_sprite_sample_coord(106,100.0+4.0*1.5,1.5)==0);
    /* Signed source positions, matching AAF's left and top coordinates. */
    assert(avp_gpu_sprite_sample_coord(73,80.0,1.0)==-7);
    assert(avp_gpu_sprite_sample_coord(79,80.0,1.0)==-1);
    /* Disallow nonexistent sprite scale. */
    assert(avp_gpu_sprite_sample_coord(80,80.0,0.0)==-1);
    return 0;
}
