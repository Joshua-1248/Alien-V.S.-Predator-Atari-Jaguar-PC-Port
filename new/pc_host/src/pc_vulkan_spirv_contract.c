#include "pc/pc_vulkan_spirv_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int avp_spirv_stage_contract(const uint32_t *words, size_t count, uint32_t model)
{
    size_t at=5;
    int memory_model=0, shader_capability=0, entrypoint=0;
    if (!words || count<5 || count>AVP_SPIRV_MAX_BYTES/sizeof(uint32_t) ||
        (model!=AVP_SPIRV_VERTEX && model!=AVP_SPIRV_FRAGMENT) ||
        words[0]!=0x07230203u || (words[1]>>16)!=1u ||
        (words[1]&0x0000ff00u)>0x00000600u ||
        words[3]==0u || words[4]!=0u) return 0;
    while (at<count) {
        uint32_t instruction=words[at], opcode=instruction&65535u;
        size_t n=(size_t)(instruction>>16);
        if (!n || n>count-at) return 0;
        if (opcode==17u && n>=2 && words[at+1]==1u) shader_capability=1;
        if (opcode==14u && n==3) memory_model=1;
        if (opcode==15u && n>=5 && words[at+1]==model &&
            words[at+2]!=0u && words[at+2]<words[3] &&
            words[at+3]==0x6e69616du && (words[at+4]&255u)==0u)
            entrypoint=1;
        at+=n;
    }
    return at==count && memory_model && shader_capability && entrypoint;
}

int avp_spirv_read_file(const char *path, uint32_t expected_model,
                        uint32_t **out, size_t *out_bytes)
{
    FILE *f=NULL;
    long length;
    uint32_t *words=NULL;
    if (!out || !out_bytes || !path || !*path || *out || *out_bytes) return 0;
    f=fopen(path,"rb");
    if (!f) return 0;
    if (fseek(f,0,SEEK_END)!=0 || (length=ftell(f))<0 ||
        length<20L || length>(long)AVP_SPIRV_MAX_BYTES || length%4L ||
        fseek(f,0,SEEK_SET)!=0) goto fail;
    words=malloc((size_t)length);
    if (!words || fread(words,1,(size_t)length,f)!=(size_t)length ||
        !avp_spirv_stage_contract(words,(size_t)length/4u,expected_model)) goto fail;
    if (fclose(f)!=0) { free(words); return 0; }
    *out=words;
    *out_bytes=(size_t)length;
    return 1;
fail:
    fclose(f);
    free(words);
    return 0;
}
