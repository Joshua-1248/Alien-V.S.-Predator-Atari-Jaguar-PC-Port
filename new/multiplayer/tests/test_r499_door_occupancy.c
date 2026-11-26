/* Uses the production host presence filter and actual native door engine.
 * Synthetic cells; native slot setup is deliberate so axes, double doors,
 * AMP occupancy and pending lifts are reproducible without a ROM. */
#include "../local_mp_walktest.c"
#include <assert.h>
/* Link-only stubs for the original runtime outside the door engine. */
s16 cur_level,new_level,player_type;
s32 new_x,new_y;
u32 x_pos,y_pos,centre_angle,new_pos,levfx_ID;
u8 panel_list[AVP_MAX_PANELS];
volatile u32 joy_cur,joy_edge;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
AvpAmp *amp_data;
static unsigned sounds,effects;
static void sound(void *u,unsigned id){(void)u;(void)id;sounds++;}
const AvpRuntimeOps *avp_runtime_ops(void){static const AvpRuntimeOps ops={.play_sfx=sound};return &ops;}
u32 DoEffect(unsigned id){(void)id;effects++;return 1u;}
#include "../../../src/game/doors.c"
static u8 cells[64*64*8];
static AvpAmp amp[AVP_NUM_AMPS];
static void setup(void){
    memset(cells,0,sizeof cells);memset(collmap,0,sizeof collmap);memset(amp,0,sizeof amp);amp_data=amp;
    cur_level=5;new_level=0;new_x=new_y=0;sounds=effects=0;
    InitDoors();avp_doors_bind_maze(cells,64,64);
    x_pos=0x228000;y_pos=0x228000;
}
static void slot(DoorState *d,unsigned off,unsigned axis,unsigned cx,unsigned cy){
    d->ptr=cells+off;*d->ptr=36;d->dir=-4;d->lim=48;
    d->index=axis?4:0;d->xpos=(s32)(cx<<16);d->ypos=(s32)(cy<<16);
    d->grid1=(u16)(cy*64+cx);d->grid2=(u16)(d->grid1-(axis?64:1));
}
static void check_pose(u32 x,u32 y){assert(x_pos==x && y_pos==y);}
int main(void){
    unsigned cases=0;
    for(unsigned axis=0;axis<2;axis++)for(unsigned second=0;second<2;second++)for(unsigned side=0;side<2;side++){
        setup();DoorState *d=second?&door2:&door1;slot(d,6,axis,10,12);
        const u32 hx=x_pos,hy=y_pos;
        AvpMpDoorOccupant p[20];for(unsigned i=0;i<20;i++)p[i]=(AvpMpDoorOccupant){hx,hy};
        p[19]=(AvpMpDoorOccupant){0xa0000u+(axis?0x8000u:(side?0x4800u:0u-0x4800u)),0xc0000u+(axis?(side?0x4800u:0u-0x4800u):0x8000u)};
        assert(avp_mp_doors_native_step_occupied(p,20,0)==(second?2u:1u));
        assert(cells[6]==40 && d->dir==4 && sounds==1);check_pose(hx,hy);cases++;
        /* Native threshold: at opening byte 40 there is no reversal yet. */
        setup();d=second?&door2:&door1;slot(d,6,axis,10,12);cells[6]=40;
        assert(!avp_mp_doors_native_step_occupied(p,20,0));assert(cells[6]==36);
        assert(avp_mp_doors_native_step_occupied(p,20,0)==(second?2u:1u));assert(cells[6]==40);cases++;
    }
    /* Separate actors can obstruct both unrelated closing doors at once. */
    setup();slot(&door1,6,0,10,12);slot(&door2,7,1,20,22);
    AvpMpDoorOccupant both[]={{0xa2000,0xc8000},{0x148000,0x162000}};
    assert(avp_mp_doors_native_step_occupied(both,2,0)==3);assert(cells[6]==40 && cells[7]==40 && sounds==2);cases++;
    /* Two leaves reverse together once, even if both are occupied. */
    setup();slot(&door1,6,0,10,12);slot(&door2,7,0,10,13);dbl_door=0xff;
    AvpMpDoorOccupant pair[]={{0xa2000,0xc8000},{0xa2000,0xd8000}};
    assert(avp_mp_doors_native_step_occupied(pair,2,0)==1);assert(cells[6]==40 && cells[7]==40 && sounds==1);cases++;
    setup();slot(&door1,6,0,10,12);slot(&door2,7,0,10,13);dbl_door=0xff;
    assert(avp_mp_doors_native_step_occupied(pair+1,1,0)==2);assert(cells[6]==40 && cells[7]==40 && sounds==1);cases++;
    /* Opening doors remain opening; absent or just-outside actors don't block. */
    setup();slot(&door1,6,0,10,12);door1.dir=4;
    assert(!avp_mp_doors_native_step_occupied(pair,2,0));assert(cells[6]==40 && sounds==0);cases++;
    setup();slot(&door1,6,0,10,12);x_pos=pair[0].x;y_pos=pair[0].y;
    AvpMpDoorOccupant outside={0xa4801,0xc8000};
    assert(!avp_mp_doors_native_step_occupied(&outside,1,0));assert(cells[6]==32 && sounds==0);check_pose(pair[0].x,pair[0].y);cases++;
    /* Native AMP grid remains effective even without a living player. */
    setup();slot(&door1,6,0,10,12);collmap[door1.grid1]=1;amp[0].xpos=0xa2000;amp[0].ypos=0xc8000;amp[0].host_static=1;
    assert(avp_mp_doors_native_step_occupied(NULL,0,0)==1);assert(cells[6]==40 && sounds==1);cases++;
    /* Pending lift must retain real passenger XY and fire exactly once. */
    setup();slot(&door1,6,0,10,12);cells[6]=4;lift_ptr=door1.ptr;lift_x=10;lift_y=12;lift_dir=-1;
    x_pos=0xa8000;y_pos=0xc8000;AvpMpDoorOccupant passenger={x_pos,y_pos};
    assert(!avp_mp_doors_native_step_occupied(&passenger,1,1));assert(cells[6]==0 && new_level==4 && effects==1 && (u32)new_x==x_pos && (u32)new_y==y_pos);cases++;
    setup();slot(&door1,6,0,10,12);cells[6]=4;lift_ptr=door1.ptr;lift_x=10;lift_y=12;lift_dir=-1;
    x_pos=0xa8000;y_pos=0xc8000;
    assert(!avp_mp_doors_native_step_occupied(NULL,0,0));assert(cells[6]==0 && new_level==0 && !lift_ptr && effects==0);cases++;
    /* Production host list: 1 human + 6 bots on two levels; dead and
     * fading actors and missing world membership are not physical. */
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpActorNativeState *a=calloc(7,sizeof *a);assert(t&&a);
    avp_local_mp_defaults(&t->session.setup);t->actors=7;t->players=1;t->session.running=1;
    t->session.setup.players=1;t->session.setup.bot_count=6;t->session.setup.campaign=0;
    t->session.worlds[0]=(AvpMpActiveWorld){0,5,0x5f,t,0};
    t->session.worlds[1]=(AvpMpActiveWorld){0,4,0x20,a,0};
    for(unsigned p=0;p<7;p++){t->session.player[p]=a+p;t->session.setup.level[p]=p==5?4:5;a[p].player.player_energy=100;a[p].player.x_pos=0x10000+p;a[p].player.y_pos=0x20000;}
    a[1].player.player_energy=0;t->player_life[2].phase=AVP_MP_LIFE_FADE_OUT;t->vent_fade[3].phase=AVP_MP_LIFE_FADE_IN;
    t->session.worlds[0].member_mask&=~AVP_MP_ACTOR_BIT(4);
    AvpMpDoorOccupant present[AVP_LOCAL_MP_MAX_ACTORS];
    for(unsigned toggle=0;toggle<2;toggle++){
        t->session.setup.player_collisions=toggle;
        assert(walk_door_occupants(t,5,present)==2);assert(present[0].x==0x10000 && present[1].x==0x10006);
        assert(walk_door_occupants(t,4,present)==1 && present[0].x==0x10005);cases++;
    }
    free(a);free(t);printf("PASS %u native door occupancy/animation/lift/presence cases\n",cases);return 0;
}
