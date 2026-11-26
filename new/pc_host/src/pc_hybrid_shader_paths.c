/* R368: relocate the Vulkan executable without breaking its SPIR-V lookup.
 * The full original game remains Software without AVP_BUILD_VULKAN_GAME_PROBE. */
#include "pc/pc_hybrid_shader_paths.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int shaders_in_directory(const char *dir,AvpHybridShaderPaths *result)
{
    static const char names[3][45]={
        "avp_diag_opaque.vert.spv",
        "avp_hybrid_world.frag.spv",
        "avp_hybrid_present_overlay.frag.spv"
    };
    char *outputs[3];
    if(!dir||!*dir||!result)return 0;
    outputs[0]=result->vertex;
    outputs[1]=result->world;
    outputs[2]=result->foreground;
    for(unsigned i=0;i<3u;i++){
        int len=snprintf(outputs[i],AVP_HYBRID_SHADER_PATH_CAP,
            "%s/%s",dir,names[i]);
        struct stat info;
        if(len<0||(unsigned)len>=AVP_HYBRID_SHADER_PATH_CAP||
           stat(outputs[i],&info)!=0||!S_ISREG(info.st_mode)||
           info.st_size<=0||access(outputs[i],R_OK)!=0)return 0;
    }
    return 1;
}

int avp_hybrid_shader_paths_find(const char *override_dir,
    const char *executable_path,const char *build_dir,
    AvpHybridShaderPaths *paths)
{
    AvpHybridShaderPaths candidate={0};
    char executable_dir[AVP_HYBRID_SHADER_PATH_CAP];
    const char *slash;
    size_t len;
    if(!paths)return 0;
    memset(paths,0,sizeof(*paths));
    /* Developer override must never silently load unrelated shaders if an
     * explicitly selected directory is missing a required stage. */
    if(override_dir && *override_dir){
        if(!shaders_in_directory(override_dir,&candidate))return 0;
        *paths=candidate;
        return 1;
    }
    /* Portable package layout: LINUX/avp_pc_vulkan and LINUX/shaders/ SPIR-V stages.
     * No lookup relative to the user's current working directory. */
    slash=executable_path?strrchr(executable_path,'/'):NULL;
    if(slash){
        len=(size_t)(slash-executable_path);
        if(len==0u){
            if(shaders_in_directory("/shaders",&candidate)){
                *paths=candidate;return 1;
            }
        }else if(len<sizeof(executable_dir)-sizeof("/shaders")){
            memcpy(executable_dir,executable_path,len);
            memcpy(executable_dir+len,"/shaders",sizeof("/shaders"));
            if(shaders_in_directory(executable_dir,&candidate)){
                *paths=candidate;return 1;
            }
        }
    }
    if(shaders_in_directory(build_dir,&candidate)){
        *paths=candidate;return 1;
    }
    return 0;
}
