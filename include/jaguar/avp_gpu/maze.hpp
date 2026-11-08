#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace avp {

constexpr std::size_t kMazeWidth = 64;
constexpr std::size_t kMazeHeight = 64;
constexpr std::size_t kMazeCellBytes = 8;
constexpr std::int32_t kCellWorldUnits = 128;
constexpr std::int32_t kFloorHeight = 0;
constexpr std::int32_t kCeilingHeight = 128;
constexpr std::int32_t kViewHeight = 64;
constexpr std::int32_t kPlayerRadius = 40;
constexpr std::int32_t kPlayerHeight = 80;

struct MazeCell {
  std::uint8_t left_wall{};
  std::uint8_t top_wall{};
  std::uint8_t right_wall{};
  std::uint8_t bottom_wall{};
  std::uint8_t ceiling{};
  std::uint8_t floor{};
  std::uint8_t left_door{};
  std::uint8_t top_door{};
};
static_assert(sizeof(MazeCell) == kMazeCellBytes, "AVP maze cell must remain exactly 8 bytes");

struct WallCode {
  bool reflected{};
  std::uint8_t type{};
  std::uint8_t panel{};
};

constexpr WallCode decode_wall(std::uint8_t value) noexcept {
  return { (value & 0x80u) != 0u,
           static_cast<std::uint8_t>((value >> 3u) & 0x0Fu),
           static_cast<std::uint8_t>(value & 0x07u) };
}

class Maze {
public:
  static Maze load_decoded(const std::filesystem::path& path);

  std::uint16_t width() const noexcept { return width_; }
  std::uint16_t height() const noexcept { return height_; }
  std::uint16_t control0() const noexcept { return control0_; }
  std::uint16_t control1() const noexcept { return control1_; }

  const MazeCell& cell(std::size_t x, std::size_t y) const;
  MazeCell& cell(std::size_t x, std::size_t y);

  std::uint8_t right_door(std::size_t x, std::size_t y) const;
  std::uint8_t bottom_door(std::size_t x, std::size_t y) const;

private:
  std::uint16_t width_{};
  std::uint16_t height_{};
  std::uint16_t control0_{};
  std::uint16_t control1_{};
  std::vector<MazeCell> cells_;
};

std::size_t retail_map_file_id(std::size_t logical_level_index);

} // namespace avp
