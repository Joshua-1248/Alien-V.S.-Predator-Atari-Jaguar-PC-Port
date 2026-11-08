#include "jaguar/avp_gpu/projection.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace avp {

double retail_flat_forward_distance(int host_y_from_horizon,
                                    int host_viewport_height) noexcept {
  if (host_viewport_height <= 0 || host_y_from_horizon <= 0) return 1e30;
  const double retail_y = double(host_y_from_horizon) *
                          double(kRetailViewportHeight) /
                          double(host_viewport_height);
  return double(kRetailSpriteHalf) / std::max(retail_y, 1e-9);
}

namespace {
int iinc_delta(std::uint32_t z) noexcept {
  // Literal host form of MAZE1.INC R1_Z2IINC with
  // DEPTH_CENTRE=$4000, DEPTH_SCALE=-$10. GPU IMULT consumes the
  // signed low 16 bits and the final shlq/shrq clears the colour byte.
  const std::uint32_t q = (z - 0x4000u) >> 8;
  const auto lo = static_cast<std::int16_t>(q & 0xffffu);
  const std::int32_t prod = std::int32_t(lo) * -16;
  /* Jaguar shifts wrap in 32 bits.  Shift the unsigned representation so a
   * negative host integer never invokes C++ signed-left-shift UB. */
  std::uint32_t v = ((std::uint32_t(prod) << 8) + 0x4000u) & 0x00ffffffu;
  std::int32_t signed24 = (v & 0x00800000u) ? std::int32_t(v | 0xff000000u) : std::int32_t(v);
  // B_IINC is signed 8.16 for CRY intensity. Arithmetic shift is the
  // closest integer host representation of the hardware flat shade.
  return signed24 >> 16;
}
}

int retail_wall_intensity_delta(double distance_cells) noexcept {
  distance_cells = std::clamp(distance_cells, 0.0, kRetailMaxDepthRooms);
  std::uint32_t z = std::uint32_t(std::llround(distance_cells * double(kRetailZUnitsPerRoom)));
  z += z >> 2; // MAZE2 USE_RESCALE y_inc = z + z/4
  z &= ~1u;     // MAZE2 bclr #0,y_inc: force symmetric even scale
  return iinc_delta(z);
}

int retail_flat_intensity_delta(double distance_cells) noexcept {
  distance_cells = std::clamp(distance_cells, 0.0, kRetailMaxDepthRooms);
  // MAZE1: depth_inc=x_pos (16.16 world-pixel units), then >>7.
  // Since one room is 128 pixels this becomes 16.16 rooms.
  const std::uint32_t z = std::uint32_t(std::llround(distance_cells * 65536.0));
  return iinc_delta(z);
}

int retail_wall_column_height(double distance_cells,
                              int host_viewport_height) noexcept {
  if (host_viewport_height <= 0) return 0;
  distance_cells = std::max(distance_cells, 0.001);

  // Keep wall intersections coherent with MAZE1's floor/ceiling projection.
  // MAZE1 projects the 64-unit eye-to-plane half-height as:
  //     horizon_offset = 64 / forward_distance
  // in the retail 228-line raster.  A full 128-unit wall therefore spans
  // exactly twice that offset.  The earlier M16/M17 host conversion treated
  // DrawColumn's internal y_inc as a direct geometric scale and made walls
  // about 1.6x too tall, so their bases/tops no longer met the flats.
  const double retail_height = (2.0 * double(kRetailSpriteHalf)) / distance_cells;
  const double scaled = retail_height * double(host_viewport_height) /
                        double(kRetailViewportHeight);
  return std::max(1, int(std::ceil(scaled)));
}

} // namespace avp
