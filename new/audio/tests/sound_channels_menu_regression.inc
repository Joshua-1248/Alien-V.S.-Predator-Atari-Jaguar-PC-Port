/* Called by the existing F12 settings roundtrip harness. */
static void test_sound_channel_menu(AvpPcVideo *v,AvpPcVideo *loaded)
{
    int found=0;
    assert(v->cfg.sound_effect_channels==8u);
    v->menu_page=MENU_ENHANCEMENTS;
    for(int row=0;row<menu_count(v);row++)if(menu_id(v,row)==199){found++;v->menu_cursor=row;}
    assert(found==1);
    v->menu_index=199;menu_adjust(v,1);assert(v->cfg.sound_effect_channels==16u);
    v->menu_index=199;
    for(unsigned count=24u;count<=256u;count+=8u){menu_adjust(v,1);assert(v->cfg.sound_effect_channels==count);}
    menu_adjust(v,1);assert(v->cfg.sound_effect_channels==256u);
    load_settings(loaded);assert(loaded->cfg.sound_effect_channels==256u);
    for(unsigned count=248u;count>=8u;count-=8u){menu_adjust(v,-1);assert(v->cfg.sound_effect_channels==count);}
    menu_adjust(v,-1);assert(v->cfg.sound_effect_channels==8u);
    FILE *f=fopen(v->settings_path,"w");assert(f);
    fputs("enhancement.sound_effect_channels=255\n",f);fclose(f);
    load_settings(loaded);assert(loaded->cfg.sound_effect_channels==8u);
    puts("R399 F12 sound channel stepping and persistence PASS");
}
