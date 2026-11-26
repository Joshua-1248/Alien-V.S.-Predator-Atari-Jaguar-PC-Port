/* R341: delegate to the same retail Jaguar projection used by Software.
 * No parallel/approximate intensity calculation is introduced. */
#include "pc/pc_vulkan_wall_lighting.h"
#include "jaguar/avp_gpu/projection.hpp"
#include "jaguar/avp_gpu/host_distance.h"
#include <cmath>
extern "C" int avp_vk_retail_wall_shade(double distance,
                                         int fog_mode,int disable_fog)
{
    if (!std::isfinite(distance) || distance<0.0 ||
        fog_mode<0 || fog_mode>=AVP_HOST_FOG_DISTANCE_CHOICES ||
        (disable_fog!=0 && disable_fog!=1))return 100000;
    return avp_host_fog_shade_delta(
        avp::retail_wall_intensity_delta(
            avp_host_fog_distance(distance,fog_mode)),disable_fog);
}
