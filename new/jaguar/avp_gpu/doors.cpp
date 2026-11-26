#include "jaguar/avp_gpu/doors.hpp"
#include "jaguar/avp_gpu/panels.hpp"
#include "jaguar/avp_gpu/maze.hpp"
#include "jaguar/avp_gpu/maze2_column.h"
#include <algorithm>
namespace avp {
bool door_visual_column_open(std::uint8_t wall,std::uint8_t open,int panel_pos) noexcept {
  const auto code=decode_wall(wall); if(code.type<4||code.type>6)return false;
  const int pos=std::clamp(panel_pos,0,127); int shifted=pos;
  return avp_gpu_maze2_centered_door_source_col(pos,open,&shifted)==0;
}
std::uint8_t DoorSystem::opening_limit(std::uint8_t wall) noexcept {
  const auto c=decode_wall(wall); switch(c.type){case 4:return c.panel==0?56:48;case 5:return 48;case 6:return c.panel==0?52:(c.panel==1?56:48);case 7:return 112;case 8:return 60;case 9:case 10:return c.panel==0?92:(c.panel==1?100:48);case 11:return 48;default:return 48;}
}
}
