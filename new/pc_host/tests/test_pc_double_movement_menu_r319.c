/* R319 focused F12 integration test. The R305-era broad menu harness has
 * obsolete fixed menu-count assertions; reuse its real X11-menu stubs but
 * execute only the current R466 four-step movement behavior. */
#define main avp_legacy_menu_main_unused
#include "test_pc_show_fps_menu.c"
#undef main
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof *v);
    int row=-1;
    assert(v);
    settings_defaults(&v->cfg);
    for(int i=0;i<(int)(sizeof(cheat_items)/sizeof(cheat_items[0]));i++)
        if(cheat_items[i]==197)row=i;
    assert(row>=0);
    v->menu_page=MENU_CHEATS_NEW;
    v->menu_cursor=row;
    assert(menu_id(v,row)==197);
    assert(menu_count(v)==(int)(sizeof(cheat_items)/sizeof(cheat_items[0]))+1);
    for(unsigned p=0;p<4u;p++)assert(!avp_pc_video_double_movement(v,p));
    assert(!avp_pc_video_double_movement(v,4u));
    v->cheat_target=0;
    menu_choose(v);
    assert(avp_pc_video_double_movement(v,0u));
    for(unsigned p=1;p<4u;p++)assert(!avp_pc_video_double_movement(v,p));
    v->cheat_target=2;
    menu_choose(v);
    assert(avp_pc_video_double_movement(v,2u));
    assert(avp_pc_video_double_movement(v,0u));
    menu_choose(v);
    assert(avp_pc_video_double_movement(v,2u)==2);
    menu_choose(v);
    assert(avp_pc_video_double_movement(v,2u)==3);
    menu_choose(v);
    assert(!avp_pc_video_double_movement(v,2u));
    menu_adjust(v,-1);
    assert(avp_pc_video_double_movement(v,2u)==3);
    menu_adjust(v,1);
    assert(!avp_pc_video_double_movement(v,2u));
    assert(avp_pc_video_double_movement(v,0u));
    v->cheat_target=3;
    menu_choose(v);
    assert(avp_pc_video_double_movement(v,3u));
    avp_pc_video_original_reset(v);
    for(unsigned p=0;p<4u;p++)assert(!avp_pc_video_double_movement(v,p));
    free(v);
    puts("R319 actual F12 New menu: 1x-4x movement P1-P4 cycle/reset PASS");
    return 0;
}
