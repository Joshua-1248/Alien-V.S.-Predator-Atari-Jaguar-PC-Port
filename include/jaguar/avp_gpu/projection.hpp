#pragma once

namespace avp {

// Retail maze renderer constants reconstructed from MAZE.INC / MAZE0.GAS /
// MAZE1.GAS / MAZE2.GAS.  The original first-person maze surface is 320x228.
constexpr int kRetailViewportWidth = 320;
constexpr int kRetailViewportHeight = 228;
constexpr int kRetailScaleDist = 160;
constexpr int kRetailSpriteHeight = 128;
constexpr int kRetailSpriteHalf = 64;
constexpr int kRetailZUnitsPerRoom = 0x8000;
/* MAZE1.INC: Z_MAX = $80000 >> 1, with MAZE2 using $8000 of z per
 * straight-ahead room.  Rays at/after this depth terminate and MAZE1 leaves
 * the corresponding 16-line horizon band black instead of letting B_IINC
 * wrap around and brighten distant geometry. */
constexpr double kRetailMaxDepthRooms = 8.0;

// Convert a host vertical pixel offset from the horizon to the retail floor /
// ceiling forward distance, in 128-unit maze-cell units.  This follows the
// DrawCeiling relation fx = 128*64 / y and only rescales the 228-line retail
// viewport to the host viewport.
double retail_flat_forward_distance(int host_y_from_horizon,
                                    int host_viewport_height) noexcept;

// Full on-screen wall-column height in host pixels for a perpendicular maze
// distance measured in cells.  This is a direct host form of DrawColumn:
// source z is 0x8000 per room, USE_RESCALE makes y_inc = z + z/4, then
// draw_half = round_outward((64<<16) / y_inc).
int retail_wall_column_height(double distance_cells,
                              int host_viewport_height) noexcept;

// MAZE1/MAZE2 both enable SRCSHADE and derive B_IINC through R1_Z2IINC.
// These return the signed integer change applied to the CRY Y/intensity byte.
int retail_wall_intensity_delta(double distance_cells) noexcept;
int retail_flat_intensity_delta(double distance_cells) noexcept;

} // namespace avp
