/* RE37 R122: Original-ROM retail maze *copy* experiment.
 * For every level shift the 62x62 INTERIOR one cell up and left inside the
 * same 64x64 allocation, removing its empty upper/left border. Preserve
 * the entire original maze bytes read-only. Exercises reconstructed retail
 * COLLLIDE.S and PC floor guard on unaltered and shifted coordinates.
 * Only aggregate counters are written, never proprietary map contents. */
#include "local_mp_retail_maps.h"
#include "local_mp_move_guard.h"
#include "collision.h"
#include "levels.h"
#include "doors.h"
#include "avp_resources.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define W 64u
#define SIZE (8u+W*W*8u)
#define CHECK(c) do{ if(!(c)){fprintf(stderr,"border audit FAIL %s line %d\n",#c,__LINE__);goto failed;} }while(0)
u32 levfx_ID;  /* The source door initializer's original global. */
static const unsigned char *cell(const unsigned char *b,unsigned x,unsigned y)
{return b+8u+((size_t)y*W+x)*8u;}
static unsigned char *mutable_cell(unsigned char *b,unsigned x,unsigned y)
{return b+8u+((size_t)y*W+x)*8u;}
int main(int argc,char **argv)
{
    FILE *fp=NULL,*csv=NULL;
    AvpRomImage rom={0};
    AvpMazeCandidate mazes[64]={{0}};
    unsigned found=0,seen=0,exposed_total=0,guard_mismatch_total=0,
             invalid_original_starts=0,invalid_rebased_starts=0;
    long sz;
    int status=1;
    if(argc!=3){fprintf(stderr,"usage: %s ORIGINAL_AVP.jag OUTPUT_COUNTERS.csv\n",argv[0]);return 2;}
    fp=fopen(argv[1],"rb");CHECK(fp);
    CHECK(fseek(fp,0,SEEK_END)==0);sz=ftell(fp);CHECK(sz==4194304L);
    CHECK(fseek(fp,0,SEEK_SET)==0);rom.size=(size_t)sz;
    rom.data=malloc(rom.size);CHECK(rom.data);
    CHECK(fread(rom.data,1,rom.size,fp)==rom.size);fclose(fp);fp=NULL;
    rom.exact_retail_sha256=1; /* Separate driver verifies SHA-256. */
    CHECK(avp_resources_bind_retail_level_info(&rom));
    found=avp_resources_find_mazes(&rom,mazes,64u);CHECK(found>=15u);
    InitDoors();
    csv=fopen(argv[2],"w");CHECK(csv);
    fputs("level,original_border_bytes_nonzero,shifted_boundary_floor,exposed_left_or_top_motion,original_start_rejected_after_shift,rebased_start_valid,edge_native_safepos_accepts_but_floor_guard_rejects,tight_width,tight_height,tight_original_start_invalid,tight_rebased_start_valid\n",csv);
    for(unsigned i=0;i<found;i++){
        const AvpMazeCandidate *m=&mazes[i];
        uint8_t *copy;
        u32 sx,sy,sa;
        unsigned original_border=0u,boundary_floor=0u,exposed=0u,guard_disagreement=0u;
        unsigned minx=W,miny=W,maxx=0u,maxy=0u,tw,th;
        int invalid_start,rebased_valid,tight_original_invalid,tight_rebased_valid;
        uint8_t *tight=NULL;
        if(m->file_index<87u||m->file_index>101u)continue;
        CHECK(m->decoded && m->decoded_size==SIZE && m->width==W && m->height==W);
        copy=calloc(1u,SIZE);CHECK(copy);
        memcpy(copy,m->decoded,8u);
        for(unsigned y=0;y<W;y++)for(unsigned x=0;x<W;x++){
            if(x==0u||y==0u||x==W-1u||y==W-1u)
                for(unsigned byte=0;byte<8u;byte++)original_border+=cell(m->decoded,x,y)[byte]!=0u;
            if(x>0u && y>0u && x<W-1u && y<W-1u)
                memcpy(mutable_cell(copy,x-1u,y-1u),cell(m->decoded,x,y),8u);
            if(cell(m->decoded,x,y)[5]){
                if(x<minx)minx=x;
                if(y<miny)miny=y;
                if(x>maxx)maxx=x;
                if(y>maxy)maxy=y;
            }
        }
        CHECK(original_border==0u); /* This is a per-level hypothesis check. */
        CHECK(minx<W&&miny<W&&maxx>=minx&&maxy>=miny);
        tw=maxx-minx+1u;th=maxy-miny+1u;
        tight=calloc(1u,8u+(size_t)tw*th*8u);CHECK(tight);
        tight[0]=(uint8_t)(tw>>8);tight[1]=(uint8_t)tw;
        tight[2]=(uint8_t)(th>>8);tight[3]=(uint8_t)th;
        tight[4]=m->decoded[4];tight[5]=m->decoded[5];
        tight[6]=m->decoded[6];tight[7]=m->decoded[7];
        for(unsigned y=0;y<th;y++)for(unsigned x=0;x<tw;x++)
            memcpy(tight+8u+((size_t)y*tw+x)*8u,
                   cell(m->decoded,x+minx,y+miny),8u);
        CHECK(avp_levels_start_pose(m->file_index-86u,&sx,&sy,&sa));
        CHECK(avp_mp_move_guard_floor(m->decoded+8u,W,W,(int32_t)sx,(int32_t)sy,40u<<9));
        invalid_start=!avp_mp_move_guard_floor(copy+8u,W,W,(int32_t)sx,(int32_t)sy,40u<<9);
        rebased_valid= sx >= 0x10000u && sy >= 0x10000u &&
            avp_mp_move_guard_floor(copy+8u,W,W,(int32_t)(sx-0x10000u),
                                     (int32_t)(sy-0x10000u),40u<<9);
        if(invalid_start)invalid_original_starts++;
        if(!rebased_valid)invalid_rebased_starts++;
        tight_original_invalid=!avp_mp_move_guard_floor(tight+8u,tw,th,(int32_t)sx,
                                                          (int32_t)sy,40u<<9);
        tight_rebased_valid=sx>=(minx<<16) && sy>=(miny<<16) &&
            avp_mp_move_guard_floor(tight+8u,tw,th,
                    (int32_t)(sx-(minx<<16)),(int32_t)(sy-(miny<<16)),40u<<9);
        CHECK(tight_rebased_valid);
        avp_collision_bind_maze(copy+8u,W,W);
        for(unsigned y=0;y<W;y++)for(unsigned x=0;x<W;x++){
            const unsigned char *c=cell(copy,x,y);
            unsigned char moves;
            if(!c[5])continue;
            if(x!=0u&&y!=0u&&x!=W-1u&&y!=W-1u)continue;
            ++boundary_floor;
            moves=AllowedMoves((u16)x,(u16)y);
            if((x==0u && (moves&1u))||(y==0u && (moves&2u)))++exposed;
            if(x==0u || y==0u){
                const int32_t probe_x=(int32_t)((x<<16)|(x==0u?0x1400u:0x8000u));
                const int32_t probe_y=(int32_t)((y<<16)|(y==0u?0x1400u:0x8000u));
                const int source_says_clear=SafePos(64u,probe_x,probe_y,40u)==0u;
                const int guard_says_clear=avp_mp_move_guard_floor(copy+8u,W,W,
                                         probe_x,probe_y,40u<<9);
                if(source_says_clear && !guard_says_clear)++guard_disagreement;
            }
        }
        avp_collision_bind_maze(NULL,0,0);
        /* No mutate, even header, of ROM-derived original maps. */
        CHECK(!memcmp(copy,m->decoded,8u));
        free(copy);
        free(tight);
        fprintf(csv,"%u,%u,%u,%u,%d,%d,%u,%u,%u,%d,%d\n",m->file_index-86u,
                original_border,boundary_floor,exposed,invalid_start,rebased_valid,
                guard_disagreement,tw,th,tight_original_invalid,tight_rebased_valid);
        exposed_total+=exposed;guard_mismatch_total+=guard_disagreement;
        ++seen;
    }
    CHECK(seen==15u);
    fprintf(stderr,"PASS: %u untouched retail ROM levels, %u exposed outer moves, %u native-SafePos/PC-floor-guard differences, %u stale starts rejected, %u rebased starts invalid.\n",
            seen,exposed_total,guard_mismatch_total,invalid_original_starts,invalid_rebased_starts);
    status=0;
failed:
    avp_collision_bind_maze(NULL,0,0);
    if(csv)fclose(csv);
    if(fp)fclose(fp);
    avp_resources_free_mazes(mazes,found<=64u?found:64u);
    free(rom.data);
    return status;
}
