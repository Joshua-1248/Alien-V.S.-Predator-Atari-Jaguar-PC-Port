#include "pc/pc_sha256.h"
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
int avp_pc_sha256(const void *data,size_t size,unsigned char out[AVP_PC_SHA256_LEN])
{
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE h=NULL;int ok=0;
    const unsigned char *p=(const unsigned char*)data;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)!=0)return 0;
    if(BCryptCreateHash(alg,&h,NULL,0,NULL,0,0)==0){
        ok=1;
        while(size){ /* BCryptHashData takes a ULONG length */
            ULONG chunk=size>0x40000000u?0x40000000u:(ULONG)size;
            if(BCryptHashData(h,(PUCHAR)p,chunk,0)!=0){ok=0;break;}
            p+=chunk;size-=chunk;
        }
        if(ok && BCryptFinishHash(h,out,AVP_PC_SHA256_LEN,0)!=0)ok=0;
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg,0);
    return ok;
}
#else
#include <openssl/sha.h>
int avp_pc_sha256(const void *data,size_t size,unsigned char out[AVP_PC_SHA256_LEN])
{
    return SHA256((const unsigned char*)data,size,out)!=NULL;
}
#endif
