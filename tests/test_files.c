#include "files.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    u32 words[32]={0};
    u8 *buf=(u8 *)words,output[8]={0};
    size_t written=0;
    /* Two valid aligned FILES.DAT records, including a padded short file. */
    memcpy(buf+8,"NONE",4);buf[15]=3;
    memcpy(buf+16,"abc",3);
    memcpy(buf+20,"NONE",4);buf[27]=1;buf[28]='Z';
    avp_files_bind(buf+8,24);
    assert(GetFile(0)==buf+8);
    assert(TransFil(buf+8,output,sizeof(output),&written));
    assert(written==3&&!memcmp(output,"abc",3));
    assert(GetFPos()==buf+20);
    assert(GetFile(0)==buf+20);
    assert(TransFil(buf+20,output,sizeof(output),&written));
    assert(written==1&&output[0]=='Z');
    assert(GetFile(0)==NULL);
    /* A cursor outside the active bound archive must not be parsed as a
     * valid record even if there is accessible data at that pointer. */
    memcpy(buf,"NONE",4);buf[7]=4;
    avp_files_bind(buf+8,24);
    SetFPos(buf);
    assert(GetFile(0)==NULL);
    SetFPos(buf+9);
    assert(GetFile(0)==NULL);
    SetFPos(buf+8);
    /* Truncated header and payload, oversized lengths, incomplete padding. */
    avp_files_bind(buf+8,7);
    assert(GetFile(0)==NULL);
    avp_files_bind(buf+8,10);
    assert(GetFile(0)==NULL);
    memset(buf+8,0,12);
    memcpy(buf+8,"NONE",4);
    buf[12]=buf[13]=buf[14]=buf[15]=0xff;
    avp_files_bind(buf+8,12);
    assert(GetFile(0)==NULL);
    memset(buf+8,0,12);memcpy(buf+8,"NONE",4);
    buf[15]=1;buf[16]='Q';
    avp_files_bind(buf+8,9); /* one data byte, no alignment padding */
    assert(GetFile(0)==NULL);
    /* A corrupt size must not wrap the archive's computed end pointer. */
    avp_files_bind(buf+8,SIZE_MAX);
    assert(GetFile(0)==NULL);
    avp_files_bind(NULL,0);
    assert(GetFile(0)==NULL);
    return 0;
}
