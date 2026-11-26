#include "jaguar/avp_gpu/cry.hpp"
#include "jaguar/cry.h"
#include <cstddef>
#include <stdexcept>

namespace avp {

std::uint16_t rgb_to_cry(std::uint8_t r,std::uint8_t g,std::uint8_t b,
                         const std::vector<std::uint8_t>& qconv) {
  if(qconv.size() < 131072u)
    throw std::invalid_argument("QCONV table must contain 131072 bytes");
  return avp_jaguar_rgb_to_cry_qconv(r,g,b,qconv.data(),qconv.size());
}

Rgb8 cry_to_rgb(std::uint16_t cry) noexcept {
  Rgb8 out{};
  avp_jaguar_cry16_to_rgb(cry,&out.r,&out.g,&out.b);
  return out;
}

void convert_rgb_image_to_cry_display(RgbImage& image,
                                      const std::vector<std::uint8_t>& qconv) {
  if(qconv.size() < 131072u) return;
  for(std::size_t i=0;i+2<image.rgb.size();i+=3) {
    const auto cry=rgb_to_cry(image.rgb[i],image.rgb[i+1],image.rgb[i+2],qconv);
    const auto rgb=cry_to_rgb(cry);
    image.rgb[i]=rgb.r; image.rgb[i+1]=rgb.g; image.rgb[i+2]=rgb.b;
  }
}

} // namespace avp
