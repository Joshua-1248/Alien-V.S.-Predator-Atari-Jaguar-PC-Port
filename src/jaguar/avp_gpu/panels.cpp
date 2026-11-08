#include "jaguar/avp_gpu/panels.hpp"
#include "jaguar/avp_gpu/maze.hpp"
#include <array>

namespace avp {
namespace {
// LEVELS.S semantic notice panel numbers.  These are not FILES ids; GetPanels
// expands FILES 16 (notice) and composites the corresponding overlay.
/* LEVELS.S defines docking ($83) but no retail level's wall list uses it. */
[[maybe_unused]] constexpr int DOCKING=0x83;
constexpr int SUBLEVEL=0x80, ARMOURY=0x81, MEDLAB=0x82, CANTEEN=0x84,
              ELEVATOR=0x85, AIRNOTICE=0x86, PETTYOFF=0x87, COMMAND=0x88,
              OFFAREA=0x89, TRAINING=0x8a, BARRACKS=0x8b, BRIG=0x8c;
struct Set {
  const int* walls; std::size_t walls_n;
  const int* wins; std::size_t wins_n;
  const int* csb; std::size_t csb_n;
  const int* csp; std::size_t csp_n;
  const int* csw; std::size_t csw_n;
  const int* bup; std::size_t bup_n;
  const int* cup; std::size_t cup_n;
  const int* bub; std::size_t bub_n;
  const int* cub; std::size_t cub_n;
  const int* flats; std::size_t flats_n;
};
#define A(name, ...) constexpr int name[]={__VA_ARGS__}
A(l15w,SUBLEVEL,ELEVATOR,5,9,3,12,13,38,15,AIRNOTICE,21,39); A(l15win,10,11,41); A(l15csb,0); A(l15csp,30); A(l15cup,6); A(l15bub,35,17); A(l15cub,1); /* LEVELS.S::l15_panels supplies eight floor brushes, then FixAlien blits
 * three additional rotations of the last one (gunk3a / FILES 40).
 * Keep the base asset ROM-owned; the host renderer rotates its UVs. */
A(l15f,2,4,8,7,18,20,38,40,40,40,40);
A(l2w,SUBLEVEL,ELEVATOR,5,9,3,12,13,OFFAREA,15,ARMOURY,21,22,23,MEDLAB,27,25,COMMAND); A(l2win,10,11,28); A(l2csb,0); A(l2csw,29); A(l2cup,6); A(l2bub,17); A(l2cub,1); A(l2f,2,4,8,7,18,20,24,26);
/* R282: L3/L4 wall lists are LEVINFO.S l3_panels/l4_panels exactly (retail
 * RDB $241BA / $2424C): L3 10=airnotis 14=canteen 15=brigg 16=barracks,
 * L4 10=armoury 17=training.  R276 had CANTEEN/TRAINING/BARRACKS/BRIG and
 * DOCKING/COMMAND in the wrong slots (Canteen showed TRAINING MAZE, the L4
 * Training Maze showed COMMAND CENTER). */
A(l3w,SUBLEVEL,ELEVATOR,5,9,3,12,13,13,15,AIRNOTICE,21,33,34,CANTEEN,BRIG,BARRACKS,PETTYOFF); A(l3win,10); A(l3csb,0); A(l3csp,30); A(l3csw,19); A(l3cup,6); A(l3bub,17); A(l3cub,1); A(l3f,2,4,8,7,18,20,31,32);
A(l4w,SUBLEVEL,ELEVATOR,5,9,3,12,13,14,15,ARMOURY,21,22,23,MEDLAB,27,25,TRAINING); A(l4win,10,11,28); A(l4csb,0); A(l4csw,19,29); A(l4cup,6); A(l4bub,17); A(l4cub,1); A(l4f,2,4,8,7,18,20,24,26);
A(ductw,36,37); A(ductf,36,37); // FixDucts: floors = walls
A(alienw,45,46,47,21,50); A(alienwin,43,44); A(aliensp,30); A(alienf,42,48,18,20,49);
A(predw,54,55,21); A(predwin,51); A(predsp,30); A(predf,54,52,53,18,20);
#undef A
constexpr Set l15{l15w,std::size(l15w),l15win,std::size(l15win),l15csb,std::size(l15csb),l15csp,std::size(l15csp),nullptr,0,nullptr,0,l15cup,std::size(l15cup),l15bub,std::size(l15bub),l15cub,std::size(l15cub),l15f,std::size(l15f)};
constexpr Set l2{l2w,std::size(l2w),l2win,std::size(l2win),l2csb,std::size(l2csb),nullptr,0,l2csw,std::size(l2csw),nullptr,0,l2cup,std::size(l2cup),l2bub,std::size(l2bub),l2cub,std::size(l2cub),l2f,std::size(l2f)};
constexpr Set l3{l3w,std::size(l3w),l3win,std::size(l3win),l3csb,std::size(l3csb),l3csp,std::size(l3csp),l3csw,std::size(l3csw),nullptr,0,l3cup,std::size(l3cup),l3bub,std::size(l3bub),l3cub,std::size(l3cub),l3f,std::size(l3f)};
constexpr Set l4{l4w,std::size(l4w),l4win,std::size(l4win),l4csb,std::size(l4csb),nullptr,0,l4csw,std::size(l4csw),nullptr,0,l4cup,std::size(l4cup),l4bub,std::size(l4bub),l4cub,std::size(l4cub),l4f,std::size(l4f)};
constexpr Set ducts{ductw,std::size(ductw),nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,ductf,std::size(ductf)};
constexpr Set alien{alienw,std::size(alienw),alienwin,std::size(alienwin),nullptr,0,aliensp,std::size(aliensp),nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,alienf,std::size(alienf)};
constexpr Set pred{predw,std::size(predw),predwin,std::size(predwin),nullptr,0,predsp,std::size(predsp),nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,predf,std::size(predf)};

const Set& set_for(std::size_t level){ if(level==1||level==5)return l15; if(level==2)return l2; if(level==3)return l3; if(level==4)return l4; if(level>=6&&level<=13)return ducts; if(level==14)return alien; return pred; }
std::optional<std::size_t> pick(const int* p,std::size_t n,std::size_t i){ if(!p||i>=n)return std::nullopt; return std::size_t(p[i]); }
}

std::optional<std::size_t> wall_texture_file(std::size_t level,std::uint8_t wall) noexcept {
  if(!wall) return std::nullopt;
  const auto c=decode_wall(wall);
  const auto& s=set_for(level);
  if(c.type<=2){
    const std::size_t raw=std::size_t(wall&0x7f); if(raw==0)return std::nullopt;
    const auto semantic=pick(s.walls,s.walls_n,raw-1);
    if(semantic && *semantic>=0x80 && *semantic<=0x8c) return std::size_t(16); // notice base
    return semantic;
  }
  switch(c.type){
    case 3:return pick(s.wins,s.wins_n,c.panel);
    case 4:return pick(s.csb,s.csb_n,c.panel);
    case 5:return pick(s.csp,s.csp_n,c.panel);
    case 6:return pick(s.csw,s.csw_n,c.panel);
    case 7:return pick(s.bup,s.bup_n,c.panel);
    case 8:return pick(s.cup,s.cup_n,c.panel);
    case 9:case 10:return pick(s.bub,s.bub_n,c.panel);
    case 11:return pick(s.cub,s.cub_n,c.panel);
    /* MAZE2.GAS wall_jumps: $68 singles, $70 one-sided bulkhead and
       $78 two-sided bulkhead all use WIN_SPRITES/WIN_WLIST.  Only $60
       is the unknown/non-rendering slot in the retail non-DUNGEON table. */
    case 13:case 14:case 15:return pick(s.wins,s.wins_n,c.panel);
    default:return std::nullopt;
  }
}

std::optional<NoticeOverlayInfo> wall_notice_overlay(std::size_t level,std::uint8_t wall) noexcept {
  if(!wall) return std::nullopt;
  const auto c=decode_wall(wall);
  if(c.type>2) return std::nullopt;
  const auto& s=set_for(level);
  const std::size_t raw=std::size_t(wall&0x7f);
  if(raw==0) return std::nullopt;
  const auto semantic=pick(s.walls,s.walls_n,raw-1);
  if(!semantic || *semantic<0x80 || *semantic>0x8c) return std::nullopt;

  // LEVELS.S notice_list: place on FILES 16, place overlay, offset from centre.
  // FILES ids 56..68 correspond in semantic notice-number order except the
  // historical generated symbols are ordered differently in filesym.inc.
  static constexpr std::size_t overlays[13] = {
    62, // $80 sublevel / _subleve
    57, // $81 armoury  / _armoury
    61, // $82 medlab   / _medlab
    59, // $83 docking  / _docking
    58, // $84 canteen  / _canteen
    60, // $85 elevator / _elevato
    56, // $86 air notice / _airnoti
    64, // $87 petty office / _pettyof
    63, // $88 command / _command
    67, // $89 off area / _offarea
    66, // $8a training / _trainin
    68, // $8b barracks / _barrack
    65  // $8c brig / _brig
  };
  const std::size_t idx=*semantic-0x80;
  NoticeOverlayInfo out{overlays[idx], 0, (*semantic==0x80 ? -16 : 0), std::nullopt};
  if(*semantic==0x80 && level>=1 && level<=5) out.number_file=std::size_t(68+level); // no1..no5 = 69..73
  return out;
}

std::optional<std::size_t> wall_window_pair_file(std::size_t level,std::uint8_t wall) noexcept {
  const auto panel=wall_texture_file(level,wall);
  if(!panel) return std::nullopt;
  // Recovered FILES 74..86 window-pair resources (256 signed bytes =
  // 128 {top,bottom} pairs), correlated to the source panel names.
  switch(*panel){
    case 51:return 74; // predbulk
    case 43:return 75; // abulk1
    case 44:return 76; // abulk2
    case 41:return 77; // respan4
    case 10:return 78; // pipepil
    case 11:return 79; // bulkpil
    case 28:return 80; // medwin
    case 0:return 81;  // bulk1
    case 29:return 82; // medoor
    case 19:return 83; // armdor2
    case 35:return 84; // baybulka
    case 17:return 85; // armbulk
    case 1:return 86;  // bulk2
    default:return std::nullopt;
  }
}

unsigned flat_texture_rotation(std::size_t level,std::uint8_t flat) noexcept {
  /* LEVELS.S::FixAlien creates the east, south and west variants directly
   * after the base north panel.  Both level 1 and level 5 use this group. */
  if((level==1u||level==5u)&&flat>=9u&&flat<=11u)
    return static_cast<unsigned>(flat-8u);
  return 0u;
}

std::optional<std::size_t> flat_texture_file(std::size_t level,std::uint8_t flat) noexcept {
  if(flat==0) return std::nullopt;
  const auto& s=set_for(level);
  return pick(s.flats,s.flats_n,std::size_t(flat-1));
}
} // namespace avp
