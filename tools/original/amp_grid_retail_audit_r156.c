/* RE38 R156: retail-only LEVELS.S placement-grid differential audit.
 * Compiled twice: once against the reconstructed original levels.c and once
 * against the PC port's levels.c, with the same retail ROM/maze input.
 * This file does not modify game behavior and uses no new/ bot modules.
 * The project and this audit do not redistribute the user's AVP.jag.
 */
#include "avp_resources.h"
#include "collision.h"
#include "amp.h"
#include "levels.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
u32 x_pos,y_pos,centre_angle;
static u8 *audit_maze;
static u16 audit_w,audit_h;
u8 *avp_collision_maze_data(void){return audit_maze;}
u16 avp_collision_maze_width(void){return audit_w;}
u16 avp_collision_maze_height(void){return audit_h;}

static uint64_t fnv_bytes(const void *p,size_t n){
    const uint8_t *b=(const uint8_t*)p;
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}
    return h;
}
/* Deterministic, endian-independent digest of the 16-bit grid values. */
static uint64_t grid_digest(void){
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<AVP_AMP_GRID_W*AVP_AMP_GRID_H;i++){
        u16 v=collmap[i];
        h^=(uint8_t)(v>>8);h*=UINT64_C(1099511628211);
        h^=(uint8_t)v;h*=UINT64_C(1099511628211);
    }
    return h;
}
static uint8_t *read_rom(const char *name,size_t *out_len){
    FILE *f=fopen(name,"rb");uint8_t *p;long n;
    if(!f)return NULL;
    if(fseek(f,0,SEEK_END)||((n=ftell(f))!=4194304L)||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    p=malloc((size_t)n);
    if(!p){fclose(f);return NULL;}
    if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*out_len=(size_t)n;return p;
}
static int selftest(void){
    u8 *raw=calloc(1u,64u*64u*8u);
    uint64_t first;unsigned open,banned;
    if(!raw)return 1;
    audit_maze=raw; audit_w=audit_h=64u;cur_level=2;
    x_pos=3u<<16;y_pos=4u<<16;
    /* An enclosed fully floored map has no failures other than the player's
     * reserved cell. Recomputing must be idempotent and leave the maze intact. */
    for(unsigned i=0;i<4096u;i++){
        for(unsigned w=0;w<4u;w++)raw[8u*i+w]=1u;
        raw[8u*i+5u]=1u;
    }
    uint64_t saved=fnv_bytes(raw,64u*64u*8u);
    place_grid();first=grid_digest();open=banned=0;
    for(unsigned i=0;i<4096u;i++){
        if(collmap[i]==0)open++;
        else if(collmap[i]==0xffffu)banned++;
        else {free(raw);return 2;}
    }
    if(open!=4095u||banned!=1u||fnv_bytes(raw,64u*64u*8u)!=saved){free(raw);return 3;}
    place_grid();
    if(grid_digest()!=first||fnv_bytes(raw,64u*64u*8u)!=saved){free(raw);return 4;}
    /* A floorless map must not expose any legal random spawn. */
    for(unsigned i=0;i<4096u;i++)raw[8u*i+5u]=0u;
    place_grid();
    for(unsigned i=0;i<4096u;i++)if(collmap[i]!=0xffffu){free(raw);return 5;}
    /* With no boundary walls, stage two/three fail-propagation excludes
     * everything despite valid floors. */
    for(unsigned i=0;i<4096u;i++){
        for(unsigned w=0;w<4u;w++)raw[8u*i+w]=0u;
        raw[8u*i+5u]=1u;
    }
    place_grid();
    for(unsigned i=0;i<4096u;i++)if(collmap[i]!=0xffffu){free(raw);return 6;}
    /* The two temporary authoring-wall exceptions for level three must be
     * restored byte-for-byte and survive repeated place_grid calls. */
    for(unsigned i=0;i<4096u;i++)for(unsigned w=0;w<4u;w++)raw[8u*i+w]=1u;
    cur_level=3;raw[(9u*64u+36u)*8u]=55u;raw[(10u*64u+36u)*8u]=77u;
    saved=fnv_bytes(raw,64u*64u*8u);place_grid();
    if(fnv_bytes(raw,64u*64u*8u)!=saved){free(raw);return 7;}
    free(raw);puts("R156 place_grid synthetic scenarios: 5/5 PASS");return 0;
}
int main(int argc,char **argv){
    if(argc==2 && strcmp(argv[1],"--selftest")==0)return selftest();
    size_t len=0;uint8_t *rom_bytes=NULL;unsigned total_open=0,total_sentinel=0,total_floor_sentinel=0;
    unsigned total_floorless_sentinel=0,total_spawn_cell=0;
    if(argc!=2){fprintf(stderr,"usage: %s <retail AVP.jag>\n",argv[0]);return 2;}
    rom_bytes=read_rom(argv[1],&len);
    if(!rom_bytes){fprintf(stderr,"expected user-provided 4194304-byte retail ROM\n");return 2;}
    AvpRomImage rom={0};rom.data=rom_bytes;rom.size=len;
    printf("level,file,open,excluded,excluded_with_floor,excluded_without_floor,player_floor_excluded,grid_fnv64,maze_restored\n");
    for(unsigned level=1;level<=15;level++){
        u8 *raw=NULL;size_t n=0;u16 w,h;unsigned open=0,excluded=0,with_floor=0,without_floor=0,spawn_cell=0;
        uint64_t maze_before,maze_after;unsigned file=86u+level;
        if(!avp_resources_decode_file(&rom,file,NULL,&raw,&n) || !raw || n<8u+64u*64u*8u){
            fprintf(stderr,"unable to decode maze file %u\n",file);free(raw);free(rom_bytes);return 1;
        }
        w=(u16)(((unsigned)raw[0]<<8)|raw[1]);h=(u16)(((unsigned)raw[2]<<8)|raw[3]);
        if(w!=64u||h!=64u){fprintf(stderr,"unexpected maze size %ux%u file=%u\n",w,h,file);free(raw);free(rom_bytes);return 1;}
        audit_maze=raw+8; audit_w=w; audit_h=h;
        /* Mark a deterministic authored floor as the player's cell.  The
         * original routine reserves this cell as $FFFF, independent of walls. */
        unsigned player_cell=4096u;
        for(unsigned i=0;i<4096u;i++)if(audit_maze[8u*i+5u]){player_cell=i;break;}
        if(player_cell==4096u){fprintf(stderr,"no floor in maze %u\n",level);free(raw);free(rom_bytes);return 1;}
        x_pos=(u32)(player_cell%64u)<<16;y_pos=(u32)(player_cell/64u)<<16;
        cur_level=(s16)level;
        maze_before=fnv_bytes(raw,n);
        place_grid();
        maze_after=fnv_bytes(raw,n);
        for(unsigned i=0;i<4096u;i++){
            u16 owner=collmap[i];int floor=audit_maze[8u*i+5u]!=0;
            if(!owner){open++;continue;}
            if(owner!=0xffffu){fprintf(stderr,"unexpected initial grid owner %04X file=%u cell=%u\n",owner,file,i);free(raw);free(rom_bytes);return 1;}
            excluded++;
            if(floor)with_floor++;else without_floor++;
            if(i==player_cell && floor)spawn_cell++;
        }
        if(maze_before!=maze_after){fprintf(stderr,"place_grid did not restore maze, level=%u\n",level);free(raw);free(rom_bytes);return 1;}
        if(open+excluded!=4096u||with_floor+without_floor!=excluded||spawn_cell!=1u){
            fprintf(stderr,"invalid classification level=%u\n",level);free(raw);free(rom_bytes);return 1;
        }
        printf("%u,%u,%u,%u,%u,%u,%u,%016llx,yes\n",level,file,open,excluded,with_floor,without_floor,spawn_cell,(unsigned long long)grid_digest());
        total_open+=open;total_sentinel+=excluded;total_floor_sentinel+=with_floor;
        total_floorless_sentinel+=without_floor;total_spawn_cell+=spawn_cell;
        free(raw);
    }
    printf("TOTAL,,%u,%u,%u,%u,%u,,all\n",total_open,total_sentinel,total_floor_sentinel,total_floorless_sentinel,total_spawn_cell);
    free(rom_bytes);return 0;
}
