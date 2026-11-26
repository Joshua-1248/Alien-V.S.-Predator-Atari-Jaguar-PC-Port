#pragma once
#include <cstdint>
namespace avp {
bool door_visual_column_open(std::uint8_t wall,std::uint8_t open,int panel_pos) noexcept;
class DoorSystem { public: static std::uint8_t opening_limit(std::uint8_t wall) noexcept; };
}
