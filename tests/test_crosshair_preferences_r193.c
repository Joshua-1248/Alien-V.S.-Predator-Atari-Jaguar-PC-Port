/* RE39 R193: actual host crosshair primitive and 4 independent profile keys.
 * Existing Jaguar firing and HUD state are not touched by this renderer. */
#include "pc/pc_crosshair.h"
#include "local_mp_input.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    AvpMpInputConfig config,copy;
    uint32_t frame[240u*120u];
    const uint32_t guard=0x00202020u;
    FILE *stream;
    unsigned count=0;
    assert(avp_pc_crosshair_x(320,0,24)==160);
    assert(avp_pc_crosshair_x(320,1,24)==184);
    assert(avp_pc_crosshair_x(320,1,-24)==136);
    assert(avp_pc_crosshair_x(320,1,1000)==184);
    assert(avp_pc_crosshair_x(320,1,-1000)==136);
    for(unsigned i=0;i<240u*120u;i++)frame[i]=guard;
    avp_pc_crosshair_draw(frame,240,240,120,0,0,120,120,0,0);
    assert(frame[60u*240u+60u]!=guard);
    assert(frame[60u*240u+180u]==guard); /* second viewport untouched */
    avp_pc_crosshair_draw(frame,240,240,120,120,0,120,120,1,16);
    assert(frame[60u*240u+186u]!=guard); /* 60+120+16*120/320 = 186 */
    assert(frame[60u*240u+180u]==guard); /* unswayed P2 center untouched */
    avp_mp_input_defaults(&config);
    for(unsigned p=0;p<4u;p++){
        assert(config.player[p].crosshair_enabled==0);
        assert(config.player[p].crosshair_weapon_sway==0);
        assert(config.player[p].low_health_energy_flash==1);
    }
    assert(avp_mp_input_parse_setting(&config,"local_multiplayer.input.p2.crosshair_enabled","1"));
    assert(avp_mp_input_parse_setting(&config,"local_multiplayer.input.p4.crosshair_weapon_sway","1"));
    assert(!avp_mp_input_parse_setting(&config,"local_multiplayer.input.p5.crosshair_enabled","1"));
    assert(!avp_mp_input_parse_setting(&config,"local_multiplayer.input.p2.crosshair_enabled","bogus"));
    assert(config.player[1].crosshair_enabled && !config.player[0].crosshair_enabled);
    assert(config.player[3].crosshair_weapon_sway && !config.player[2].crosshair_weapon_sway);
    /* R305: independent and strict per-player low-health meter preference. */
    assert(avp_mp_input_parse_setting(&config,
        "local_multiplayer.input.p2.low_health_energy_flash","0"));
    assert(avp_mp_input_parse_setting(&config,
        "local_multiplayer.input.p4.low_health_energy_flash","0"));
    assert(!avp_mp_input_parse_setting(&config,
        "local_multiplayer.input.p3.low_health_energy_flash","bogus"));
    assert(config.player[0].low_health_energy_flash==1);
    assert(config.player[1].low_health_energy_flash==0);
    assert(config.player[2].low_health_energy_flash==1);
    assert(config.player[3].low_health_energy_flash==0);
    stream=tmpfile();assert(stream);
    avp_mp_input_write_settings(&config,stream);
    rewind(stream);
    avp_mp_input_defaults(&copy);
    char line[1024];
    while(fgets(line,sizeof line,stream)){
        char *eq=strchr(line,'=');
        if(!eq)continue;
        *eq++='\0';eq[strcspn(eq,"\r\n")]=0;
        if(avp_mp_input_parse_setting(&copy,line,eq))count++;
    }
    fclose(stream);
    assert(count>2u);
    assert(copy.player[1].crosshair_enabled==1);
    assert(copy.player[3].crosshair_weapon_sway==1);
    assert(copy.player[0].crosshair_enabled==0);
    assert(copy.player[0].low_health_energy_flash==1);
    assert(copy.player[1].low_health_energy_flash==0);
    assert(copy.player[2].low_health_energy_flash==1);
    assert(copy.player[3].low_health_energy_flash==0);
    puts("R193 per-player crosshair compositing, sway and persistence PASS");
    return 0;
}
