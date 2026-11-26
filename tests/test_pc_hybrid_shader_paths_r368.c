#define _POSIX_C_SOURCE 200809L
#include "pc/pc_hybrid_shader_paths.h"
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const shaders[3]={
    "avp_diag_opaque.vert.spv",
    "avp_hybrid_world.frag.spv",
    "avp_hybrid_present_overlay.frag.spv"
};
static void join(char *dst,size_t cap,const char *dir,const char *name)
{
    int n=snprintf(dst,cap,"%s/%s",dir,name);
    assert(n>0&&(size_t)n<cap);
}
static void create_shaders(const char *dir)
{
    char path[1024];
    for(unsigned i=0u;i<3u;i++){
        int fd;
        join(path,sizeof(path),dir,shaders[i]);
        fd=open(path,O_CREAT|O_WRONLY|O_TRUNC,0600);
        assert(fd>=0);
        assert(write(fd,"x",1)==1);
        assert(close(fd)==0);
    }
}
static void delete_shaders(const char *dir)
{
    char path[1024];
    for(unsigned i=0u;i<3u;i++){
        join(path,sizeof(path),dir,shaders[i]);
        assert(unlink(path)==0);
    }
}
int main(void)
{
    char root[]="/tmp/avp-hybrid-r368-XXXXXX";
    char bin[1024],packed[1024],fallback[1024],override[1024];
    char exe[1024],stale[1024],giant[2048];
    AvpHybridShaderPaths paths={0};
    assert(mkdtemp(root)!=NULL);
    join(bin,sizeof(bin),root,"bin");
    join(packed,sizeof(packed),bin,"shaders");
    join(fallback,sizeof(fallback),root,"build-spv");
    join(override,sizeof(override),root,"override");
    join(exe,sizeof(exe),bin,"avp_pc_vulkan");
    assert(mkdir(bin,0700)==0);
    assert(mkdir(packed,0700)==0);
    assert(mkdir(fallback,0700)==0);
    assert(mkdir(override,0700)==0);
    create_shaders(fallback);
    /* Development build may load its build-directory stages. */
    assert(avp_hybrid_shader_paths_find(NULL,exe,fallback,&paths));
    assert(strncmp(paths.vertex,fallback,strlen(fallback))==0);
    /* Portable stages MUST win over stale compiled-in absolute directory. */
    create_shaders(packed);
    assert(avp_hybrid_shader_paths_find(NULL,exe,fallback,&paths));
    assert(strncmp(paths.vertex,packed,strlen(packed))==0);
    assert(strncmp(paths.world,packed,strlen(packed))==0);
    assert(strncmp(paths.foreground,packed,strlen(packed))==0);
    /* Empty stage must NOT mask a complete developer shader set. */
    join(stale,sizeof(stale),packed,shaders[1]);
    assert(truncate(stale,0)==0);
    assert(avp_hybrid_shader_paths_find(NULL,exe,fallback,&paths));
    assert(strncmp(paths.vertex,fallback,strlen(fallback))==0);
    create_shaders(packed);
    /* Explicit override always wins, even over a fully installed pack. */
    create_shaders(override);
    assert(avp_hybrid_shader_paths_find(override,exe,fallback,&paths));
    assert(strncmp(paths.vertex,override,strlen(override))==0);
    /* Incomplete override MUST NOT fall back to packaged stages. */
    join(stale,sizeof(stale),override,shaders[1]);
    assert(unlink(stale)==0);
    assert(!avp_hybrid_shader_paths_find(override,exe,fallback,&paths));
    assert(!paths.vertex[0]&&!paths.world[0]&&!paths.foreground[0]);
    /* Incomplete packaged set falls back to the complete developer build. */
    join(stale,sizeof(stale),packed,shaders[2]);
    assert(unlink(stale)==0);
    assert(avp_hybrid_shader_paths_find(NULL,exe,fallback,&paths));
    assert(strncmp(paths.vertex,fallback,strlen(fallback))==0);
    /* No valid pack -> fail closed; excessive paths never truncate silently. */
    delete_shaders(fallback);
    memset(giant,'q',sizeof(giant)-1u);
    giant[sizeof(giant)-1u]='\0';
    assert(!avp_hybrid_shader_paths_find(giant,exe,NULL,&paths));
    assert(!paths.vertex[0]);
    assert(!avp_hybrid_shader_paths_find(NULL,exe,fallback,&paths));
    assert(!avp_hybrid_shader_paths_find(NULL,NULL,NULL,&paths));
    assert(!avp_hybrid_shader_paths_find(NULL,NULL,NULL,NULL));
    join(stale,sizeof(stale),packed,shaders[0]);
    assert(unlink(stale)==0);
    join(stale,sizeof(stale),packed,shaders[1]);
    assert(unlink(stale)==0);
    join(stale,sizeof(stale),override,shaders[0]);
    assert(unlink(stale)==0);
    join(stale,sizeof(stale),override,shaders[2]);
    assert(unlink(stale)==0);
    assert(rmdir(override)==0);
    assert(rmdir(fallback)==0);
    assert(rmdir(packed)==0);
    assert(rmdir(bin)==0);
    assert(rmdir(root)==0);
    puts("R368: packaged SPIR-V path lookup/override/fallback/invalid cases PASS");
    return 0;
}
