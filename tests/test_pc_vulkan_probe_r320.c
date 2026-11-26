#include "pc/pc_vulkan_probe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
int main(void)
{
    AvpVulkanDevices v;
    const char *mock=getenv("AVP_EXPECT_MOCK_VULKAN");
#ifndef _WIN32
    /* Retain mock driver across probing so lifetime counters do not reset
     * when the production probe dlcloses its private loader handle. */
    void *mock_library=mock?dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL):NULL;
    if(mock&&!mock_library) return 14;
#endif
    avp_vk_enumerate(&v,mock?(void*)(uintptr_t)1u:NULL,mock?2u:0u);
#ifndef _WIN32
    if(mock){
        int (*balance)(void)=(int (*)(void))dlsym(mock_library,"avp_test_vk_instance_balance");
        if(!balance||balance()!=0) return 15;
        dlclose(mock_library);
    }
#endif
    if(!v.status[0]||v.count>AVP_VK_MAX_GPUS)return 1;
    if(avp_vk_find_selected(&v,NULL)!=-1)return 2;
    if(avp_vk_find_selected(&v,"")!=-1)return 3;
    if(avp_vk_find_selected(&v,"missing:-:-")!=-1)return 4;
    if(mock){
        if(!v.loader_found||!v.instance_created||!v.surface_created)return 5;
        if(v.count!=2u||v.surface_compatible_count!=1u)return 6;
        if(strcmp(v.gpu[0].name,"Mock Integrated GPU")||
           strcmp(v.gpu[1].name,"Mock Discrete GPU"))return 7;
        if(v.gpu[0].surface_compatible||!v.gpu[1].surface_compatible)return 8;
        if(!strcmp(v.gpu[0].key,v.gpu[1].key))return 9;
        if(avp_vk_find_selected(&v,v.gpu[0].key)!=0 ||
           avp_vk_find_selected(&v,v.gpu[1].key)!=1)return 10;
        if(avp_vk_choose_compatible(&v,NULL)!=1 ||
           avp_vk_choose_compatible(&v,v.gpu[0].key)!=1 ||
           avp_vk_choose_compatible(&v,v.gpu[1].key)!=1)return 11;
        v.gpu[1].surface_compatible=0;
        if(avp_vk_choose_compatible(&v,NULL)!=-1)return 12;
    }
    printf("PASS Vulkan discovery: %s (GPU count: %u, WSI count: %u)\n",
           v.status,v.count,v.surface_compatible_count);
    return 0;
}
