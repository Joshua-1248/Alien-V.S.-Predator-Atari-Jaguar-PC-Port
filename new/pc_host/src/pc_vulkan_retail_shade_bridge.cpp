/* Standalone Vulkan WSI diagnostic links only the original retail projection
 * math; it must not pull the entire game visual/JPEG/door bridge into the
 * smoke test. Production gameplay continues using visual_bridge.cpp. */
#include "jaguar/avp_gpu/projection.hpp"

extern "C" int avp_gpu_retail_wall_intensity_delta(double distance_cells)
{
    return avp::retail_wall_intensity_delta(distance_cells);
}
extern "C" int avp_gpu_retail_flat_intensity_delta(double distance_cells)
{
    return avp::retail_flat_intensity_delta(distance_cells);
}
