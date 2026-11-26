#ifndef AVP_PC_SHA256_H
#define AVP_PC_SHA256_H
#include <stddef.h>
/* Host SHA-256 used to authenticate the user's ROM: OpenSSL on Linux,
 * the operating system's BCrypt provider on Windows (no extra DLLs). */
#define AVP_PC_SHA256_LEN 32
int avp_pc_sha256(const void *data,size_t size,unsigned char out[AVP_PC_SHA256_LEN]);
#endif
