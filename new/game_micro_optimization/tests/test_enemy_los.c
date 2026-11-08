/* RE31 R10: original (Collision.S) LOS result versus optional AI shortcut.
 * Link with original collision.c; no Jaguar game code is rewritten.
 */
#include "collision.h"
#include "doors.h"
#include "game_micro_optimization.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
u8 fullbits[256];
int __real_LineOfSight(const AvpXY *from,const AvpXY *to);
static uint32_t rng_state=0x5d6c32a1u;
static uint32_t next_u32(void){uint32_t x=rng_state;x^=x<<13;x^=x>>17;x^=x<<5;return rng_state=x;}
static int match(const AvpXY *from,const AvpXY *to,unsigned *count)
{
    int a,b;
    avp_game_micro_opt_set(0);
    a=LineOfSight(from,to);
    avp_game_micro_opt_set(1);
    b=LineOfSight(from,to);
    if(a!=b){fprintf(stderr,"LOS mismatch from %08x,%08x to %08x,%08x original=%d opt=%d\n",(unsigned)from->x,(unsigned)from->y,(unsigned)to->x,(unsigned)to->y,a,b);return 0;}
    ++*count;
    return 1;
}
int main(void)
{
    static u8 maze[16*16*8];
    unsigned comparisons=0;
    static const uint16_t edge_fracs[]={0,1,2,0x007f,0x7fff,0x8000,0xff00,0xfffd,0xfffe,0xffff};
    memset(fullbits,0,sizeof(fullbits));
    fullbits[1]=AVP_WB_SOLID;
    fullbits[2]=AVP_WB_DOOR;
    fullbits[3]=AVP_WB_LEFTONLY;
    fullbits[4]=AVP_WB_2SIDES;
    for(unsigned i=0;i<sizeof(maze)/8;i++) {
        for(unsigned j=0;j<4;j++) maze[i*8+j]=(u8)((next_u32()%5u));
        maze[i*8+6]=(u8)next_u32();
        maze[i*8+7]=(u8)next_u32();
    }
    avp_collision_bind_maze(maze,16,16);
    /* Test every boundary combination for a center cell. */
    for(unsigned a=0;a<sizeof(edge_fracs)/sizeof(edge_fracs[0]);a++)
    for(unsigned b=0;b<sizeof(edge_fracs)/sizeof(edge_fracs[0]);b++)
    for(unsigned c=0;c<sizeof(edge_fracs)/sizeof(edge_fracs[0]);c++)
    for(unsigned d=0;d<sizeof(edge_fracs)/sizeof(edge_fracs[0]);d++){
        AvpXY from={(s32)((7u<<16)|edge_fracs[a]),(s32)((7u<<16)|edge_fracs[b])};
        AvpXY to={(s32)((7u<<16)|edge_fracs[c]),(s32)((7u<<16)|edge_fracs[d])};
        if(!match(&from,&to,&comparisons))return 1;
    }
    /* Verify the entire fast-path envelope, including its inclusive corners,
     * against randomly varied authored wall and door bytes. */
    for (unsigned i=0;i<500000;i++) {
        const u32 x=1u+(next_u32()%14u),y=1u+(next_u32()%14u);
        AvpXY from={ (s32)((x<<16)|(0x6000u+(next_u32()%0x4000u))),
                     (s32)((y<<16)|(0x6000u+(next_u32()%0x4000u))) };
        AvpXY to={ (s32)((x<<16)|(0x6000u+(next_u32()%0x4000u))),
                   (s32)((y<<16)|(0x6000u+(next_u32()%0x4000u))) };
        if(!match(&from,&to,&comparisons))return 1;
    }
    /* Diverse wall states/cells, same-cell subpositions, adjacent cells, and
     * longer LOS calls. Verify distinct and outside-map paths fall back. */
    for(unsigned i=0;i<1100000;i++){
        AvpXY from,to;
        uint32_t x=next_u32()%16u,y=next_u32()%16u;
        from.x=(s32)((x<<16)|(next_u32()&0xffffu));
        from.y=(s32)((y<<16)|(next_u32()&0xffffu));
        if(i%17u==0){to.x=(s32)((next_u32()%18u<<16)|(next_u32()&0xffffu));to.y=(s32)((next_u32()%18u<<16)|(next_u32()&0xffffu));}
        else if(i%17u==1){to.x=(s32)(((x+1u)%16u<<16)|(next_u32()&0xffffu));to.y=(s32)((y<<16)|(next_u32()&0xffffu));}
        else {to.x=(s32)((x<<16)|(next_u32()&0xffffu));to.y=(s32)((y<<16)|(next_u32()&0xffffu));}
        if(!match(&from,&to,&comparisons))return 1;
    }
    avp_collision_bind_maze(NULL,0,0);
    {
        AvpXY p={(s32)0x00008000,(s32)0x00008000};
        if(!match(&p,&p,&comparisons))return 1;
    }
    printf("Enemy AI LineOfSight original parity: PASS (%u checks)\n",comparisons);
    return 0;
}
