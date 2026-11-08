#include "jaguar/avp_gpu/projection.hpp"
#include <cassert>

int main()
{
    using namespace avp;
    static_assert(kRetailZUnitsPerRoom == 0x8000, "retail z scale drift");
    static_assert(kRetailMaxDepthRooms == 8.0, "MAZE1/2 Z_MAX drift");

    /* Source R1_Z2IINC stays monotonic throughout the legal scan range.\n     * The old host path evaluated it beyond Z_MAX, where the signed low-word\n     * multiply wraps and distant walls become bright again. */
    int previous = retail_wall_intensity_delta(0.0);
    for (int i=1;i<=80;i++) {
        const double d = (double)i / 10.0;
        const int cur = retail_wall_intensity_delta(d);
        assert(cur <= previous);
        previous = cur;
    }
    assert(retail_wall_intensity_delta(100.0) == retail_wall_intensity_delta(8.0));
    assert(retail_flat_intensity_delta(100.0) == retail_flat_intensity_delta(8.0));
    assert(retail_flat_forward_distance(8,228) == 8.0);
    return 0;
}
