#include "avp_resources.h"
#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ROM_FILES is the beginning of the wider packed resource area, NOT the
 * beginning of FILES.DAT itself.  Historical FILES.DB loads AAFS, overlays,
 * SFX, pause/NF resources, etc. before NF.DB finally aligns .rom_files and
 * reads FILES.DAT.  Therefore the PC port discovers the validated tagged
 * FILES.DAT stream inside the retail ROM rather than assuming $816008 is
 * record zero. */
#define RETAIL_ROM_FILES_OFF 0x16008u

static u16 be16(const u8*p){return (u16)(((u16)p[0]<<8)|p[1]);}
static u32 be32(const u8*p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static int tag_ok(const u8*p){return !memcmp(p,"NONE",4)||!memcmp(p,"GZIP",4)||(p[0]=='J'&&p[1]=='P');}


int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 descriptor_addr,const u8 **data,size_t *size)
{
    size_t off,doff;u32 ptr,n;
    if(data)*data=NULL;
    if(size)*size=0;
    if(!rom||!rom->data||descriptor_addr<0x800000u)return 0;
    off=(size_t)(descriptor_addr-0x800000u);
    if(off+8u>rom->size)return 0;
    ptr=be32(rom->data+off);n=be32(rom->data+off+4u);
    if(ptr<0x800000u)return 0;
    doff=(size_t)(ptr-0x800000u);
    if(doff>rom->size||(size_t)n>rom->size-doff)return 0;
    if(data)*data=rom->data+doff;
    if(size)*size=(size_t)n;
    return 1;
}

static int record_next(const u8 *base,size_t size,size_t off,size_t *next)
{
    u32 n;size_t q;if(off+8u>size||!tag_ok(base+off))return 0;
    n=be32(base+off+4);if((size_t)n>size-off-8u)return 0;
    q=(off+8u+(size_t)n+3u)&~(size_t)3u;
    if(q<=off||q>size)return 0;
    *next=q;
    return 1;
}

/* Find the longest internally-valid FILES.DAT-style record chain.  Starting
 * at ROM_FILES is wrong: that address names the whole packed resource area.
 * FILES.DAT begins later, after several historical DB resource groups. */
int avp_resources_find_files_archive(const AvpRomImage *rom,size_t *start,unsigned *records)
{
    size_t best_off=0,best_bytes=0,off;unsigned best_count=0;
    if(start)*start=0;
    if(records)*records=0;
    if(!rom||!rom->data||rom->size<RETAIL_ROM_FILES_OFF+8u)return 0;
    for(off=RETAIL_ROM_FILES_OFF;off+8u<=rom->size;off+=4u){
        size_t cur,next;unsigned count=0;
        if(!tag_ok(rom->data+off))continue;
        cur=off;
        while(count<8192u&&record_next(rom->data,rom->size,cur,&next)){
            ++count;cur=next;
            if(cur+8u>rom->size||!tag_ok(rom->data+cur))break;
        }
        if(count>best_count||(count==best_count&&count&&cur-off>best_bytes)){
            best_count=count;best_off=off;best_bytes=cur-off;
        }
        /* A real archive is long; once found, skip its body during candidate
         * scanning so this remains cheap even on the 4 MiB image. */
        if(count>=16u&&cur>off+4u)off=(cur-4u)&~(size_t)3u;
    }
    if(best_count<4u)return 0;
    if(start)*start=best_off;
    if(records)*records=best_count;
    return 1;
}


#define RETAIL_AAF_START 0x16008u
static int decode_record(const u8 *rec,size_t avail,u8 **dst,size_t *dstn);

unsigned avp_resources_aaf_count(const AvpRomImage *rom)
{
    size_t off=RETAIL_AAF_START,next; unsigned count=0;
    if(!rom||!rom->data)return 0;
    while(off+8u<=rom->size&&count<1024u){
        const u8 *p=rom->data+off;
        if(memcmp(p,"NONE",4)&&memcmp(p,"GZIP",4))break;
        if(!record_next(rom->data,rom->size,off,&next))break;
        ++count;off=next;
    }
    return count;
}

int avp_resources_aaf_payload(const AvpRomImage *rom,unsigned aaf_index,const u8 **payload,size_t *payload_size)
{
    size_t off=RETAIL_AAF_START,next; unsigned i=0; u32 n;
    if(payload)*payload=NULL;
    if(payload_size)*payload_size=0;
    if(!rom||!rom->data)return 0;
    while(i<aaf_index){if(!record_next(rom->data,rom->size,off,&next))return 0;off=next;++i;}
    if(off+8u>rom->size)return 0;
    if(memcmp(rom->data+off,"NONE",4)&&memcmp(rom->data+off,"GZIP",4))return 0;
    n=be32(rom->data+off+4);if((size_t)n>rom->size-off-8u)return 0;
    if(payload)*payload=rom->data+off+8u;
    if(payload_size)*payload_size=n;
    return 1;
}

int avp_resources_decode_aaf(const AvpRomImage *rom,unsigned aaf_index,u8 **decoded,size_t *decoded_size)
{
    size_t off=RETAIL_AAF_START,next; unsigned i=0;
    if(decoded)*decoded=NULL;
    if(decoded_size)*decoded_size=0;
    if(!rom||!rom->data||!decoded||!decoded_size)return 0;
    while(i<aaf_index){if(!record_next(rom->data,rom->size,off,&next))return 0;off=next;++i;}
    if(off+8u>rom->size)return 0;
    if(memcmp(rom->data+off,"NONE",4)&&memcmp(rom->data+off,"GZIP",4))return 0;
    return decode_record(rom->data+off,rom->size-off,decoded,decoded_size);
}

static int decode_record(const u8 *rec,size_t avail,u8 **dst,size_t *dstn)
{
    u32 n;if(avail<8)return 0;n=be32(rec+4);if((size_t)n+8u>avail)return 0;
    if(!memcmp(rec,"NONE",4)){u8*p=malloc(n?n:1u);if(!p)return 0;memcpy(p,rec+8,n);*dst=p;*dstn=n;return 1;}
    if(!memcmp(rec,"GZIP",4)){
        const u8*src=rec+8;u32 isize;uLongf cap;u8*p;z_stream z;int rc;
        if(n<18)return 0;
        isize=(u32)src[n-4]|((u32)src[n-3]<<8)|((u32)src[n-2]<<16)|((u32)src[n-1]<<24);
        if(!isize||isize>8u*1024u*1024u)return 0;
        cap=isize;p=malloc(cap);if(!p)return 0;memset(&z,0,sizeof(z));z.next_in=(Bytef*)src;z.avail_in=n;z.next_out=p;z.avail_out=(uInt)cap;
        rc=inflateInit2(&z,15+16);if(rc!=Z_OK){free(p);return 0;}rc=inflate(&z,Z_FINISH);inflateEnd(&z);if(rc!=Z_STREAM_END){free(p);return 0;}*dst=p;*dstn=(size_t)z.total_out;return 1;
    }
    return 0;
}
static int maze_shape(const u8*p,size_t n,u16*w,u16*h)
{
    size_t need;unsigned sample=0,nonzero=0;if(n<8)return 0;*w=be16(p);*h=be16(p+2);if(*w<4||*h<4||*w>64||*h>64)return 0;need=8u+(size_t)(*w)*(*h)*8u;if(n<need)return 0;
    for(size_t i=8;i<need;i+=8){sample++;if(p[i]||p[i+1]||p[i+2]||p[i+3]||p[i+4]||p[i+5])nonzero++;}
    return sample&&nonzero*4u>sample;
}

unsigned avp_resources_find_mazes(const AvpRomImage*rom,AvpMazeCandidate*out,unsigned cap)
{
    size_t off;unsigned idx=0,count=0,record_count=0;if(!rom||!rom->data)return 0;
    if(!avp_resources_find_files_archive(rom,&off,&record_count))return 0;
    while(off+8u<=rom->size&&idx<record_count){const u8*rec=rom->data+off;size_t next;u8*d=NULL;size_t dn=0;u16 w=0,h=0;
        if(!record_next(rom->data,rom->size,off,&next))break;
        if((!memcmp(rec,"NONE",4)||!memcmp(rec,"GZIP",4))&&decode_record(rec,rom->size-off,&d,&dn)){
            int is_map=maze_shape(d,dn,&w,&h);
            /* Retail FILES 87..101 are the complete 15-map set.  Some duct/ship
               maps are sparse enough to fail the generic nonzero-density heuristic,
               so recognize their source-proven 0x8008/64x64/header shape exactly. */
            if(!is_map&&idx>=87u&&idx<=101u&&dn==0x8008u&&be16(d)==64u&&be16(d+2)==64u&&be16(d+4)==2u&&be16(d+6)==0u){w=64u;h=64u;is_map=1;}
            if(is_map){if(count<cap){out[count].file_index=idx;out[count].record_offset=off;out[count].width=w;out[count].height=h;out[count].decoded=d;out[count].decoded_size=dn;d=NULL;}count++;}
        }
        free(d);off=next;idx++;
    }
    return count;
}
void avp_resources_free_mazes(AvpMazeCandidate*m,unsigned count){unsigned i;if(!m)return;for(i=0;i<count;i++){free(m[i].decoded);m[i].decoded=NULL;}}


int avp_resources_file_payload(const AvpRomImage *rom,unsigned file_index,char tag[5],const u8 **payload,size_t *payload_size)
{
    size_t off,next;unsigned idx=0,record_count=0;u32 n;
    if(tag){memset(tag,0,5u);}
    if(payload)*payload=NULL;
    if(payload_size)*payload_size=0;
    if(!rom||!rom->data||!avp_resources_find_files_archive(rom,&off,&record_count)||file_index>=record_count)return 0;
    while(idx<file_index){if(!record_next(rom->data,rom->size,off,&next))return 0;off=next;++idx;}
    if(off+8u>rom->size||!tag_ok(rom->data+off))return 0;
    n=be32(rom->data+off+4);if((size_t)n>rom->size-off-8u)return 0;
    if(tag){memcpy(tag,rom->data+off,4u);tag[4]=0;}
    if(payload)*payload=rom->data+off+8u;
    if(payload_size)*payload_size=(size_t)n;
    return 1;
}


int avp_resources_decode_file(const AvpRomImage *rom,unsigned file_index,char tag[5],u8 **decoded,size_t *decoded_size)
{
    size_t off,next;unsigned idx=0,record_count=0;
    if(tag)memset(tag,0,5u);
    if(decoded)*decoded=NULL;
    if(decoded_size)*decoded_size=0;
    if(!rom||!rom->data||!decoded||!decoded_size||!avp_resources_find_files_archive(rom,&off,&record_count)||file_index>=record_count)return 0;
    while(idx<file_index){if(!record_next(rom->data,rom->size,off,&next))return 0;off=next;++idx;}
    if(off+8u>rom->size||!tag_ok(rom->data+off))return 0;
    if(tag){memcpy(tag,rom->data+off,4u);tag[4]=0;}
    return decode_record(rom->data+off,rom->size-off,decoded,decoded_size);
}


/* -------------------------------------------------------------------------
 * Retail AMP authored-placement bridge.
 *
 * Canonical retail ROM contains four consecutive 15-entry big-endian pointer
 * tables at ROM offset 0x15c48.  Correlation against AMP.S labels establishes
 * the table order as common_list, level_list (human), pred_level_list and
 * alien_level_list.  Each target is a stream of 6-byte records:
 *   s16 x_8_8, s16 y_8_8, u16 template_index
 * terminated by x == -1.  No game-owned bytes are shipped: these arrays are
 * reconstructed at runtime from the user's verified AVP.jag.
 */
#include "amp.h"
#include "levels.h"
#define RETAIL_AMP_LEVEL_PTRS_OFF 0x15c48u
#define RETAIL_ROM_BASE_ADDR      0x800000u

static u16 pc_be16(const u8 *p){return (u16)(((u16)p[0]<<8)|p[1]);}
static u32 pc_be32(const u8 *p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}

static const AvpAmpPlacement *g_retail_amp_lists[4][AVP_MAX_LEVEL];
const AvpAmpPlacement *avp_resources_retail_amp_list(unsigned kind,unsigned level)
{
    return kind<4u&&level>=1u&&level<=AVP_MAX_LEVEL?
           g_retail_amp_lists[kind][level-1u]:NULL;
}
int avp_resources_bind_retail_amp_placements(const AvpRomImage *rom,unsigned *out_lists,unsigned *out_entries)
{
    static AvpAmpPlacement *lists[4][AVP_MAX_LEVEL];
    static int built;
    unsigned g,l,total_lists=0,total_entries=0;
    /* AMP.S source order -> host enum kind. */
    static const unsigned kind_for_group[4]={AVP_AMP_LIST_COMMON,AVP_AMP_LIST_HUMAN,AVP_AMP_LIST_PREDATOR,AVP_AMP_LIST_ALIEN};
    if(out_lists)*out_lists=0;
    if(out_entries)*out_entries=0;
    if(!rom||!rom->data||!rom->exact_retail_sha256||rom->size<RETAIL_AMP_LEVEL_PTRS_OFF+4u*4u*AVP_MAX_LEVEL)return 0;
    if(!built){
        for(g=0;g<4;g++)for(l=0;l<AVP_MAX_LEVEL;l++){
            size_t po=RETAIL_AMP_LEVEL_PTRS_OFF+4u*(g*AVP_MAX_LEVEL+l);
            u32 addr=pc_be32(rom->data+po); size_t off,p; unsigned n=0;
            AvpAmpPlacement *a;
            if(addr<RETAIL_ROM_BASE_ADDR||addr>=RETAIL_ROM_BASE_ADDR+rom->size)return 0;
            off=(size_t)(addr-RETAIL_ROM_BASE_ADDR);p=off;
            /* First pass validates and counts. Coordinates are authored 8.8
             * positions inside the 64x64 retail map space; def is 0..59. */
            for(;;){
                s16 x,y;u16 d;
                if(p+6u>rom->size||n>AVP_NUM_AMPS)return 0;
                x=(s16)pc_be16(rom->data+p);y=(s16)pc_be16(rom->data+p+2);d=pc_be16(rom->data+p+4);
                if(x==-1)break;
                if(x<0||x>0x4000||y<0||y>0x4000||d>=60u)return 0;
                ++n;p+=6u;
            }
            a=(AvpAmpPlacement*)calloc((size_t)n+1u,sizeof(*a));if(!a)return 0;
            p=off;
            for(unsigned i=0;i<n;i++,p+=6u){a[i].x=(s16)pc_be16(rom->data+p);a[i].y=(s16)pc_be16(rom->data+p+2);a[i].def=pc_be16(rom->data+p+4);}
            a[n].x=-1;a[n].y=0;a[n].def=0;
            lists[g][l]=a;
        }
        built=1;
    }
    for(g=0;g<4;g++)for(l=0;l<AVP_MAX_LEVEL;l++){
        const AvpAmpPlacement *p=lists[g][l];unsigned n=0;
        if(!p)continue;
        g_retail_amp_lists[kind_for_group[g]][l]=p;
        avp_amp_bind_level_list(kind_for_group[g],l+1u,p);
        ++total_lists;
        while(p[n].x!=-1){++n;++total_entries;}
    }
    if(out_lists)*out_lists=total_lists;
    if(out_entries)*out_entries=total_entries;
    return total_lists==4u*AVP_MAX_LEVEL;
}


/* LEVELS.S::levels is linked at RDB.DTA address $24026.  RDB.DTA itself is
 * stored gzip-compressed in the retail cart beginning at ROM offset $E840 and
 * has historical load base $20300.  Decode the 15 pointers and their packed
 * `um_info` start fields at runtime so campaign spawn positions remain wholly
 * ROM-derived rather than being bundled as game data. */
int avp_resources_bind_retail_level_info(const AvpRomImage *rom)
{
    enum { RDB_BASE=0x20300u, LEVELS_ADDR=0x24026u, RDB_CAP=0x20000u };
    static AvpLevelInfo info[AVP_MAX_LEVEL];
    u8 *rdb;z_stream z;int rc;size_t n;
    if(!rom||!rom->data||!rom->exact_retail_sha256||rom->size<=0xe840u)return 0;
    rdb=(u8*)malloc(RDB_CAP);if(!rdb)return 0;
    memset(&z,0,sizeof(z));z.next_in=(Bytef*)(rom->data+0xe840u);z.avail_in=(uInt)(rom->size-0xe840u);z.next_out=rdb;z.avail_out=RDB_CAP;
    rc=inflateInit2(&z,31);if(rc!=Z_OK){free(rdb);return 0;}
    rc=inflate(&z,Z_FINISH);n=(size_t)z.total_out;inflateEnd(&z);if(rc!=Z_STREAM_END){free(rdb);return 0;}
    if(LEVELS_ADDR<RDB_BASE||(size_t)(LEVELS_ADDR-RDB_BASE)+AVP_MAX_LEVEL*4u>n){free(rdb);return 0;}
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        size_t po=(size_t)(LEVELS_ADDR-RDB_BASE)+i*4u;
        u32 p=pc_be32(rdb+po);size_t o;
        if(p<RDB_BASE){free(rdb);return 0;}o=(size_t)(p-RDB_BASE);
        if(o+14u>n){free(rdb);return 0;}
        info[i].maze_file=pc_be16(rdb+o);
        info[i].start_x=pc_be32(rdb+o+2u);info[i].start_y=pc_be32(rdb+o+6u);info[i].start_angle=pc_be32(rdb+o+10u);
        if(info[i].maze_file<87u||info[i].maze_file>101u){free(rdb);return 0;}
    }
    free(rdb);avp_levels_set_table(info,AVP_MAX_LEVEL);return 1;
}

/* RDB.DTA is stored as the gzip stream beginning at retail ROM offset $E840.
 * Its historical load base is $20300; callers that require the canonical ROM
 * remain responsible for enforcing the verified-hash policy. */
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **decoded,size_t *decoded_size)
{
    z_stream z;
    int rc;
    u8 *out;
    const size_t cap=0x20000u;
    if(decoded)*decoded=NULL;
    if(decoded_size)*decoded_size=0;
    if(!decoded||!decoded_size||!rom||!rom->data||rom->size<=0xe840u)return 0;
    out=(u8*)malloc(cap);
    if(!out)return 0;
    memset(&z,0,sizeof(z));
    z.next_in=(Bytef*)(rom->data+0xe840u);
    z.avail_in=(uInt)(rom->size-0xe840u);
    z.next_out=out;
    z.avail_out=(uInt)cap;
    rc=inflateInit2(&z,15+16);
    if(rc!=Z_OK){free(out);return 0;}
    rc=inflate(&z,Z_FINISH);
    inflateEnd(&z);
    if(rc!=Z_STREAM_END){free(out);return 0;}
    *decoded=out;
    *decoded_size=(size_t)z.total_out;
    return *decoded_size>0;
}

/* RDB.TXT is the first gzip payload embedded by ROM.RSP.  Do not hardcode
 * its cartridge offset: scan gzip members and select the one whose exact
 * expanded size matches the historical $8000..$202ff text segment. */
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **decoded,size_t *decoded_size)
{
    enum { RDB_TEXT_SIZE=0x18300u };
    size_t off;
    if(decoded)*decoded=NULL;
    if(decoded_size)*decoded_size=0;
    if(!decoded||!decoded_size||!rom||!rom->data||!rom->exact_retail_sha256)return 0;
    for(off=0;off+18u<rom->size;off++){
        z_stream z;u8 *out;int rc;
        if(rom->data[off]!=0x1fu||rom->data[off+1]!=0x8bu||rom->data[off+2]!=8u)continue;
        out=(u8*)malloc(RDB_TEXT_SIZE);
        if(!out)return 0;
        memset(&z,0,sizeof(z));
        z.next_in=(Bytef*)(rom->data+off);z.avail_in=(uInt)(rom->size-off);
        z.next_out=out;z.avail_out=RDB_TEXT_SIZE;
        rc=inflateInit2(&z,15+16);
        if(rc==Z_OK){rc=inflate(&z,Z_FINISH);inflateEnd(&z);}
        if(rc==Z_STREAM_END&&z.total_out==RDB_TEXT_SIZE){
            *decoded=out;*decoded_size=RDB_TEXT_SIZE;return 1;
        }
        free(out);
    }
    return 0;
}

const char *avp_resources_rdb_address(const u8 *rdb,size_t rdb_size,u32 address)
{
    size_t off;
    if(!rdb||address<0x20300u)return NULL;
    off=(size_t)(address-0x20300u);
    if(off>=rdb_size)return NULL;
    return (const char*)rdb+off;
}

/* Decode LEVELS.S::LoadPanels metadata from the user's RDB.DTA.  This binds
 * the exact per-level panel-number -> FILES.DAT id table used by DOORS.S and
 * derives only the special identities that DoorKeys compares.  No retail
 * panel ids are embedded in the executable. */
int avp_resources_retail_panel_info(const AvpRomImage *rom,unsigned level,AvpDoorPanelIds *out_ids,u8 out_list[AVP_MAX_PANELS],unsigned *out_count)
{
    enum { RDB_BASE=0x20300u, LEVELS_ADDR=0x24026u };
    u8 *rdb=NULL;
    size_t n=0,table_off,level_off,list_off,p;
    u32 level_addr,list_addr;
    unsigned panel_index=0;
    AvpDoorPanelIds ids;

    memset(&ids,0xff,sizeof(ids));
    u8 local_list[AVP_MAX_PANELS];
    memset(local_list,0xff,sizeof(local_list));
    if(level<1u||level>AVP_MAX_LEVEL||!rom||!rom->exact_retail_sha256)return 0;
    if(!avp_resources_decode_rdb(rom,&rdb,&n))return 0;
    table_off=(size_t)(LEVELS_ADDR-RDB_BASE)+(size_t)(level-1u)*4u;
    if(table_off+4u>n)goto fail;
    level_addr=be32(rdb+table_off);
    if(level_addr<RDB_BASE)goto fail;
    level_off=(size_t)(level_addr-RDB_BASE);
    /* level record: word maze, 3 longs start pose, long LoadPanels, long list */
    if(level_off+22u>n)goto fail;
    list_addr=be32(rdb+level_off+18u);
    if(list_addr<RDB_BASE)goto fail;
    list_off=(size_t)(list_addr-RDB_BASE);
    if(list_off+2u>n)goto fail;
    panel_index=be16(rdb+list_off);
    if(panel_index>AVP_MAX_PANELS)goto fail;
    p=list_off+2u;
    while(p+4u<=n){
        u32 loader=be32(rdb+p);p+=4u;
        if(loader==0xffffffffu)break;
        if(p+4u>n)goto fail;
        p+=4u; /* optional window-list pointer */
        for(;;){
            u8 id;
            if(p>=n)goto fail;
            id=rdb[p++];
            if(id==0xffu)break;
            if(panel_index>=AVP_MAX_PANELS)goto fail;
            local_list[panel_index++]=id;
        }
        if(p&1u)++p; /* assembler .even before the next dc.l group */
    }

    if(level<=5u){
        if(panel_index>=6u){
            ids.lift_back=local_list[2];
            ids.lift_side=local_list[3];
            ids.computer2=local_list[4];
            ids.wall2=local_list[5];
        }
        if((level==2u||level==4u)&&panel_index>=16u){
            ids.computer1=local_list[12];
            ids.medical_computer=local_list[15];
        }
    }else if(level>=6u&&level<=13u){
        if(panel_index>=2u)ids.airduct1=local_list[1];
    }
    if(out_ids)*out_ids=ids;
    if(out_count)*out_count=panel_index;
    if(out_list)memcpy(out_list,local_list,sizeof(local_list));
    free(rdb);
    return panel_index!=0u;
fail:
    free(rdb);
    return 0;
}

int avp_resources_bind_retail_panel_info(const AvpRomImage *rom,unsigned level,AvpDoorPanelIds *out_ids)
{
    AvpDoorPanelIds ids;
    u8 list[AVP_MAX_PANELS];
    unsigned original_count=0u;
    int ok=avp_resources_retail_panel_info(rom,level,&ids,list,&original_count);
    memset(panel_list,0xff,sizeof(panel_list));
    panel_pos=0;
    if(!ok){avp_doors_bind_panel_ids(NULL);return 0;}
    memcpy(panel_list,list,sizeof(panel_list));
    panel_pos=original_count;
    avp_doors_bind_panel_ids(&ids);
    if(out_ids)*out_ids=ids;
    return 1;
}
