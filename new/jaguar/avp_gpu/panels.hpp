#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
namespace avp {

struct NoticeOverlayInfo {
  std::size_t overlay_file{};
  int x_from_center{};
  int y_from_center{};
  std::optional<std::size_t> number_file{};
};

// Resolve a maze wall byte to the retail FILES texture record loaded for the
// requested logical level. Composite notice panels resolve to their common
// base panel (FILES 16); wall_notice_overlay() supplies the retail overlay(s).
std::optional<std::size_t> wall_texture_file(std::size_t level, std::uint8_t wall) noexcept;

// LEVELS.S notice_list / num_list reconstruction. Notice panels are semantic
// panel numbers $80..$8c; retail expands FILES 16 then blits one overlay at a
// source-defined offset. Sublevel additionally receives the current deck digit.
std::optional<NoticeOverlayInfo> wall_notice_overlay(std::size_t level, std::uint8_t wall) noexcept;

// The retail renderer uses separate 256-byte signed window-pair tables for
// windows and framed doors. FILES 74..86 are the recovered tables paired to
// particular panel images. This returns the FILES id for the active wall when
// such a table exists.
std::optional<std::size_t> wall_window_pair_file(std::size_t level, std::uint8_t wall) noexcept;

// Ceiling/floor bytes are 1-based indices into the level's `floors` panel
// group (MAZE1.GAS subtracts one before computing the 128x128 source address).
std::optional<std::size_t> flat_texture_file(std::size_t level, std::uint8_t flat) noexcept;
// FixAlien appends east, south and west rotations after the l15 floor brush.
unsigned flat_texture_rotation(std::size_t level, std::uint8_t flat) noexcept;
}
