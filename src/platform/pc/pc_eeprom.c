#include "pc/pc_eeprom.h"
#include "eeprom.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "pc/win32_posix.h"
#else
#include <unistd.h>
#endif

static FILE *g_eeprom_file;
static char g_eeprom_path[PATH_MAX];

static int eeprom_path_from_executable(const char *argv0,char *out,size_t cap)
{
    char exe[PATH_MAX];
    ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1u);
    char *slash;
    size_t dir_len;
    const char suffix[]="/avp_eeprom.bin";

    if(n>0 && (size_t)n<sizeof(exe))exe[n]=0;
    else if(argv0 && *argv0){
        if(strlen(argv0)>=sizeof(exe))return 0;
        memcpy(exe,argv0,strlen(argv0)+1u);
    }else{
        memcpy(exe,".",2u);
    }

    slash=strrchr(exe,'/');
    if(slash)*slash=0;
    else memcpy(exe,".",2u);
    dir_len=strlen(exe);
    if(dir_len+sizeof(suffix)>cap)return 0;
    memcpy(out,exe,dir_len);
    memcpy(out+dir_len,suffix,sizeof(suffix));
    return 1;
}

static int initialise_blank_file(FILE *f)
{
    unsigned i;
    if(!f)return 0;
    if(fseek(f,0,SEEK_SET)!=0)return 0;
    for(i=0;i<AVP_EE_SIZE;i++)if(fputc(0xff,f)==EOF)return 0;
    if(fflush(f)!=0)return 0;
    return fseek(f,0,SEEK_SET)==0;
}

int avp_pc_eeprom_open_for_executable(const char *argv0,char *out_path,size_t out_path_cap)
{
    long size;
    if(g_eeprom_file)avp_pc_eeprom_close();
    if(!eeprom_path_from_executable(argv0,g_eeprom_path,sizeof(g_eeprom_path)))return 0;

    g_eeprom_file=fopen(g_eeprom_path,"r+b");
    if(!g_eeprom_file){
        if(errno!=ENOENT)return 0;
        g_eeprom_file=fopen(g_eeprom_path,"w+b");
        if(!g_eeprom_file)return 0;
        if(!initialise_blank_file(g_eeprom_file)){avp_pc_eeprom_close();return 0;}
    }else{
        if(fseek(g_eeprom_file,0,SEEK_END)!=0){avp_pc_eeprom_close();return 0;}
        size=ftell(g_eeprom_file);
        if(size!=(long)AVP_EE_SIZE){
            /* Never truncate an existing save simply because its size is
             * unexpected.  Return failure, leave its bytes untouched, and
             * let the caller continue with volatile EEPROM if necessary.
             * A clean EEPROM is created only when the file did not exist. */
            avp_pc_eeprom_close();
            return 0;
        }
    }

    if(out_path&&out_path_cap){
        size_t n=strlen(g_eeprom_path);
        if(n+1u>out_path_cap){avp_pc_eeprom_close();return 0;}
        memcpy(out_path,g_eeprom_path,n+1u);
    }
    return 1;
}

void avp_pc_eeprom_close(void)
{
    if(g_eeprom_file){fclose(g_eeprom_file);g_eeprom_file=NULL;}
    g_eeprom_path[0]=0;
}

u16 avp_pc_eeprom_read_word(u16 index)
{
    unsigned char b[2];
    long off=(long)(index&63u)*2l;
    if(!g_eeprom_file)return 0xffffu;
    if(fseek(g_eeprom_file,off,SEEK_SET)!=0)return 0xffffu;
    if(fread(b,1u,2u,g_eeprom_file)!=2u)return 0xffffu;
    return (u16)(((u16)b[0]<<8)|b[1]);
}

void avp_pc_eeprom_write_word(u16 index,u16 value)
{
    unsigned char b[2];
    long off=(long)(index&63u)*2l;
    if(!g_eeprom_file)return;
    b[0]=(unsigned char)(value>>8);b[1]=(unsigned char)value;
    if(fseek(g_eeprom_file,off,SEEK_SET)!=0)return;
    if(fwrite(b,1u,2u,g_eeprom_file)!=2u)return;
    (void)fflush(g_eeprom_file);
}
