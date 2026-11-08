#ifndef AVP_JAGUAR_AVP_GPU_AAF_COLUMNS_H
#define AVP_JAGUAR_AVP_GPU_AAF_COLUMNS_H

#include <stddef.h>

/* Original retail AAF frame: two signed edge bytes, then N big-endian
 * frame-relative 16-bit column offsets.  Zero means the column is absent.
 * Its value must never be used to read the frame header as CRY16 pixels. */
static inline int avp_gpu_aaf_column_offset_valid(size_t relative, unsigned columns)
{
    return relative >= 2u+(size_t)columns*2u;
}

#endif
