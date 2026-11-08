#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace avp {
struct RgbImage {
  int width{};
  int height{};
  std::vector<std::uint8_t> rgb; // RGB888
};

RgbImage decode_jagpeg(const std::vector<std::uint8_t>& bytes, int quality, bool swap_chroma=false);
void write_ppm(const std::filesystem::path& path, const RgbImage& image);
RgbImage read_ppm(const std::filesystem::path& path);
}
