#pragma once
#include <cstdint>
#include <vector>
#include "jaguar/avp_gpu/jagpeg.hpp"

namespace avp {

struct Rgb8 { std::uint8_t r{}, g{}, b{}; };

// AVP's shipping RGB->CRY conversion using the user's FILES #239 QCONV table.
// qconv must contain exactly the decoded 128 KiB lookup table.
std::uint16_t rgb_to_cry(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                         const std::vector<std::uint8_t>& qconv);

// Jaguar TOM CRY16 -> host RGB using the documented 16x16 modifier tables.
Rgb8 cry_to_rgb(std::uint16_t cry) noexcept;

// Convert a JagPEG RGB intermediate into the color that Jaguar TOM displays
// after AVP's QCONV/CRY path. This is done locally from the user's ROM data.
void convert_rgb_image_to_cry_display(RgbImage& image,
                                      const std::vector<std::uint8_t>& qconv);

} // namespace avp
