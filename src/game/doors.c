/* Readable C reconstruction of the active ordinary-68000 control flow in
 * MAZE/DOORS.S.
 *
 * The authored LEVDOOR.S access/exit/interlock tables and the per-level
 * panel-file-id mapping are supplied as typed resource metadata.  Door/lift/
 * duct/access state machines, byte stepping, collision reversal and level
 * transition decisions remain in C exactly because they are 68000 gameplay
 * behavior, not renderer/backend behavior.
 */
#include "doors.h"
#include "amp.h"
#include "hud_message.h"
#include "joypad.h"
#include "levels.h"
#include "maze.h"
#include "music.h"
#include "avp_sfx_ids.h"
#include "player.h"
#include "avp_runtime.h"
#include <stddef.h>
#include <string.h>

#define CELL 8u
#define LEFT_WALL 0u
#define TOP_WALL 1u
#define RIGHT_WALL 2u
#define BOTTOM_WALL 3u
#define LEFT_DOOR 6u
#define TOP_DOOR 7u
#define DOOR_STEP 4
#define DOOR_LIM_DEFAULT 48u
#define D2_SPECIAL (AVP_WB_LEFTONLY|AVP_WB_EDGEDOOR|AVP_WB_TBDOOR)
#define DC_SIZE 0x4800u
#define DC_MIN 8u
#define LIFTDOR 64u
#define DUCT_EDGE ((40u+1u)<<9) /* PLAYER_WIDTH == 40 in PLAYER.INC */

/* Semantic sound tokens used by the portable boundary.  The original source
 * names are preserved in comments; a host resource layer may remap them to
 * the user's ROM-derived sound table. */
enum {
    DSFX_RISE2=AVP_FX_RISE2, DSFX_DDOORS=AVP_FX_DDOOR, DSFX_FLUP1=AVP_FX_FLUP1, DSFX_EL10X=AVP_FX_EL10X,
    DSFX_AIRLOCK=AVP_FX_AIRLO, DSFX_ACCD=AVP_FX_ACCD, DSFX_ACCG=AVP_FX_ACCG
};

u8 fullbits[256];
u8 acs_level;
u8 lift_key,comp_panel;
u16 comp_offset;
/* Local multiplayer adapter only; does not modify original Jaguar behavior. */
static int mp_dm_no_terminal,mp_dm_no_travel;
void avp_doors_mp_restrict_interactions(int deathmatch,int level_travel)
{
    mp_dm_no_terminal=!!deathmatch;
    mp_dm_no_travel=!!deathmatch && !level_travel;
}

static u8 *maze_data;
static u16 maze_w,maze_h;
static u8 l2l_area[AVP_L2L_SIZE];
static u16 l2l_used;
static s16 l2l_off[AVP_MAX_LEVEL];
/* Reserve a small bookkeeping trailer INSIDE the existing Jaguar door-state
 * arena. No new BSS or quicksave fields are introduced by this host safety
 * guard, so pre-existing quickstate core sizes remain unchanged. */
#define L2L_META_BYTES (AVP_MAX_LEVEL*2u)
#define L2L_DOOR_BYTES (AVP_L2L_SIZE-L2L_META_BYTES)
static unsigned l2l_span_length(unsigned level)
{
    unsigned offset=L2L_DOOR_BYTES+level*2u;
    return ((unsigned)l2l_area[offset]<<8)|l2l_area[offset+1u];
}
static void l2l_store_length(unsigned level,unsigned length)
{
    unsigned offset=L2L_DOOR_BYTES+level*2u;
    l2l_area[offset]=(u8)(length>>8);
    l2l_area[offset+1u]=(u8)length;
}

static const AvpDoorLevelMeta *level_meta;
static unsigned level_meta_count;
static const AvpDoorLevelMeta *cur_meta;
static AvpDoorPanelIds panel_ids={0xffu,0xffu,0xffu,0xffu,0xffu,0xffu,0xffu};

/* DOORS.S stores these fields contiguously after each active pointer. */
typedef struct DoorState {
    u8 *ptr;
    s8 dir;
    u8 lim;
    s32 xpos,ypos;       /* standard door line in 16.16 room coordinates */
    u16 index;           /* 0 => x axis, 4 => y axis in the original ABI */
    u16 grid1,grid2;     /* one-based AMP collision grid neighbors */
} DoorState;
static DoorState door1,door2;
static u8 dbl_door;
static s8 lift_dir;
static u8 *lift_ptr;
static u16 lift_x,lift_y;
static unsigned host_exit_kind;
void avp_doors_host_clear_exit_kind(void){host_exit_kind=AVP_DOORS_EXIT_NONE;}
unsigned avp_doors_host_exit_kind(void){return host_exit_kind;}

static const u8 init_wbits[16]={
    AVP_WB_SOLID,AVP_WB_SOLID,AVP_WB_SOLID,AVP_WB_SOLID,
    AVP_WB_DOOR|AVP_WB_2SIDES,
    AVP_WB_DOOR,
    AVP_WB_DOOR,
    AVP_WB_DOOR|AVP_WB_TBDOOR|AVP_WB_EDGEDOOR,
    AVP_WB_DOOR|AVP_WB_TBDOOR,
    AVP_WB_DOOR|AVP_WB_TBDOOR|AVP_WB_EDGEDOOR|AVP_WB_LEFTONLY,
    AVP_WB_DOOR|AVP_WB_TBDOOR|AVP_WB_EDGEDOOR|AVP_WB_2SIDES,
    AVP_WB_DOOR|AVP_WB_TBDOOR|AVP_WB_2SIDES,
    0,AVP_WB_LEFTONLY,AVP_WB_LEFTONLY,AVP_WB_2SIDES
};

void avp_doors_bind_maze(u8 *maze,u16 width,u16 height)
{ maze_data=maze;maze_w=width;maze_h=height; }
void avp_doors_bind_level_meta(const AvpDoorLevelMeta *levels,unsigned count)
{ level_meta=levels;level_meta_count=count;ResetAccess(); }
void avp_doors_bind_panel_ids(const AvpDoorPanelIds *ids)
{ if(ids)panel_ids=*ids;else memset(&panel_ids,0xff,sizeof(panel_ids)); }

int avp_doors_cell_is_liftside(s32 xpos,s32 ypos)
{
    /* AMP.S::stun_mode tests the TOP_WALL panel of the AMP's current cell,
     * masks reflection, then resolves through panel_list to the original file
     * number and compares it with liftside. */
    unsigned x=(u32)xpos>>16,y=(u32)ypos>>16;
    u8 panel,clean,file_id;
    if(!maze_data||x>=maze_w||y>=maze_h)return 0;
    panel=maze_data[((size_t)y*maze_w+x)*CELL+TOP_WALL];
    clean=(u8)(panel&0x7fu);
    file_id=(clean>=1u&&clean<=AVP_MAX_PANELS)?panel_list[clean-1u]:0xffu;
    return panel_ids.lift_side!=0xffu && file_id==panel_ids.lift_side;
}

static void play(unsigned id){const AvpRuntimeOps *o=avp_runtime_ops();if(o->play_sfx)o->play_sfx(o->user,id);}
static int pressed(u32 v,unsigned bit){return (v&(1u<<bit))==0u;}
static int panel_id_is(u8 file_id,u8 special){return special!=0xffu && file_id==special;}

void InitAccess(void){acs_level=(u8)(player_type==PT_HUMAN?0:10);}
void ResetAccess(void)
{
    cur_meta=NULL;
    if(level_meta&&cur_level>=1&&(unsigned)cur_level<=level_meta_count)
        cur_meta=&level_meta[(unsigned)cur_level-1u];
}

void InitDoors(void)
{
    unsigned type,n;
    levfx_ID=0;
    for(type=0;type<16u;++type)for(n=0;n<8u;++n){
        fullbits[type*8u+n]=init_wbits[type];
        fullbits[0x80u+type*8u+n]=init_wbits[type];
    }
    fullbits[0]=0;fullbits[0x80]=0;
    for(n=0;n<AVP_MAX_LEVEL;++n){l2l_off[n]=-1;l2l_store_length(n,0);}
    l2l_used=0;
    memset(&door1,0,sizeof door1);memset(&door2,0,sizeof door2);
    dbl_door=0;comp_panel=0;lift_key=0;lift_dir=0;lift_ptr=NULL;lift_x=lift_y=0;
}

static unsigned door_byte_count(void)
{
    unsigned cells=(unsigned)maze_w*(unsigned)maze_h,i,count=0;
    if(!maze_data)return 0;
    for(i=0;i<cells;++i){const u8 *c=maze_data+i*CELL;
        if(fullbits[c[LEFT_WALL]]&AVP_WB_DOOR)++count;
        if(fullbits[c[TOP_WALL]]&AVP_WB_DOOR)++count;
    }
    return count;
}
void SaveDoors(void)
{
    unsigned i,cells,need,pos=0;u8 *out;
    if(!maze_data||cur_level<1||cur_level>AVP_MAX_LEVEL)return;
    need=door_byte_count();
    /* Each level owns exactly the number of door bytes originally saved.
     * If a rebound map has a different layout, writing into the old slot
     * would overwrite the next level's saved doors. Allocate a new span;
     * previously saved spans remain untouched until InitDoors resets them. */
    if(l2l_off[cur_level-1]<0 || l2l_span_length((unsigned)cur_level-1u)!=need){
        if(need>L2L_DOOR_BYTES-(unsigned)l2l_used)return;
        l2l_off[cur_level-1]=(s16)l2l_used;
        l2l_store_length((unsigned)cur_level-1u,need);
        l2l_used=(u16)(l2l_used+need);
    }
    out=l2l_area+(unsigned)l2l_off[cur_level-1];cells=(unsigned)maze_w*(unsigned)maze_h;
    for(i=0;i<cells;++i){u8 *c=maze_data+i*CELL;
        if(fullbits[c[LEFT_WALL]]&AVP_WB_DOOR)out[pos++]=c[LEFT_DOOR];
        if(fullbits[c[TOP_WALL]]&AVP_WB_DOOR)out[pos++]=c[TOP_DOOR];
    }
}

static u8 *cell_at(unsigned x,unsigned y)
{ if(!maze_data||x>=maze_w||y>=maze_h)return NULL;return maze_data+((size_t)y*maze_w+x)*CELL; }
static u8 *door_ptr_for(u8 *cell,unsigned side)
{
    size_t row=(size_t)maze_w*CELL;
    switch(side&3u){
    case 0:return cell+LEFT_DOOR;
    case 1:return cell+TOP_DOOR;
    case 2:return cell+CELL+LEFT_DOOR;
    default:return cell+row+TOP_DOOR;
    }
}
static ptrdiff_t maze_move(unsigned side)
{
    switch(side&3u){case 0:return -(ptrdiff_t)CELL;case 1:return -(ptrdiff_t)((size_t)maze_w*CELL);case 2:return CELL;default:return (ptrdiff_t)((size_t)maze_w*CELL);}
}

void ResetDoors(void)
{
    unsigned i,cells,pos=0;const u8 *in=NULL;
    /* Never apply saved bytes to a map with a different number of doors.
     * Otherwise ResetDoors reads into another level's stored state. */
    if(maze_data&&cur_level>=1&&cur_level<=AVP_MAX_LEVEL&&
       l2l_off[cur_level-1]>=0 && l2l_span_length((unsigned)cur_level-1u)==door_byte_count())
        in=l2l_area+(unsigned)l2l_off[cur_level-1];
    if(in){cells=(unsigned)maze_w*(unsigned)maze_h;for(i=0;i<cells;++i){u8 *c=maze_data+i*CELL;
        if(fullbits[c[LEFT_WALL]]&AVP_WB_DOOR)c[LEFT_DOOR]=in[pos++];
        if(fullbits[c[TOP_WALL]]&AVP_WB_DOOR)c[TOP_DOOR]=in[pos++];
    }}
    memset(&door1,0,sizeof door1);memset(&door2,0,sizeof door2);dbl_door=0;comp_panel=0;lift_key=0;lift_dir=0;lift_ptr=NULL;
    /* DOORS.S force-closes the two authored lift doors on levels 1..5. */
    if(cur_level>=1&&cur_level<=5&&maze_data){u8 *p=cell_at(10u,21u);if(p)p[LEFT_DOOR]=0;p=cell_at(34u,21u);if(p)p[LEFT_DOOR]=0;}
}

static unsigned facing_side(void)
{
    /* neg.l / rol.l #3 / add.w #5 / ror.w #1 / and #3 */
    u32 d=0u-centre_angle;d=(d<<3)|(d>>29);{u16 w=(u16)d;w=(u16)(w+5u);w=(u16)((w>>1)|(w<<15));return w&3u;}
}
/* Host input adapter query only. DOORS.S::DoorKeys is left intact: the
 * original active-low FIRE_B + JOY_UP/JOY_DOWN decision runs unchanged.
 * Resolve precisely the same cell, facing side and panel-file-id as DoorKeys
 * so a PC analog stick is converted to Jaguar lift input only at a real lift. */
int avp_doors_player_facing_lift(void)
{
    u8 *cell;
    u8 panel,clean,file_id;
    unsigned side;
    if(!maze_data)return 0;
    if(new_pos<(u32)((size_t)maze_w*maze_h*CELL))
        cell=maze_data+new_pos;
    else
        cell=cell_at((unsigned)(x_pos>>16),(unsigned)(y_pos>>16));
    if(!cell)return 0;
    side=facing_side();
    panel=cell[side];
    clean=(u8)(panel&0x7fu);
    file_id=(clean>=1u&&clean<=AVP_MAX_PANELS)?panel_list[clean-1u]:0xffu;
    return panel_id_is(file_id,panel_ids.lift_back)||
           panel_id_is(file_id,panel_ids.lift_side);
}

static u8 door_limit(u8 panel)
{
    /* Source doorlims[] contains word offsets into the byte table which follows
     * it.  Reproduce that lookup rather than assigning per-type defaults.  The
     * authored retail panels use only valid subtype indices, but keeping the
     * contiguous byte order also preserves the 68000 behavior at boundaries. */
    static const s8 off[16]={-1,-1,-1,-1,0,1,2,4,5,6,6,8,-1,-1,-1,-1};
    static const u8 limbytes[9]={56,48,52,56,112,60,92,100,48};
    unsigned type=(panel&0x78u)>>3,idx=panel&7u;
    if(off[type]<0)return DOOR_LIM_DEFAULT;
    { unsigned pos=(unsigned)off[type]+idx;
      return pos<sizeof(limbytes)?limbytes[pos]:DOOR_LIM_DEFAULT; }
}

static const AvpDoorAccessEntry *find_access(u16 off)
{
    size_t i;if(!cur_meta)return NULL;for(i=0;i<cur_meta->access_count;++i)if(cur_meta->access[i].door_offset==off)return &cur_meta->access[i];return NULL;
}
static const AvpDoorExitEntry *find_exit_record(u16 off)
{
    size_t i;if(!cur_meta)return NULL;for(i=0;i<cur_meta->exit_count;++i)if(cur_meta->exits[i].panel_offset==off)return &cur_meta->exits[i];return NULL;
}
static const AvpDoorInterlockEntry *find_interlock(u16 off)
{
    size_t i;if(!cur_meta)return NULL;for(i=0;i<cur_meta->interlock_count;++i)if(cur_meta->interlocks[i].door_offset==off)return &cur_meta->interlocks[i];return NULL;
}
static int apply_exit(u16 panel_off)
{
    const AvpDoorExitEntry *e=find_exit_record(panel_off);if(!e)return 0;
    new_x=(s32)(((u32)e->x<<16)|((u32)new_x&0xffffu));
    new_y=(s32)(((u32)e->y<<16)|((u32)new_y&0xffffu));
    new_level=(s16)e->level;return 1;
}
static int airlock_exit(u16 door_off,u16 panel_off)
{
    const AvpDoorInterlockEntry *ie=find_interlock(door_off);size_t i;
    if(!ie){avp_hudmsg_queue(avp_msg_notsecure);return 0;}
    for(i=0;i<ie->required_count;++i){u16 off=ie->required_offsets[i];if(!maze_data||off>=(u16)((size_t)maze_w*maze_h*CELL)||maze_data[off]){avp_hudmsg_queue(avp_msg_notsecure);return 0;}}
    new_x=(s32)(((u32)x_pos&0xffffu)|((u32)new_x&0xffff0000u));
    new_y=(s32)(((u32)y_pos&0xffffu)|((u32)new_y&0xffff0000u));
    /* In the final source find_exit silently leaves the high words unchanged if
     * the authored record is absent; do_airlock still selects msg_pressure and
     * plays the airlock effect once all interlocks are secure. */
    (void)apply_exit(panel_off);
    if(new_level)host_exit_kind=AVP_DOORS_EXIT_AIRLOCK;
    avp_hudmsg_queue(avp_msg_pressure);levfx_ID=DoEffect(DSFX_AIRLOCK);return 1;
}
static int access_ok(u8 *door,u16 panel_off)
{
    const AvpDoorAccessEntry *ae;s16 code;
    if(*door)return 1; /* source always permits operating an already-open door */
    ae=find_access((u16)(door-maze_data));if(!ae)return 1;code=ae->code;
    if(code==AVP_ACC_UNUSED){avp_hudmsg_queue(avp_msg_airlocked);return 0;}
    if(code==AVP_ACC_JAMMED){avp_hudmsg_queue(avp_msg_jammed);return 0;}
    if(code==AVP_ACC_EXIT){
        if(mp_dm_no_travel){avp_hudmsg_queue(avp_msg_airlocked);return 0;}
        (void)airlock_exit((u16)(door-maze_data),panel_off);return 0;
    }
    if(code==AVP_ACC_ESCAPE){if((u16)(y_pos>>16)==50u)avp_hudmsg_queue(avp_msg_escape);return 1;}
    if(code>0&&(u16)code>acs_level){avp_hudmsg_set_access_code((unsigned)code);avp_hudmsg_queue(avp_msg_access_denied);play(DSFX_ACCD);return 0;}
    if(player_type==PT_HUMAN)play(DSFX_ACCG);
    return 1;
}

static void init_dcoll(DoorState *d,unsigned side,int dx,int dy)
{
    static const s16 std_xy[4][2]={{0,0},{0,0},{1,0},{0,1}};
    static const s16 grid_other[4]={-1,-64,-1,-64};
    s16 x=(s16)(x_pos>>16),y=(s16)(y_pos>>16);unsigned idx;
    x=(s16)(x+dx+std_xy[side&3u][0]);y=(s16)(y+dy+std_xy[side&3u][1]);
    /* A signed 16-bit room coordinate scaled by 65536 always fits s32.
     * Unlike a signed left shift, multiplication also defines negative
     * door-edge positions while preserving the original 16.16 word. */
    d->xpos=(s32)x*65536;d->ypos=(s32)y*65536;d->index=(u16)(((side&1u)!=0u)?4u:0u);
    idx=(unsigned)((u16)y*64u+(u16)x);d->grid1=(u16)idx;d->grid2=(u16)(idx+grid_other[side&3u]);
}
static void install_door(DoorState *d,u8 *ptr,u8 panel,unsigned side,int dx,int dy)
{
    d->ptr=ptr;d->dir=(s8)(*ptr?-DOOR_STEP:DOOR_STEP);d->lim=door_limit(panel);init_dcoll(d,side,dx,dy);
}

static int amp_grid_hit(u16 grid,unsigned axis,int second)
{
    u16 n;if(grid>=AVP_AMP_GRID_W*AVP_AMP_GRID_H)return 0;n=collmap[grid];if(n==0||n>AVP_NUM_AMPS)return 0;
    {
        const AvpAmp *a=&amp_data[n-1u];
        unsigned x=(u32)a->xpos>>16,y=(u32)a->ypos>>16;
        u16 frac;
        /* Stale grid entries must not bounce a closing door. This follows
         * the same live-owner checks already used by projectile hits. */
        if((!a->mode&&!a->host_static) || x>=AVP_AMP_GRID_W ||
           y>=AVP_AMP_GRID_H || y*AVP_AMP_GRID_W+x!=grid)return 0;
        frac=(u16)((axis==0u?a->xpos:a->ypos)&0xffffu);
        return second?(frac>=(u16)(0u-DC_SIZE)):(frac<=DC_SIZE);
    }
}
/* 68000 SUB/NEG are modulo-2^32.  Interpret the wrapped sign bit
 * without a C signed subtraction/negation overflow at INT32_MIN. */
static int door_collides(const DoorState *d)
{
    u8 threshold;if(!d||!d->ptr)return 0;threshold=(u8)(d->lim-DC_MIN);if(*d->ptr>=threshold)return 0;
    if(d->index==0u){
        if((u16)(y_pos>>16)==(u16)(d->ypos>>16)){u32 delta=x_pos-(u32)d->xpos;if(delta&0x80000000u)delta=0u-delta;if(delta<=DC_SIZE)return 1;}
        if(amp_grid_hit(d->grid1,0,0)||amp_grid_hit(d->grid2,0,1))return 1;
    }else{
        if((u16)(x_pos>>16)==(u16)(d->xpos>>16)){u32 delta=y_pos-(u32)d->ypos;if(delta&0x80000000u)delta=0u-delta;if(delta<=DC_SIZE)return 1;}
        if(amp_grid_hit(d->grid1,1,0)||amp_grid_hit(d->grid2,1,1))return 1;
    }
    return 0;
}
static void ping_door(DoorState *d)
{
    if(!d)return;
    d->dir=(s8)-d->dir;
    if(dbl_door){DoorState *other=(d==&door1?&door2:&door1);other->dir=(s8)-other->dir;play(DSFX_DDOORS);}else play(DSFX_RISE2);
}

static void lift_exit(void)
{
    lift_ptr=NULL;new_level=(s16)(cur_level+lift_dir);new_x=(s32)x_pos;new_y=(s32)y_pos;levfx_ID=DoEffect(DSFX_EL10X);host_exit_kind=AVP_DOORS_EXIT_LIFT;
}
static void check_lift(void)
{
    if(!lift_ptr)return;
    if((u16)(x_pos>>16)!=lift_x||(u16)(y_pos>>16)!=lift_y){lift_ptr=NULL;return;}
    if(*lift_ptr==0){lift_exit();return;}
    if(door1.ptr==lift_ptr){if(door1.dir<0)return;lift_ptr=NULL;return;}
    if(door2.ptr==lift_ptr){if(door2.dir<0)return;lift_ptr=NULL;return;}
    lift_ptr=NULL;
}

static int start_normal_or_double(u8 *cell,u8 *panelp,u8 panel,unsigned side,int from_lift)
{
    u8 clean=(u8)(panel&0x7fu),flags=fullbits[clean];u8 *door=door_ptr_for(cell,side);u16 panel_off=(u16)(panelp-maze_data);
    if(!(flags&AVP_WB_DOOR))return 0;
    if((flags&D2_SPECIAL)==D2_SPECIAL){
        static const s16 dd[4][2]={{0,-1},{1,0},{0,1},{-1,0}};ptrdiff_t step;int dx,dy;
        if(door1.ptr==door||door2.ptr==door){door1.dir=(s8)-door1.dir;door2.dir=(s8)-door2.dir;play(DSFX_DDOORS);return 1;}
        if(door1.ptr||door2.ptr)return 0;
        if(!access_ok(door,panel_off))return 0;
        /* Assembly rotates the side once before indexing maze_moves; reflect reverses it. */
        step=maze_move((side+1u)&3u);if(panel&0x80u)step=-step;
        dx=dd[side][0];dy=dd[side][1];if(panel&0x80u){dx=-dx;dy=-dy;}
        {
            u8 *other=door+step;
            s8 dir=(s8)(*other?-DOOR_STEP:DOOR_STEP);
            install_door(&door1,door,clean,side,0,0);
            install_door(&door2,other,clean,side,dx,dy);
            /* DOORS.S tests the second-half byte after computing both pointers
             * and writes that single direction to both active blocks. */
            door1.dir=door2.dir=dir;
        }
        dbl_door=0xffu;play(DSFX_DDOORS);return 1;
    }
    if(door2.ptr==door){door2.dir=(s8)-door2.dir;play(DSFX_RISE2);return 1;}
    if(door1.ptr==door){door1.dir=(s8)-door1.dir;play(DSFX_RISE2);return 1;}
    {DoorState *slot=!door1.ptr?&door1:(!door2.ptr?&door2:NULL);if(!slot)return 0;if(!access_ok(door,panel_off))return 0;install_door(slot,door,clean,side,0,0);play(DSFX_RISE2);}
    (void)from_lift;return 1;
}

static int do_lifts(u8 *cell,unsigned side)
{
    (void)side;
    int dir=0,target;unsigned i,door_side=0;u8 *door=NULL;
    if(pressed(joy_cur,JOY_UP))--dir;
    if(pressed(joy_cur,JOY_DOWN))++dir;
    if(!dir){lift_key=0xffu;return 0;}
    if(player_type==PT_ALIEN){avp_hudmsg_queue(avp_msg_jlift);play(DSFX_FLUP1);lift_key=0xffu;return 0;}
    if((u16)(x_pos>>16)==9u&&(u16)(y_pos>>16)==21u&&((cur_level==3&&dir>0)||(cur_level==4&&dir<0))){avp_hudmsg_queue(avp_msg_jlift);play(DSFX_FLUP1);lift_key=0xffu;return 0;}
    target=cur_level+dir;if(target<=0||target>=6){play(DSFX_FLUP1);lift_key=0xffu;return 0;}lift_dir=(s8)dir;
    for(i=0;i<4u;++i)if(cell[i]==LIFTDOR){door_side=i;door=door_ptr_for(cell,i);break;}
    if(!door){play(DSFX_FLUP1);lift_key=0xffu;return 0;}
    /* `beq lift_exit` branches out of do_lifts and returns directly to DoorKeys;
     * it does not execute .exit and therefore does not set lift_key=-1. */
    if(*door==0){lift_exit();return 0;}
    lift_ptr=door;lift_x=(u16)(x_pos>>16);lift_y=(u16)(y_pos>>16);
    if(door1.ptr==door&&door1.dir<0){lift_key=0xffu;return 0;}
    if(door2.ptr==door&&door2.dir<0){lift_key=0xffu;return 0;}
    /* The source BSRs back into lift_door.  Its local `move_doors` label is
     * only the DoorKeys exit (clear lift_key + RTS), NOT MoveDoors::.  Door
     * bytes advance once later in PLAYER.S::UpdatePlayer. */
    (void)start_normal_or_double(cell,cell+door_side,cell[door_side],door_side,1);
    lift_key=0xffu;return 1;
}

static int do_duct(unsigned side,u16 panel_off)
{
    static const u16 lx[4]={0x10000u-DUCT_EDGE,0x8000u,DUCT_EDGE,0x8000u};
    static const u16 ly[4]={0x8000u,0x10000u-DUCT_EDGE,0x8000u,DUCT_EDGE};
    static const u32 ang[4]={0x80000000u,0x40000000u,0u,0xc0000000u};
    new_x=(s32)(((u32)new_x&0xffff0000u)|lx[side&3u]);new_y=(s32)(((u32)new_y&0xffff0000u)|ly[side&3u]);centre_angle=ang[side&3u];return apply_exit(panel_off);
}

static int pc_alien_use_active,pc_alien_use_down,pc_alien_use_previous;
void avp_doors_pc_alien_use(int active,int down)
{
    if(!active){pc_alien_use_active=pc_alien_use_down=pc_alien_use_previous=0;return;}
    if(!pc_alien_use_active)pc_alien_use_previous=0;
    pc_alien_use_active=1;pc_alien_use_down=down?1:0;
}
void DoorKeys(void)
{
    u32 key=lift_key?joy_cur:joy_edge;u8 *cell,*panelp;unsigned side;u8 panel,clean,file_id;
    int do_use;
    if(pc_alien_use_active){
        do_use=pc_alien_use_down&&(lift_key||!pc_alien_use_previous);
        pc_alien_use_previous=pc_alien_use_down;
    }else do_use=pressed(key,FIRE_B);
    if(!maze_data)return;
    /* In Jaguar mode keep retail B. In Alien PC-action mode, host Use is
       independent of B so biting cannot operate a door or terminal. */
    if(!do_use){if(lift_key)lift_key=0;return;}
    if(new_pos<(u32)((size_t)maze_w*maze_h*CELL))cell=maze_data+new_pos;else cell=cell_at((unsigned)(x_pos>>16),(unsigned)(y_pos>>16));
    if(!cell)return;
    side=facing_side();panelp=cell+side;panel=*panelp;
    if(!panel){if(player_type!=PT_ALIEN)play(DSFX_FLUP1);lift_key=0;return;}
    clean=(u8)(panel&0x7fu);file_id=(clean>=1u&&clean<=AVP_MAX_PANELS)?panel_list[clean-1u]:0xffu;
    if(panel_id_is(file_id,panel_ids.lift_back)||panel_id_is(file_id,panel_ids.lift_side)){
        if(mp_dm_no_travel){avp_hudmsg_queue(avp_msg_jlift);lift_key=0;return;}
        (void)do_lifts(cell,side);return;
    }
    if(lift_key){if(player_type!=PT_ALIEN)play(DSFX_FLUP1);lift_key=0;return;}
    if(player_type!=PT_PREDATOR&&(panel_id_is(file_id,panel_ids.wall2)||panel_id_is(file_id,panel_ids.airduct1))){
        if(!mp_dm_no_travel)(void)do_duct(side,(u16)(panelp-maze_data));
        lift_key=0;return;
    }
    if(player_type==PT_HUMAN&&(panel_id_is(file_id,panel_ids.computer1)||panel_id_is(file_id,panel_ids.computer2)||panel_id_is(file_id,panel_ids.medical_computer))){
        if(mp_dm_no_terminal){lift_key=0;return;}
        comp_panel=file_id;comp_offset=(u16)(panelp-maze_data);lift_key=0;return;
    }
    if(!start_normal_or_double(cell,panelp,panel,side,0)&&player_type!=PT_ALIEN)play(DSFX_FLUP1);
    lift_key=0;
}

void MoveDoors(void)
{
    if(door1.ptr&&door1.dir<0&&door_collides(&door1))ping_door(&door1);
    if(door2.ptr&&door2.dir<0&&door_collides(&door2))ping_door(&door2);
    if(door1.ptr){u8 *p=door1.ptr;u8 v=(u8)(*p+(u8)door1.dir);if(v==0||v==door1.lim)door1.ptr=NULL;*p=v;}
    if(door2.ptr){u8 *p=door2.ptr;u8 v=(u8)(*p+(u8)door2.dir);if(v==0||v==door2.lim){door2.ptr=NULL;dbl_door=0;}*p=v;}
    check_lift();
}

/* Optional PC-only state bridge: all implementation stays in new/.
 * No effect on the original Jaguar/single-player build when compiled out. */
#ifdef AVP_LOCAL_MULTIPLAYER_DOOR_BRIDGE
#include "../../new/multiplayer/local_mp_doors_native.inc"
#endif
