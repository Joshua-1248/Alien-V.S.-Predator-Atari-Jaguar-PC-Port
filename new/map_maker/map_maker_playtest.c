#include "map_maker_playtest.h"
#include "amp.h"
#include "avp_resources.h"
#include <stdlib.h>
#include <string.h>

static AvpAmpPlacement *g_test_lists[4];
static AvpMmPlaytestDoorMeta g_playtest_door_meta;
/* Only child-process playtest state, never serialized or installed into
 * the original Jaguar game. Offsets are canonical door state bytes. */
static u8 g_noncanonical_saved[AVP_MM_MAX_BYTES];
static u8 g_noncanonical_valid[AVP_MM_MAX_BYTES];
static unsigned g_noncanonical_level;

static int mm_playtest_panel_is_door(u8 panel)
{
    unsigned type=(panel&0x7fu)>>3u;
    return type>=4u&&type<=11u;
}

unsigned avp_mm_playtest_capture_noncanonical_door_states(
    const AvpMapMaker *m,unsigned level,const u8 *maze,size_t bytes)
{
    unsigned count=0;
    if(!m||!maze||level!=m->level||!m->w||!m->h||
       m->w>AVP_MM_DIM||m->h>AVP_MM_DIM||
       bytes<(size_t)m->w*m->h*AVP_MM_CELL)return 0;
    memset(g_noncanonical_valid,0,sizeof g_noncanonical_valid);
    g_noncanonical_level=level;
    for(unsigned y=0;y<m->h;y++)for(unsigned x=0;x<m->w;x++){
        unsigned i=(y*m->w+x)*AVP_MM_CELL;
        if(x+1u<m->w && mm_playtest_panel_is_door(m->cells[i+2u])){
            unsigned neighbor=i+AVP_MM_CELL;
            if(!mm_playtest_panel_is_door(m->cells[neighbor])){
                unsigned at=neighbor+6u;
                if(!g_noncanonical_valid[at])count++;
                g_noncanonical_valid[at]=1u;g_noncanonical_saved[at]=maze[at];
            }
        }
        if(y+1u<m->h && mm_playtest_panel_is_door(m->cells[i+3u])){
            unsigned neighbor=i+(unsigned)m->w*AVP_MM_CELL;
            if(!mm_playtest_panel_is_door(m->cells[neighbor+1u])){
                unsigned at=neighbor+7u;
                if(!g_noncanonical_valid[at])count++;
                g_noncanonical_valid[at]=1u;g_noncanonical_saved[at]=maze[at];
            }
        }
    }
    return count;
}

unsigned avp_mm_playtest_restore_noncanonical_door_states(
    const AvpMapMaker *m,unsigned level,u8 *maze,size_t bytes)
{
    unsigned count=0;
    if(!m||!maze||level!=m->level||level!=g_noncanonical_level||
       !m->w||!m->h||m->w>AVP_MM_DIM||m->h>AVP_MM_DIM||
       bytes<(size_t)m->w*m->h*AVP_MM_CELL)return 0;
    for(unsigned i=0;i<(unsigned)m->w*m->h*AVP_MM_CELL;i++)
        if(g_noncanonical_valid[i]){maze[i]=g_noncanonical_saved[i];count++;}
    return count;
}


/* In DOORS.S a physical boundary may be authored from either cell.
 * Door motion state always has a canonical LEFT/TOP owner, even if the
 * selected visible face belongs to the neighbor on its opposite side. */
static int mm_playtest_state_has_door(const AvpMapMaker *m,unsigned offset)
{
    unsigned cell=offset/8u,field=offset&7u,x=cell%64u,y=cell/64u;
    unsigned near_panel,far_panel=0;
    if(field!=6u && field!=7u)return 0;
    near_panel=cell*8u+(field==6u?0u:1u);
    if(field==6u && x>0u)far_panel=(cell-1u)*8u+2u;
    if(field==7u && y>0u)far_panel=(cell-64u)*8u+3u;
    /* 4..11 are precisely the DOORS.S InitDoors door panel groups. */
    {unsigned group=(m->cells[near_panel]&0x7fu)>>3u;
     if(group>=4u && group<=11u)return 1;}
    if(far_panel){unsigned group=(m->cells[far_panel]&0x7fu)>>3u;
        if(group>=4u && group<=11u)return 1;}
    return 0;
}

int avp_mm_playtest_validate_relocated_geometry(const AvpMapMaker *m)
{
    AvpMmScriptRow original[AVP_MM_MAX_SCRIPTS];
    unsigned n;
    if(!m||m->custom_map||m->w!=64u||m->h!=64u||
       m->level<1u||m->level>AVP_MM_LEVELS)return 0;
    n=avp_mm_script_copy_level(m->level,original,AVP_MM_MAX_SCRIPTS);
    if(!n||!avp_mm_script_validate(m->script_rows,m->script_count,m->level))return 0;
    for(unsigned i=0;i<n;i++){
        const AvpMmScriptRow *row=&m->script_rows[i];
        if(row->offset==original[i].offset)continue;
        if(row->offset>=AVP_MM_MAX_BYTES)return 0;
        if(row->kind==2u){
            /* Exits are addressed by the actual renderable panel byte.
             * A cleared/mirror-only panel cannot activate that exit. */
            if((m->cells[row->offset]&0x7fu)==0u)return 0;
        }else if(!mm_playtest_state_has_door(m,row->offset)){
            /* Access / interlock / required-door rows use state bytes, not
             * a panel position. Never start a child with a dangling link. */
            return 0;
        }
    }
    return 1;
}

int avp_mm_playtest_build_door_meta(const AvpMapMaker *m, AvpMmPlaytestDoorMeta *out)
{
    unsigned access_used=0,exit_used=0,inter_used=0,required_used=0;
    if(!out || !m || m->level<1u || m->level>AVP_MM_LEVELS ||
       m->custom_map || !m->script_modified ||
       !avp_mm_playtest_validate_relocated_geometry(m))return 0;
    memset(out,0,sizeof(*out));
    for(unsigned level=1u;level<=AVP_MM_LEVELS;level++){
        AvpMmScriptRow originals[AVP_MM_MAX_SCRIPTS];
        const AvpMmScriptRow *rows;unsigned count;
        AvpDoorLevelMeta *meta=&out->levels[level-1u];
        if(level==m->level){rows=m->script_rows;count=m->script_count;}
        else {
            count=avp_mm_script_copy_level(level,originals,AVP_MM_MAX_SCRIPTS);
            rows=originals;
        }
        if(!count)return 0;
        meta->access=&out->access[access_used];
        meta->exits=&out->exits[exit_used];
        meta->interlocks=&out->interlocks[inter_used];
        for(unsigned i=0;i<count;i++){
            const AvpMmScriptRow *row=&rows[i];
            if(row->level!=level)return 0;
            switch(row->kind){
            case 1u:
                if(access_used>=AVP_MM_MAX_SCRIPTS)return 0;
                out->access[access_used++]=(AvpDoorAccessEntry){row->offset,row->arg};
                meta->access_count++;
                break;
            case 2u:
                if(exit_used>=AVP_MM_MAX_SCRIPTS)return 0;
                out->exits[exit_used++]=(AvpDoorExitEntry){
                    row->offset,row->destination_x,row->destination_y,row->destination_level};
                meta->exit_count++;
                break;
            case 3u:{
                if(inter_used>=AVP_MM_MAX_SCRIPTS)return 0;
                AvpDoorInterlockEntry *lock=&out->interlocks[inter_used++];
                lock->door_offset=row->offset;
                lock->required_offsets=&out->required[required_used];
                for(unsigned j=0;j<count;j++)if(rows[j].kind==4u &&
                     (u16)rows[j].arg==row->offset){
                    if(required_used>=AVP_MM_MAX_SCRIPTS)return 0;
                    out->required[required_used++]=rows[j].offset;
                    lock->required_count++;
                }
                if(!lock->required_count)return 0;
                meta->interlock_count++;
                break;
            }
            case 4u:{
                int owner=0;
                for(unsigned j=0;j<count;j++)if(rows[j].kind==3u &&
                    rows[j].offset==(u16)row->arg)owner=1;
                if(!owner)return 0;
                break;
            }
            default:return 0;
            }
        }
    }
    return 1;
}

int avp_mm_playtest_bind_scripts(const AvpMapMaker *m)
{
    if(!m)return 0;
    if(!m->script_modified)return 1; /* Preserve exact retail binding. */
    if(!avp_mm_playtest_build_door_meta(m,&g_playtest_door_meta))return 0;
    avp_doors_bind_level_meta(g_playtest_door_meta.levels,AVP_MM_LEVELS);
    return 1;
}

/* AMP definitions with live hostile behavior, including eggs and lurking
 * enemies. Dead bodies, pickups and ordinary scenery remain in playtest.
 * This is only called for the isolated --map-test child process. */
static int mm_playtest_is_enemy(unsigned def)
{
    return def<=5u || (def>=29u && def<=37u) ||
           def==50u || def==51u || def==58u || def==59u;
}
int avp_mm_playtest_bind_amp(const AvpMapMaker *m)
{
    unsigned retail_count[4]={0},added[4]={0},count[4];
    const AvpAmpPlacement *retail[4]={0};
    AvpAmpPlacement *new_lists[4]={0};
    if(!m||m->level<1u||m->level>AVP_MM_LEVELS ||
       m->object_count>AVP_MM_MAX_OBJECTS)return 0;
    if(!m->object_count && !m->custom_map && !m->playtest_no_enemies)return 1;
    for(unsigned i=0;i<m->object_count;i++){
        const AvpMmObjectRecord *a=&m->objects[i];
        if(a->group>3u||a->def>=60u||a->x8>=m->w*256u||
           a->y8>=m->h*256u)return 0;
        if(!(m->playtest_no_enemies && mm_playtest_is_enemy(a->def)))added[a->group]++;
    }
    for(unsigned g=0;g<4u;g++){
        /* A custom blank map must not inherit the original campaign's
         * enemy/object placements. A retail-derived draft still does. */
        if(!m->custom_map){
            retail[g]=avp_resources_retail_amp_list(g,m->level);
            if(!retail[g])return 0;
            unsigned i=0u;
            while(i<AVP_NUM_AMPS&&retail[g][i].x!=-1){
                if(!m->playtest_no_enemies||!mm_playtest_is_enemy(retail[g][i].def))
                    retail_count[g]++;
                ++i;
            }
            if(i>=AVP_NUM_AMPS)return 0;
        }
        count[g]=retail_count[g]+added[g];
        if(count[g]>AVP_NUM_AMPS)return 0;
    }
    /* Retail level_loop uses COMMON and append_objs adds two of the three
     * species lists. Validate worst-case occupancy before installing any. */
    unsigned total=count[3u]+count[0u]+count[1u]+count[2u];
    unsigned least=count[0u];
    if(count[1u]<least)least=count[1u];
    if(count[2u]<least)least=count[2u];
    if(total-least>AVP_NUM_AMPS)return 0;
    for(unsigned g=0;g<4u;g++)if(added[g]||m->custom_map||m->playtest_no_enemies){
        unsigned at=0;
        AvpAmpPlacement *list=(AvpAmpPlacement*)calloc((size_t)count[g]+1u,sizeof(*list));
        if(!list){for(unsigned i=0;i<4u;i++)free(new_lists[i]);return 0;}
        new_lists[g]=list;
        if(retail[g])for(unsigned j=0;retail[g][j].x!=-1;j++){
            if(!m->playtest_no_enemies||!mm_playtest_is_enemy(retail[g][j].def))
                list[at++]=retail[g][j];
        }
        for(unsigned j=0;j<m->object_count;j++)if(m->objects[j].group==g){
            const AvpMmObjectRecord *a=&m->objects[j];
            if(m->playtest_no_enemies && mm_playtest_is_enemy(a->def))continue;
            list[at].x=(s16)a->x8;list[at].y=(s16)a->y8;
            list[at++].def=a->def;
        }
        list[at].x=-1;list[at].y=0;list[at].def=0;
    }
    for(unsigned g=0;g<4u;g++)if(new_lists[g]){
        free(g_test_lists[g]);g_test_lists[g]=new_lists[g];
        avp_amp_bind_level_list(g,m->level,new_lists[g]);
    }
    return 1;
}

int avp_mm_playtest_overlay_maze(const AvpMapMaker *m,unsigned level,
                                  u8 *decoded,size_t decoded_size)
{
    size_t bytes;
    /* Moved metadata must be valid before any draft maze byte is installed.
     * The child process binds the separate metadata table before campaign
     * initialization; this overlay only changes its private decoded maze. */
    if(!m||!decoded||level!=m->level||!m->w||!m->h||
       m->w>64u||m->h>64u)return 0;
    if(m->script_modified &&
       !avp_mm_playtest_validate_relocated_geometry(m))return 0;
    bytes=(size_t)m->w*m->h*8u;
    if(decoded_size<8u+bytes||
       ((unsigned)decoded[0]<<8|decoded[1])!=m->w||
       ((unsigned)decoded[2]<<8|decoded[3])!=m->h)return 0;
    memcpy(decoded+8u,m->cells,bytes);
    return 1;
}
