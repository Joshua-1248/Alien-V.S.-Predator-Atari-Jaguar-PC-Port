#include "map_maker_playtest.h"
#include "amp.h"
#include <stdio.h>
#include <string.h>
static int checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static const AvpAmpPlacement common[]={{128,128,21},{-1,0,0}};
static const AvpAmpPlacement human[]={{256,256,0},{-1,0,0}};
static const AvpAmpPlacement pred[]={{384,384,1},{-1,0,0}};
static const AvpAmpPlacement alien[]={{512,512,2},{-1,0,0}};
static const AvpAmpPlacement *original[]={human,pred,alien,common};
static const AvpAmpPlacement *installed[4];
const AvpAmpPlacement *avp_resources_retail_amp_list(unsigned group,unsigned level)
{return group<4u&&level==2u?original[group]:NULL;}
void avp_amp_bind_level_list(unsigned group,unsigned level,const AvpAmpPlacement *list)
{if(group<4u&&level==2u)installed[group]=list;}
/* Intercept the existing public door-metadata binding in this isolated
 * test executable.  The real game implementation is never linked here. */
static const AvpDoorLevelMeta *door_bound;
static unsigned door_bound_count,door_bind_calls;
void avp_doors_bind_level_meta(const AvpDoorLevelMeta *levels,unsigned count)
{door_bound=levels;door_bound_count=count;door_bind_calls++;}

static int verify_door_tables(const AvpDoorLevelMeta *actual,
                             const AvpDoorLevelMeta *expected,unsigned except_level)
{
    for(unsigned l=0;l<AVP_MM_LEVELS;l++){
        const AvpDoorLevelMeta *a=&actual[l],*b=&expected[l];
        if(l+1u==except_level)continue;
        if(a->access_count!=b->access_count || a->exit_count!=b->exit_count ||
           a->interlock_count!=b->interlock_count)return 0;
        for(size_t i=0;i<a->access_count;i++)
            if(a->access[i].door_offset!=b->access[i].door_offset ||
               a->access[i].code!=b->access[i].code)return 0;
        for(size_t i=0;i<a->exit_count;i++)
            if(a->exits[i].panel_offset!=b->exits[i].panel_offset ||
               a->exits[i].x!=b->exits[i].x ||
               a->exits[i].y!=b->exits[i].y ||
               a->exits[i].level!=b->exits[i].level)return 0;
        for(size_t i=0;i<a->interlock_count;i++){
            const AvpDoorInterlockEntry *ai=&a->interlocks[i],*bi=&b->interlocks[i];
            if(ai->door_offset!=bi->door_offset ||
               ai->required_count!=bi->required_count)return 0;
            for(size_t j=0;j<ai->required_count;j++)
                if(ai->required_offsets[j]!=bi->required_offsets[j])return 0;
        }
    }
    return 1;
}
int main(void)
{
    AvpMapMaker m;memset(&m,0,sizeof m);
    m.w=m.h=64u;m.level=2u;
    m.object_count=2;
    m.objects[0]=(AvpMmObjectRecord){(u16)(7u*256u+128u),(u16)(9u*256u+128u),21u,3u};
    m.objects[1]=(AvpMmObjectRecord){(u16)(6u*256u+128u),(u16)(10u*256u+128u),2u,0u};
    CHECK(avp_mm_playtest_bind_amp(&m));
    CHECK(installed[3]!=NULL && installed[0]!=NULL);
    CHECK(installed[1]==NULL && installed[2]==NULL);
    CHECK(installed[3][0].def==21u && installed[3][1].def==21u);
    CHECK(installed[3][1].x==(s16)(7u*256u+128u));
    CHECK(installed[3][2].x==-1);
    CHECK(installed[0][0].def==0u && installed[0][1].def==2u);
    CHECK(installed[0][2].x==-1);
    CHECK(common[1].x==-1 && human[1].x==-1); /* retail untouched */
    m.objects[0].def=60u;
    CHECK(!avp_mm_playtest_bind_amp(&m));
    CHECK(installed[3][1].def==21u); /* no replacement on invalid input */
    m.object_count=0;
    CHECK(avp_mm_playtest_bind_amp(&m));
    /* A fake retail 64x64 maze demonstrates exactly where the playtest
     * initial bootstrap and later transition loader install draft bytes. */
    static u8 retail_maze[8u+64u*64u*8u+8u];
    retail_maze[1]=retail_maze[3]=64u;
    memset(m.cells,0,sizeof m.cells);
    m.cells[5]=17u;
    CHECK(!avp_mm_playtest_overlay_maze(&m,1u,retail_maze,sizeof retail_maze));
    CHECK(!avp_mm_playtest_overlay_maze(&m,2u,retail_maze,7u));
    CHECK(retail_maze[8u+5u]==0u);
    CHECK(avp_mm_playtest_overlay_maze(&m,2u,retail_maze,sizeof retail_maze));
    CHECK(retail_maze[8u+5u]==17u);
    m.script_modified=1u;
    m.cells[5]=29u;
    CHECK(!avp_mm_playtest_overlay_maze(&m,2u,retail_maze,sizeof retail_maze));
    CHECK(retail_maze[8u+5u]==17u); /* no partial writes */
    m.script_modified=0u;
    CHECK(retail_maze[8u+64u*64u*8u]==0u); /* tail canary */
    /* R30: compare ALL rebuilt runtime records to the original typed
     * door_retail_meta.c table, including the untouched other 14 levels. */
    {
        static AvpMmPlaytestDoorMeta runtime;
        const AvpDoorLevelMeta *retail_tables;
        unsigned before_bind;
        m.level=1u;m.custom_map=0;m.script_modified=1u;
        m.script_count=avp_mm_script_copy_level(1u,m.script_rows,AVP_MM_MAX_SCRIPTS);
        CHECK(m.script_count==15u);
        CHECK(avp_mm_script_validate(m.script_rows,m.script_count,1u));
        avp_doors_bind_retail_meta();
        retail_tables=door_bound;
        CHECK(retail_tables!=NULL && door_bound_count==15u);
        CHECK(avp_mm_playtest_build_door_meta(&m,&runtime));
        CHECK(verify_door_tables(runtime.levels,retail_tables,0u));
        /* The R26 editor relocation: level 1 TOP->RIGHT moves the exit
         * 1169->1170 and access+interlock state 1175->1182. The required
         * offset 3223 stays in place and must reference the NEW interlock. */
        for(unsigned i=0;i<m.script_count;i++){
            AvpMmScriptRow *row=&m.script_rows[i];
            if(row->offset==1169u)row->offset=1170u;
            if(row->offset==1175u)row->offset=1182u;
            if(row->kind==4u && row->arg==1175)row->arg=1182;
        }
        CHECK(avp_mm_script_validate(m.script_rows,m.script_count,1u));
        /* The copied records alone are insufficient: a changed script
         * must point at a real authored panel and physical door owner. */
        CHECK(!avp_mm_playtest_validate_relocated_geometry(&m));
        m.cells[1170u]=0x20u;
        CHECK(avp_mm_playtest_validate_relocated_geometry(&m));
        CHECK(avp_mm_playtest_build_door_meta(&m,&runtime));
        CHECK(verify_door_tables(runtime.levels,retail_tables,1u));
        CHECK(runtime.levels[0].access[0].door_offset==1182u);
        CHECK(runtime.levels[0].access[0].code==AVP_ACC_EXIT);
        CHECK(runtime.levels[0].exits[4].panel_offset==1170u);
        CHECK(runtime.levels[0].exits[4].level==15u);
        CHECK(runtime.levels[0].interlocks[0].door_offset==1182u);
        CHECK(runtime.levels[0].interlocks[0].required_count==1u);
        CHECK(runtime.levels[0].interlocks[0].required_offsets[0]==3223u);
        /* Both the panel and door-state anchor must survive the edit. */
        m.cells[1170u]=0x80u; /* mirror-only is empty */
        CHECK(!avp_mm_playtest_validate_relocated_geometry(&m));
        m.cells[1170u]=0x10u; /* a non-door panel is not an airlock */
        CHECK(!avp_mm_playtest_validate_relocated_geometry(&m));
        m.cells[1170u]=0x20u;
        CHECK(avp_mm_playtest_validate_relocated_geometry(&m));
        before_bind=door_bind_calls;
        CHECK(avp_mm_playtest_bind_scripts(&m));
        CHECK(door_bind_calls==before_bind+1u);
        CHECK(door_bound_count==15u && door_bound!=retail_tables);
        CHECK(door_bound[0].interlocks[0].door_offset==1182u);
        CHECK(verify_door_tables(door_bound,retail_tables,1u));
        /* Unmodified playtest NEVER replaces retail tables. */
        m.script_modified=0u;before_bind=door_bind_calls;
        CHECK(avp_mm_playtest_bind_scripts(&m));
        CHECK(door_bind_calls==before_bind);
        m.script_modified=1u;
        /* Invalid access code or orphaned interlock must fail atomically. */
        m.script_rows[0].arg=99;before_bind=door_bind_calls;
        CHECK(!avp_mm_playtest_build_door_meta(&m,&runtime));
        CHECK(!avp_mm_playtest_bind_scripts(&m));
        CHECK(door_bind_calls==before_bind);
        m.script_rows[0].arg=AVP_ACC_EXIT;
        for(unsigned i=0;i<m.script_count;i++)if(m.script_rows[i].kind==4u)
            m.script_rows[i].arg=999;
        CHECK(!avp_mm_playtest_bind_scripts(&m));
        CHECK(door_bind_calls==before_bind);
        /* The altered draft maze is accepted only with valid scripts. */
        m.script_count=avp_mm_script_copy_level(1u,m.script_rows,AVP_MM_MAX_SCRIPTS);
        CHECK(avp_mm_playtest_overlay_maze(&m,1u,retail_maze,sizeof retail_maze));
        m.script_rows[0].arg=99;
        CHECK(!avp_mm_playtest_overlay_maze(&m,1u,retail_maze,sizeof retail_maze));
        CHECK(retail_maze[8u+64u*64u*8u]==0u);
        m.script_modified=0u;m.level=2u;
    }

    /* R31: a RIGHT-only door's live state is not saved by the retail
     * LEFT/TOP-only SaveDoors loop; the PC child remembers only those bytes. */
    {
        static AvpMapMaker door_draft;
        static u8 live[AVP_MM_MAX_BYTES];
        memset(&door_draft,0,sizeof door_draft);
        door_draft.level=3u;door_draft.w=door_draft.h=64u;
        door_draft.cells[1170u]=0x20u;
        live[1182u]=48u;
        CHECK(avp_mm_playtest_capture_noncanonical_door_states(
            &door_draft,3u,live,sizeof live)==1u);
        live[1182u]=0u;
        CHECK(avp_mm_playtest_restore_noncanonical_door_states(
            &door_draft,3u,live,sizeof live)==1u);
        CHECK(live[1182u]==48u);
        CHECK(avp_mm_playtest_restore_noncanonical_door_states(
            &door_draft,2u,live,sizeof live)==0u);
        CHECK(!avp_mm_playtest_capture_noncanonical_door_states(
            &door_draft,3u,live,10u));
        /* A matching canonical face is already managed by retail code. */
        door_draft.cells[1176u]=0x20u;live[1182u]=12u;
        CHECK(avp_mm_playtest_capture_noncanonical_door_states(
            &door_draft,3u,live,sizeof live)==0u);
        live[1182u]=0;
        CHECK(avp_mm_playtest_restore_noncanonical_door_states(
            &door_draft,3u,live,sizeof live)==0u);
        CHECK(live[1182u]==0u);
        /* BOTTOM face uses the southern TOP owner, not current cell. */
        door_draft.cells[1176u]=0u;door_draft.cells[1170u]=0u;
        door_draft.cells[1171u]=0x20u;live[1687u]=36u;
        CHECK(avp_mm_playtest_capture_noncanonical_door_states(
            &door_draft,3u,live,sizeof live)==1u);
        live[1687u]=0u;
        CHECK(avp_mm_playtest_restore_noncanonical_door_states(
            &door_draft,3u,live,sizeof live)==1u);
        CHECK(live[1687u]==36u);
    }

    /* Enemy-free mode is a map-test-only AMP filter. UI Object 0 is Food
     * (native AMP 21); native AMP 0 is Alien and must be filtered. */
    m.level=2u;m.w=m.h=64u;m.custom_map=0;
    m.object_count=2u;
    m.objects[0]=(AvpMmObjectRecord){(u16)(7u*256u+128u),(u16)(9u*256u+128u),21u,3u};
    m.objects[1]=(AvpMmObjectRecord){(u16)(8u*256u+128u),(u16)(9u*256u+128u),0u,0u};
    m.playtest_no_enemies=1;
    CHECK(avp_mm_playtest_bind_amp(&m));
    CHECK(installed[0]!=NULL && installed[0][0].x==-1); /* Alien filtered */
    CHECK(installed[1]!=NULL && installed[1][0].x==-1); /* Predator filtered */
    CHECK(installed[2]!=NULL && installed[2][0].x==-1); /* Human filtered */
    CHECK(installed[3]!=NULL && installed[3][0].def==21u); /* retail Food */
    CHECK(installed[3][1].def==21u && installed[3][2].x==-1); /* placed Food */
    CHECK(common[0].def==21u && human[0].def==0u); /* retail lists intact */
    m.playtest_no_enemies=0;m.object_count=0u;
    retail_maze[1]=32u;
    CHECK(!avp_mm_playtest_overlay_maze(&m,2u,retail_maze,sizeof retail_maze));
    retail_maze[1]=64u;
    m.custom_map=1; /* Blank custom map must not carry retail enemies. */
    CHECK(avp_mm_playtest_bind_amp(&m));
    for(unsigned g=0;g<4u;g++)CHECK(installed[g]&&installed[g][0].x==-1);
    /* RE34: a retail-derived draft launched as custom ID 16 also must not
     * inherit enemies, pickups or original door scripts. */
    m.custom_map=0;m.standalone_runtime=1u;
    CHECK(avp_mm_playtest_bind_amp(&m));
    for(unsigned g=0;g<4u;g++)CHECK(installed[g]&&installed[g][0].x==-1);
    CHECK(avp_mm_playtest_bind_scripts(&m));
    CHECK(door_bound_count==15u && door_bound[1].exit_count==0u &&
          door_bound[1].access_count==0u && door_bound[1].interlock_count==0u);
    avp_mm_playtest_clear_runtime_overrides();
    for(unsigned g=0;g<4u;g++)CHECK(installed[g]==original[g]);
    /* Repeated cleanup may not free retail-owned placement records. */
    avp_mm_playtest_clear_runtime_overrides();
    for(unsigned g=0;g<4u;g++)CHECK(installed[g]==original[g]);
    printf("Map maker playtest AMP overlay: %d PASS\n",checks);
    return 0;
}
