/* Durable native PC quicksave archive for Linux and Windows.  No devices,
 * host window state or ROM bytes are serialized.  The exact executable and
 * retail ROM are required because the snapshot holds native pointers. */
#include "pc/pc_quickstate.h"
#include "jaguar/avp_gpu/state.h"
#include "avp_resources.h"
#include "pc/pc_sha256.h"
#include <zlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef _WIN32
#include "pc/win32_posix.h"
#else
#include <unistd.h>
#endif

extern unsigned char __avp_game_state_start[];
#ifndef _WIN32
extern unsigned char __ehdr_start[];
extern unsigned char _end[];
#endif

enum { QS_DISK_VERSION=2, QS_DISK_MAX_IMAGE=16*1024*1024 };
typedef struct QsDiskHeader {
    char magic[8];
    uint32_t version,header_size,word_bytes,core_bytes,maze_bytes,map_bytes;
    uint32_t width,height,level;
    uint64_t old_image,old_image_size,old_core,old_maze,old_rom,rom_size,ticks;
    unsigned char exe_hash[AVP_PC_SHA256_LEN];
    char rom_hash[65];
    uint32_t crc32_value;
} QsDiskHeader;

/* A per-user writable location keeps saves available after moving the
 * executable or rebuilding in a read-only installation directory.  Reads
 * also accept the earlier executable-adjacent Linux archive. */
static int executable_path(char *path,size_t n)
{
    ssize_t nr=readlink("/proc/self/exe",path,n-1u);
    if(n<2u||nr<=0||(size_t)nr>=n)return 0;
    path[nr]=0;return 1;
}
static int adjacent_path(char *path,size_t n)
{
    char *slash;size_t a;static const char suffix[]="/avp_quickstate.bin";
    if(!executable_path(path,n))return 0;
    slash=strrchr(path,'/');if(!slash)return 0;*slash=0;
    a=strlen(path);if(a+sizeof(suffix)>n)return 0;
    memcpy(path+a,suffix,sizeof(suffix));return 1;
}
static int ensure_dir(const char *p)
{
#ifdef _WIN32
    if(CreateDirectoryA(p,NULL))return 1;
    return GetLastError()==ERROR_ALREADY_EXISTS;
#else
    return mkdir(p,0700)==0||errno==EEXIST;
#endif
}
static int user_path(char *path,size_t n)
{
    const char *base=NULL;char dir[PATH_MAX];size_t len;
#ifdef _WIN32
    base=getenv("LOCALAPPDATA");
    if(!base||!*base)base=getenv("APPDATA");
    if(!base||!*base)return 0;
    if(snprintf(dir,sizeof(dir),"%s/AVP_Jaguar",base)>=(int)sizeof(dir))return 0;
#else
    base=getenv("XDG_DATA_HOME");
    if(base&&*base){
        if(snprintf(dir,sizeof(dir),"%s/avp-jaguar",base)>=(int)sizeof(dir))return 0;
    }else{
        char parent[PATH_MAX];base=getenv("HOME");if(!base||!*base)return 0;
        if(snprintf(parent,sizeof(parent),"%s/.local",base)>=(int)sizeof(parent)||
           !ensure_dir(parent))return 0;
        if(snprintf(parent,sizeof(parent),"%s/.local/share",base)>=(int)sizeof(parent)||
           !ensure_dir(parent))return 0;
        if(snprintf(dir,sizeof(dir),"%s/avp-jaguar",parent)>=(int)sizeof(dir))return 0;
    }
#endif
    if(!ensure_dir(dir))return 0;
    len=strlen(dir);
    if(len+sizeof("/avp_quickstate.bin")>n)return 0;
    memcpy(path,dir,len);memcpy(path+len,"/avp_quickstate.bin",sizeof("/avp_quickstate.bin"));
    return 1;
}
static uint64_t loaded_image_base(void)
{
#ifdef _WIN32
    return (uint64_t)(uintptr_t)GetModuleHandleA(NULL);
#else
    return (uint64_t)(uintptr_t)__ehdr_start;
#endif
}
static uint64_t loaded_image_size(void)
{
#ifdef _WIN32
    const unsigned char *base=(const unsigned char*)GetModuleHandleA(NULL);
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER*)base;
    const IMAGE_NT_HEADERS64 *pe;
    if(!base||dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>65536)return 0;
    pe=(const IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return 0;
    return (uint64_t)pe->OptionalHeader.SizeOfImage;
#else
    return (uint64_t)(_end-__ehdr_start);
#endif
}
static int executable_hash(unsigned char out[AVP_PC_SHA256_LEN])
{
    char link[PATH_MAX];FILE *f;long n;unsigned char *data;int ok=0;
    if(!executable_path(link,sizeof(link)))return 0;
    f=fopen(link,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0||(n=ftell(f))<=0||n>QS_DISK_MAX_IMAGE||
       fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    data=(unsigned char*)malloc((size_t)n);
    if(data){
        if(fread(data,1,(size_t)n,f)==(size_t)n)
            ok=avp_pc_sha256(data,(size_t)n,out);
        free(data);
    }
    fclose(f);return ok;
}
static int open_save_temp(const char *path,char *tmp,size_t cap,FILE **out)
{
#ifdef _WIN32
    char dir[PATH_MAX],*slash;size_t n=strlen(path);
    if(n>=sizeof(dir))return 0;
    memcpy(dir,path,n+1);slash=strrchr(dir,'/');if(!slash)return 0;*slash=0;
    if(!GetTempFileNameA(dir,"avp",0,tmp))return 0;
    *out=fopen(tmp,"wb");if(!*out){DeleteFileA(tmp);return 0;}
    (void)cap;return 1;
#else
    int fd;
    if(strlen(path)+sizeof(".tmp.XXXXXX")>cap)return 0;
    strcpy(tmp,path);strcat(tmp,".tmp.XXXXXX");
    fd=mkstemp(tmp);if(fd<0)return 0;
    *out=fdopen(fd,"wb");if(!*out){close(fd);unlink(tmp);return 0;}
    return 1;
#endif
}
static int flush_save(FILE *f)
{
    if(fflush(f)!=0)return 0;
#ifdef _WIN32
    return _commit(_fileno(f))==0;
#else
    return fsync(fileno(f))==0;
#endif
}
static int replace_save(const char *tmp,const char *path)
{
#ifdef _WIN32
    return MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(tmp,path)==0;
#endif
}
static void discard_temp(const char *tmp)
{
#ifdef _WIN32
    DeleteFileA(tmp);
#else
    unlink(tmp);
#endif
}
static uint32_t checksum_parts(const unsigned char *core,size_t cn,
                               const unsigned char *maze,size_t mn,
                               const unsigned char *map,size_t sn)
{
    uLong c=crc32(0,Z_NULL,0);
    c=crc32(c,core,(uInt)cn);c=crc32(c,maze,(uInt)mn);c=crc32(c,map,(uInt)sn);
    return (uint32_t)c;
}
static int io_read(FILE *f,void *data,size_t n){return fread(data,1,n,f)==n;}
static int io_write(FILE *f,const void *data,size_t n){return fwrite(data,1,n,f)==n;}
static int within(uintptr_t p,uint64_t start,uint64_t count)
{ return (uint64_t)p>=start && (uint64_t)p-start<count; }

/* Refuse any archive that would leave a pointer to the previous process.
 * Pointers into game globals/code, the active maze, and ROM are relocated.
 * Host-owned immutable bindings already established at startup retain their
 * CURRENT pointers.  Pointers created only during the saved simulation have
 * no safe replacement and cause the whole archive load to fail unchanged. */
static int relocate(unsigned char *dest,const unsigned char *live,size_t n,
                    const QsDiskHeader *h,const AvpRomImage *rom,
                    const AvpMazeCandidate *maze)
{
    uint64_t new_image=loaded_image_base();
    uint64_t new_core=(uint64_t)(uintptr_t)__avp_game_state_start;
    uint64_t new_maze=(uint64_t)(uintptr_t)(maze->decoded+8);
    uint64_t new_rom=(uint64_t)(uintptr_t)rom->data;
    size_t pos;
    if(!new_image||!new_maze||!new_rom||!loaded_image_size()||
       h->old_image_size!=loaded_image_size())return 0;
    for(pos=0;pos+sizeof(uintptr_t)<=n;pos+=sizeof(uintptr_t)){
        uintptr_t v=0,current=0,patched=0;
        memcpy(&v,dest+pos,sizeof(v));
        if(!v)continue;
        if(within(v,h->old_core,h->core_bytes))patched=(uintptr_t)(new_core+((uint64_t)v-h->old_core));
        else if(within(v,h->old_image,h->old_image_size))patched=(uintptr_t)(new_image+((uint64_t)v-h->old_image));
        else if(within(v,h->old_maze-8u,h->maze_bytes+8u))
            patched=(uintptr_t)(new_maze+((uint64_t)v-h->old_maze));
        else if(within(v,h->old_rom,h->rom_size))patched=(uintptr_t)(new_rom+((uint64_t)v-h->old_rom));
        else if(((uint64_t)v>>32)==(h->old_image>>32)||
                ((uint64_t)v>>32)==(h->old_rom>>32)){
            memcpy(&current,live+pos,sizeof(current));
            /* Rebind host/resource allocations from the freshly initialized
             * process.  Refuse genuinely absent bindings rather than reusing
             * a stale pointer.  High-address integer fields (positions, flags)
             * are not treated as pointers solely because they have bit 46 set. */
            if((uint64_t)current<0x0000100000000000ull||
               (uint64_t)current>=0x0000800000000000ull)return 0;
            patched=current;
        }else continue; /* scalar integers, not native process addresses */
        memcpy(dest+pos,&patched,sizeof(patched));
    }
    return 1;
}

int avp_pc_quickstate_disk_save(const AvpPcQuickState *q,
    const AvpRomImage *rom,unsigned level,const void *map,size_t map_size)
{
    char path[PATH_MAX],tmp[PATH_MAX];QsDiskHeader h;FILE *f=NULL;
    int ok=0;unsigned char *core=NULL,*maze=NULL;size_t cn,mn;
    u64 ticks=0;
    if(!q||!rom||!rom->exact_retail_sha256||!map||map_size!=sizeof(AvpGpuAutomapSnapshot)||
       !avp_pc_quickstate_valid(q)||level<1u||level>15u||
       (!user_path(path,sizeof(path))&&!adjacent_path(path,sizeof(path))))return 0;
    cn=avp_pc_quickstate_core_bytes();
    mn=(size_t)avp_pc_quickstate_width(q)*avp_pc_quickstate_height(q)*8u;
    if(!cn||cn>1048576u||!mn||mn>32768u)return 0;
    core=(unsigned char*)malloc(cn);maze=(unsigned char*)malloc(mn);
    if(!core||!maze)goto done;
    if(!avp_pc_quickstate_export(q,core,cn,maze,mn,&ticks))goto done;
    memset(&h,0,sizeof(h));memcpy(h.magic,"AVPQS19",7);h.version=QS_DISK_VERSION;
    h.header_size=sizeof(h);h.word_bytes=sizeof(uintptr_t);h.core_bytes=(uint32_t)cn;
    h.maze_bytes=(uint32_t)mn;h.map_bytes=(uint32_t)map_size;
    h.width=avp_pc_quickstate_width(q);h.height=avp_pc_quickstate_height(q);
    h.level=level;h.old_image=loaded_image_base();
    h.old_image_size=loaded_image_size();
    if(!h.old_image||!h.old_image_size)goto done;
    h.old_core=(uint64_t)(uintptr_t)__avp_game_state_start;
    h.old_maze=avp_pc_quickstate_maze_source(q);
    h.old_rom=(uint64_t)(uintptr_t)rom->data;h.rom_size=rom->size;h.ticks=ticks;
    memcpy(h.rom_hash,rom->sha256_hex,65);
    if(!executable_hash(h.exe_hash))goto done;
    h.crc32_value=checksum_parts(core,cn,maze,mn,map,map_size);
    if(!open_save_temp(path,tmp,sizeof(tmp),&f))goto done;
    ok=io_write(f,&h,sizeof(h))&&io_write(f,core,cn)&&io_write(f,maze,mn)&&io_write(f,map,map_size);
    if(ok)ok=flush_save(f);
    if(fclose(f)!=0)ok=0;
    f=NULL;
    if(ok)ok=replace_save(tmp,path);
    if(!ok)discard_temp(tmp);
done:
    free(core);free(maze);return ok;
}

int avp_pc_quickstate_disk_load(AvpPcQuickState *q,
    const AvpRomImage *rom,const AvpMazeCandidate *mazes,unsigned maze_count,
    void *map,size_t map_size)
{
    char path[PATH_MAX];QsDiskHeader h;FILE *f=NULL;
    unsigned char exe_hash[AVP_PC_SHA256_LEN],*core=NULL,*maze_data=NULL,*map_data=NULL;
    const AvpMazeCandidate *target=NULL;size_t cn,mn;int ok=0;
    if(!q||!rom||!rom->exact_retail_sha256||!map||map_size!=sizeof(AvpGpuAutomapSnapshot)||
       (!user_path(path,sizeof(path))&&!adjacent_path(path,sizeof(path))))return 0;
    f=fopen(path,"rb");
    if(!f && adjacent_path(path,sizeof(path)))f=fopen(path,"rb");
    if(!f)return 0;
    if(!io_read(f,&h,sizeof(h))||memcmp(h.magic,"AVPQS19",7)!=0||
       h.version!=QS_DISK_VERSION||h.header_size!=sizeof(h)||
       h.word_bytes!=sizeof(uintptr_t)||h.core_bytes!=avp_pc_quickstate_core_bytes()||
       h.core_bytes>1048576u||h.maze_bytes>32768u||h.map_bytes!=map_size||
       h.level<1u||h.level>15u||h.rom_size!=rom->size||
       memcmp(h.rom_hash,rom->sha256_hex,65)!=0||
       !executable_hash(exe_hash)||memcmp(h.exe_hash,exe_hash,sizeof(exe_hash)))goto done;
    for(unsigned i=0;i<maze_count;i++)if(mazes[i].file_index==86u+h.level){target=&mazes[i];break;}
    if(!target||!target->decoded||target->width!=h.width||target->height!=h.height||
       h.maze_bytes!=(size_t)h.width*h.height*8u||
       h.old_maze<8u||h.old_image_size>QS_DISK_MAX_IMAGE)goto done;
    cn=h.core_bytes;mn=h.maze_bytes;
    core=(unsigned char*)malloc(cn);maze_data=(unsigned char*)malloc(mn);
    map_data=(unsigned char*)malloc(map_size);
    if(!core||!maze_data||!map_data)goto done;
    if(!io_read(f,core,cn)||!io_read(f,maze_data,mn)||!io_read(f,map_data,map_size)||
       fgetc(f)!=EOF||h.crc32_value!=checksum_parts(core,cn,maze_data,mn,map_data,map_size))goto done;
    if(!relocate(core,__avp_game_state_start,cn,&h,rom,target))goto done;
    /* Only publish a fully validated archive.  Existing in-memory quickstates
       remain available if disk validation fails. */
    if(!avp_pc_quickstate_import(q,core,cn,maze_data,mn,target->decoded+8,
                                 target->width,target->height,h.ticks))goto done;
    memcpy(map,map_data,map_size);ok=1;
done:
    if(f)fclose(f);
    free(core);free(maze_data);free(map_data);return ok;
}
