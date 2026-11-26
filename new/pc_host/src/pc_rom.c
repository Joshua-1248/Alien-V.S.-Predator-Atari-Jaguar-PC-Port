#include "pc/pc_platform.h"
#include "pc/pc_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AVP_RETAIL_ROM_SIZE 4194304u
static const char retail_sha256[] = "b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b";

static void hex_sha(const unsigned char d[AVP_PC_SHA256_LEN], char out[65])
{
    static const char h[]="0123456789abcdef";
    unsigned i;
    for(i=0;i<AVP_PC_SHA256_LEN;i++){out[i*2]=h[d[i]>>4];out[i*2+1]=h[d[i]&15u];}
    out[64]=0;
}

int avp_pc_rom_load(const char *path, AvpRomImage *rom, char *err, size_t errcap)
{
    FILE *f; long n; unsigned char digest[AVP_PC_SHA256_LEN];
    if(!rom||!path)return 0;
    memset(rom,0,sizeof(*rom));
    f=fopen(path,"rb");
    if(!f){if(err&&errcap)snprintf(err,errcap,"cannot open ROM: %s",path);return 0;}
    if(fseek(f,0,SEEK_END)!=0||(n=ftell(f))<0||fseek(f,0,SEEK_SET)!=0){fclose(f);if(err&&errcap)snprintf(err,errcap,"cannot size ROM");return 0;}
    if((unsigned long)n!=AVP_RETAIL_ROM_SIZE){fclose(f);if(err&&errcap)snprintf(err,errcap,"expected 4,194,304-byte retail .jag; got %ld bytes",n);return 0;}
    rom->data=(u8*)malloc((size_t)n);if(!rom->data){fclose(f);if(err&&errcap)snprintf(err,errcap,"out of memory reading ROM");return 0;}
    if(fread(rom->data,1,(size_t)n,f)!=(size_t)n){fclose(f);free(rom->data);memset(rom,0,sizeof(*rom));if(err&&errcap)snprintf(err,errcap,"short read from ROM");return 0;}
    fclose(f);rom->size=(size_t)n;
    if(!avp_pc_sha256(rom->data,rom->size,digest)){free(rom->data);rom->data=NULL;if(err&&errcap)snprintf(err,errcap,"SHA-256 provider unavailable");return 0;}hex_sha(digest,rom->sha256_hex);
    rom->exact_retail_sha256=!strcmp(rom->sha256_hex,retail_sha256);
    return 1;
}

void avp_pc_rom_free(AvpRomImage *rom){if(rom){free(rom->data);memset(rom,0,sizeof(*rom));}}
