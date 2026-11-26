/* Uses the REAL R320 F12 menu and settings functions, with the established
 * isolated video harness's test-only backend stubs. No graphics device needed. */
#define main avp_legacy_menu_main_unused_r320
#include "test_pc_show_fps_menu.c"
#undef main
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));
    char path[]="/tmp/avp_r320_video_XXXXXX";
    int fd;
    assert(v);
    settings_defaults(&v->cfg);
    v->menu_page=MENU_VIDEO;
#ifdef AVP_LOCAL_MULTIPLAYER
    assert(menu_count(v)==18);
#else
    assert(menu_count(v)==16);
#endif
    assert(!v->cfg.renderer_requested);
    assert(avp_vk_find_selected(&v->vulkan_devices,v->cfg.vulkan_gpu_key)==-1);
    /* The GPU choice is locked until Hardware is explicitly requested. */
    v->vulkan_devices.count=2u;
    strcpy(v->vulkan_devices.gpu[0].name,"Integrated GPU");
    strcpy(v->vulkan_devices.gpu[0].key,"a0");
    strcpy(v->vulkan_devices.gpu[1].name,"Discrete GPU");
    strcpy(v->vulkan_devices.gpu[1].key,"b1");
    menu_video_adjust(v,1,1);
    assert(!v->cfg.vulkan_gpu_key[0]);
    /* Software always remains active until the actual Vulkan draw path exists. */
    menu_video_adjust(v,0,1);
    assert(v->cfg.renderer_requested==1);
    menu_video_adjust(v,1,1);
    assert(!strcmp(v->cfg.vulkan_gpu_key,"a0"));
    menu_video_adjust(v,1,1);
    assert(!strcmp(v->cfg.vulkan_gpu_key,"b1"));
    menu_video_adjust(v,1,1);
    assert(!v->cfg.vulkan_gpu_key[0]);
    menu_video_adjust(v,1,-1);
    assert(!strcmp(v->cfg.vulkan_gpu_key,"b1"));
    /* Changes to existing menu rows must not be redirected to the GPU. */
    assert(v->cfg.video_aspect==0u);
    menu_video_adjust(v,2,1);
    assert(v->cfg.video_aspect==1u);
    assert(v->cfg.texture_filter_mode==0u);
    menu_video_adjust(v,menu_count(v)-5,1);
    assert(v->cfg.texture_filter_mode==1u);
    assert(v->cfg.anisotropic_filter==0u);
    menu_video_adjust(v,menu_count(v)-4,1);
    assert(v->cfg.anisotropic_filter==2u);
    v->menu_cursor=menu_count(v)-3;
    menu_choose(v);
    assert(v->menu_page==MENU_FILTERING);
    menu_back(v);assert(v->menu_page==MENU_VIDEO);
    v->menu_cursor=menu_count(v)-2;
    menu_choose(v);assert(v->menu_page==MENU_POST_PROCESSING);
    menu_back(v);assert(v->menu_page==MENU_VIDEO);
    v->menu_cursor=menu_count(v)-1;
    menu_choose(v);assert(v->menu_page==MENU_VR_HEADSET);
    menu_back(v);assert(v->menu_page==MENU_VIDEO);
    /* Settings persist a *requested* backend; no Vulkan backend is activated. */
    fd=mkstemp(path);assert(fd>=0);close(fd);
    snprintf(v->settings_path,sizeof(v->settings_path),"%s",path);
    save_settings(v);
    {
        FILE *f=fopen(path,"rb");
        char line[256];int have_renderer=0,have_gpu=0,have_aspect=0,have_af=0;
        assert(f);
        while(fgets(line,sizeof(line),f)){
            if(!strcmp(line,"video.renderer=hardware\n"))have_renderer=1;
            if(!strcmp(line,"video.hardware_gpu=b1\n"))have_gpu=1;
            if(!strcmp(line,"video.aspect=1\n"))have_aspect=1;
            if(!strcmp(line,"video.anisotropic_filter=2\n"))have_af=1;
        }
        fclose(f);
        assert(have_renderer&&have_gpu&&have_aspect&&have_af);
    }
    menu_video_adjust(v,0,1);
    assert(v->cfg.renderer_requested==0);
    menu_video_adjust(v,1,1);
    assert(!strcmp(v->cfg.vulkan_gpu_key,"b1"));
    unlink(path);
    free(v);
    puts("PASS R320 Vulkan Video menu / GPU cycling / old rows / save-restore");
    return 0;
}
