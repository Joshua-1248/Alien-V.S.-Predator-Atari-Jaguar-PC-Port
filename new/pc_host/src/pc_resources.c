#include "pc/pc_resources.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include "pc/win32_posix.h"
#else
#include <unistd.h>
#endif

static int mk_dir_one(const char *p)
{
    if(mkdir(p,0755)==0||errno==EEXIST)return 1;
    return 0;
}

int avp_pc_prepare_rom_cache(const AvpRomImage *rom,const char *exe_path,char *out_dir,size_t out_dir_cap)
{
    char base[1024],root[1200],romdir[1280],filesdir[1340],aafdir[1340],manifest[1400];
    const char *slash; size_t off=0; unsigned records=0,i; FILE *mf; struct stat st;
    if(out_dir&&out_dir_cap)out_dir[0]=0;
    if(!rom||!rom->data||!rom->exact_retail_sha256||!avp_resources_find_files_archive(rom,&off,&records))return 0;
    if(exe_path&&*exe_path){snprintf(base,sizeof(base),"%s",exe_path);slash=strrchr(base,'/');if(slash)base[slash-base]=0;else snprintf(base,sizeof(base),".");}
    else snprintf(base,sizeof(base),".");
    snprintf(root,sizeof(root),"%s/cache",base);
    snprintf(romdir,sizeof(romdir),"%s/rom_%.16s",root,rom->sha256_hex);
    snprintf(filesdir,sizeof(filesdir),"%s/files",romdir);
    snprintf(aafdir,sizeof(aafdir),"%s/aaf",romdir);
    if(!mk_dir_one(root)||!mk_dir_one(romdir)||!mk_dir_one(filesdir)||!mk_dir_one(aafdir))return 0;
    snprintf(manifest,sizeof(manifest),"%s/manifest.txt",romdir);
    if(stat(manifest,&st)==0&&st.st_size>0){if(out_dir&&out_dir_cap)snprintf(out_dir,out_dir_cap,"%s",romdir);return 1;}
    mf=fopen(manifest,"w");if(!mf)return 0;
    fprintf(mf,"AVP Jaguar derived ROM cache\nROM_SHA256=%s\nFILES_RECORDS=%u\nAAF_RECORDS=%u\n",rom->sha256_hex,records,avp_resources_aaf_count(rom));
    fprintf(mf,"# Generated locally from the user's ROM. Do not redistribute this cache.\n");
    for(i=0;i<records;i++){
        char tag[5]={0},path[1500];const u8 *payload=NULL;size_t pn=0;u8 *decoded=NULL;size_t dn=0;FILE *f;const char *ext="bin";const u8 *bytes=NULL;size_t n=0;
        if(!avp_resources_file_payload(rom,i,tag,&payload,&pn))continue;
        if(tag[0]=='J'&&tag[1]=='P'){ext="jagpeg";bytes=payload;n=pn;}
        else if(avp_resources_decode_file(rom,i,tag,&decoded,&dn)){bytes=decoded;n=dn;ext="bin";}
        else {bytes=payload;n=pn;ext="raw";}
        snprintf(path,sizeof(path),"%s/%03u_%s.%s",filesdir,i,tag[0]?tag:"NONE",ext);
        f=fopen(path,"wb");if(f){if(n)fwrite(bytes,1,n,f);fclose(f);fprintf(mf,"%03u %s %zu %s\n",i,tag,n,path+strlen(romdir)+1);}
        free(decoded);
    }
    { unsigned ac=avp_resources_aaf_count(rom),ai;
      for(ai=0;ai<ac;ai++){const u8 *p=NULL;size_t n=0;char path[1500];FILE *f;
        if(!avp_resources_aaf_payload(rom,ai,&p,&n))continue;
        snprintf(path,sizeof(path),"%s/%03u.aaf",aafdir,ai);
        f=fopen(path,"wb");if(f){if(n)fwrite(p,1,n,f);fclose(f);fprintf(mf,"AAF %03u %zu %s\n",ai,n,path+strlen(romdir)+1);}
      }
    }
    fclose(mf);
    if(out_dir&&out_dir_cap)snprintf(out_dir,out_dir_cap,"%s",romdir);
    return 1;
}



/* A decoded FILES maze is intentionally writable while playing: the door
 * state machine updates its cell door bytes in place, and same-campaign
 * return visits use SaveDoors/ResetDoors to restore their prior state.
 * Starting a NEW campaign must first reload the original cartridge maze
 * bytes. Rebinding the existing mutable cache without this reset carries
 * opened doors across death -> new game, although InitDoors clears its
 * separate per-level bookkeeping. Decode all maps before overwriting any
 * cache entry so a failed ROM read cannot partially reset the campaign. */
int avp_pc_restore_new_campaign_mazes(const AvpRomImage *rom,AvpMazeCandidate *mazes,unsigned count)
{
    u8 *fresh[64]={0};
    unsigned i;
    int ok=0;
    if(!rom||!rom->data||!mazes||count>64u)return 0;
    for(i=0;i<count;i++){
        size_t len=0;
        if(!mazes[i].decoded || !mazes[i].decoded_size ||
           !avp_resources_decode_file(rom,mazes[i].file_index,NULL,&fresh[i],&len) ||
           !fresh[i] || len!=mazes[i].decoded_size)goto done;
    }
    for(i=0;i<count;i++)
        memcpy(mazes[i].decoded,fresh[i],mazes[i].decoded_size);
    ok=1;
done:
    for(i=0;i<count;i++)free(fresh[i]);
    return ok;
}

/* Decode BEFORE mutation so a failed read cannot partially alter the map. */
int avp_pc_restore_single_maze(const AvpRomImage *rom,AvpMazeCandidate *maze)
{
    u8 *fresh=NULL;
    size_t size=0;
    int ok=0;
    if(!rom || !rom->data || !maze || !maze->decoded ||
       !maze->decoded_size)return 0;
    if(avp_resources_decode_file(rom,maze->file_index,NULL,&fresh,&size) &&
       fresh && size==maze->decoded_size){
        memcpy(maze->decoded,fresh,size);
        ok=1;
    }
    free(fresh);
    return ok;
}
