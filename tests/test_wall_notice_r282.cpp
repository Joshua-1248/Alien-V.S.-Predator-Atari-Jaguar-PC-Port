// R282: per-level wall notice semantics must equal LEVINFO.S l*_panels and the
// retail RDB bytes ($240A8 l15, $24130 l2, $241BA l3, $2424C l4).  Overlay
// FILES: 56 airnotice, 57 armoury, 58 canteen, 61 medlab, 63 command,
// 64 petty office, 65 brig, 66 training, 67 off area, 68 barracks.
#include "jaguar/avp_gpu/panels.hpp"
#include <cassert>
#include <cstdio>
static std::size_t ov(std::size_t level,unsigned code)
{
    const auto o=avp::wall_notice_overlay(level,static_cast<std::uint8_t>(code));
    assert(o);
    return o->overlay_file;
}
int main()
{
    // User-reported defects.
    assert(ov(3,14)==58);   // Sub-Level 3 Canteen: CANTEEN (was TRAINING)
    assert(ov(4,17)==66);   // Sub-Level 4 Training Maze: TRAINING (was COMMAND)
    // Same transcription error, other slots.
    assert(ov(3,10)==56);   // airnotis (was CANTEEN)
    assert(ov(3,15)==65);   // brigg (was BARRACKS)
    assert(ov(3,16)==68);   // barracks (was BRIG)
    assert(ov(4,10)==57);   // armoury (was DOCKING)
    // Already-correct signs stay unchanged.
    assert(ov(3,17)==64 && ov(4,14)==61);
    assert(ov(2,17)==63 && ov(2,10)==57 && ov(2,8)==67 && ov(2,14)==61);
    assert(ov(1,10)==56 && ov(5,10)==56);
    for(unsigned l=1;l<=5;l++)assert(ov(l,1)==62); // SUBLEVEL on every deck
    // Non-notice wall codes are not notices.
    assert(!avp::wall_notice_overlay(3,11) && !avp::wall_notice_overlay(4,15));
    std::puts("R282 LEVINFO wall notice semantics PASS");
    return 0;
}
