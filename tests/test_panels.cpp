#include "jaguar/avp_gpu/panels.hpp"
#include <cassert>

int main()
{
    using namespace avp;
    /* FILES 40 is the final l15 floor brush in retail LEVINFO.S.
     * FixAlien expands indices 9,10,11 as E/S/W rotated variants. */
    for(unsigned l : {1u,5u}){
        for(unsigned f=8;f<=11;f++){
            auto id=flat_texture_file(l,static_cast<std::uint8_t>(f));
            assert(id&&*id==40u);
            assert(flat_texture_rotation(l,static_cast<std::uint8_t>(f))==f-8u);
        }
    }
    assert(!flat_texture_file(5u,12u));
    assert(!flat_texture_file(5u,0u));
    assert(flat_texture_rotation(6u,9u)==0u);
    assert(flat_texture_rotation(2u,9u)==0u);
    assert(flat_texture_file(10u,1u)==36u);
    assert(flat_texture_file(10u,2u)==37u);
    return 0;
}
