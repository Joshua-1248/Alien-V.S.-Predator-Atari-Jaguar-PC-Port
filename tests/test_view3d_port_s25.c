/* RE24 Session 2: ROM-free tests for the native C 3D-view port (view3d.c).
 *
 * The authoritative 1:1 check needs the user's cartridge and lives in
 * tools/jagref/view3d_port_parity.sh.  This test needs no cartridge:
 *  1. the port's GPU divider equals the TRM JRISC model's divider bit for
 *     bit (quotient and remainder, both modes, edge cases);
 *  2. a fully synthetic scene (invented maze, textures, tables and objects,
 *     no ROM data) renders through all three stages with the TRM Blitter
 *     without a fault, every wall type is reached, objects are sorted and
 *     drawn, every column writes its stage-2 record, and the result is
 *     deterministic. */
#include "jaguar/avp_gpu/view3d.h"
#include "jaguar/blitter_trm.h"
#include "jaguar/jrisc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL(...) do{fprintf(stderr,"test_view3d_port_s25: " __VA_ARGS__);fputc('\n',stderr);exit(1);}while(0)

typedef struct Sys { uint8_t ram[0x200000]; uint8_t cart[0x20000]; JagBlitter blit; unsigned blits; } Sys;
static Sys sys;
static uint8_t rd8(Sys *s,uint32_t a)
{
    a&=0xFFFFFFu;
    if(a<0x200000u)return s->ram[a];
    if(a>=0x800000u&&a<0x820000u)return s->cart[a-0x800000u];
    return 0;
}
static void wr8(Sys *s,uint32_t a,uint8_t v){a&=0xFFFFFFu;if(a<0x200000u)s->ram[a]=v;}
static uint8_t b_rd8(void *c,uint32_t a){return rd8((Sys*)c,a);}
static void b_wr8(void *c,uint32_t a,uint8_t v){wr8((Sys*)c,a,v);}
static void on_blit(void *c,const JagBlitter *b){(void)b;((Sys*)c)->blits++;}
static uint32_t bus_rd(void *c,uint32_t a,int w)
{
    Sys *s=c;uint32_t v=0;
    if(a>=0xF02200u&&a<0xF022A0u){jag_blitter_read(&s->blit,a-0xF02200u,&v);return v;}
    for(int i=0;i<w;i++)v=(v<<8)|rd8(s,a+(uint32_t)i);
    return v;
}
static void bus_wr(void *c,uint32_t a,uint32_t v,int w)
{
    Sys *s=c;
    if(a>=0xF02200u&&a<0xF022A0u){jag_blitter_write(&s->blit,a-0xF02200u,v);return;}
    for(int i=w-1;i>=0;i--){wr8(s,a+(uint32_t)i,(uint8_t)v);v>>=8;}
}
static void w32(uint32_t a,uint32_t v){bus_wr(&sys,a,v,4);}
static void w16(uint32_t a,uint32_t v){bus_wr(&sys,a,v,2);}
static void c16(uint32_t off,uint32_t v){sys.cart[off]=(uint8_t)(v>>8);sys.cart[off+1]=(uint8_t)v;}
static uint64_t fnv(const uint8_t *p,size_t n){uint64_t h=0xcbf29ce484222325ull;for(size_t i=0;i<n;i++){h^=p[i];h*=0x100000001b3ull;}return h;}

static uint64_t lcg=12345;
static uint32_t rnd(void){lcg=lcg*6364136223846793005ull+1442695040888963407ull;return (uint32_t)(lcg>>33);}

/* Synthetic scene: invented values only. */
#define MAZE   0x04FC00u
#define W 24u
#define H 24u
static void build_scene(AvpView3DState *st)
{
    memset(&sys,0,sizeof sys);
    { JagBlitBus bb={&sys,b_rd8,b_wr8}; jag_blitter_init(&sys.blit,bb); }
    sys.blit.on_blit=on_blit;sys.blit.on_blit_ctx=&sys;
    /* "cartridge" tables: column angle offsets, cos correction, steps */
    for(unsigned i=0;i<=512;i++){c16(0x15604u+2u*i,i*24u); /* stays inside the cos table (index 512-(a>>5)) */c16(0x15000u+2u*i,0x4000u+i*8u);}
    for(unsigned i=0;i<256;i++){c16(0x15A06u+2u*i,0x8000u-i*64u);c16(0x15402u+2u*i,0x1000u+i*32u);}
    /* GPU start-up registers (secondary bank) */
    memset(st,0,sizeof *st);
    st->alt[3]=0xA0u;st->alt[4]=W;st->alt[5]=H;
    st->alt[10]=0xF02238u;st->alt[11]=0xF0223Cu;st->alt[12]=0xF02114u;st->alt[13]=0x7F80u;
    st->alt[14]=0xF02200u;st->alt[15]=0xF02224u;st->alt[16]=0x72u;st->alt[17]=0x400000u;st->alt[18]=MAZE;
    st->r[17]=0xF0342Eu;
    /* parameter block */
    w32(0x7F9Cu,0x03989Cu);w32(0x7FA0u,0x03D0DCu);w32(0x7FA4u,0x02EFA8u);
    w32(0x7FA8u,0xFFFFA000u);w32(0x7FACu,0x00005000u);
    w32(0x7FB0u,(12u<<16)|0x8000u);w32(0x7FB4u,(12u<<16)|0x4000u);w32(0x7FB8u,0x1234ABCDu);w32(0x7FBCu,0x0B9600u);
    /* texture and table variables */
    w32(AVP_VIEW3D_VAR_FLOOR_TEX,0x105000u);w32(AVP_VIEW3D_VAR_WALL_TEX,0x0DD000u);
    { static const uint32_t tex[]={AVP_VIEW3D_VAR_SHAPED_TEX,AVP_VIEW3D_VAR_DOOR4_TEX,AVP_VIEW3D_VAR_SLIDE5_TEX,
        AVP_VIEW3D_VAR_SLIDE6_TEX,AVP_VIEW3D_VAR_RISE7_TEX,AVP_VIEW3D_VAR_SPLIT8_TEX,AVP_VIEW3D_VAR_DOOR9_TEX,AVP_VIEW3D_VAR_DOOR11_TEX};
      static const uint32_t ht[]={AVP_VIEW3D_VAR_SHAPED_HT,AVP_VIEW3D_VAR_DOOR4_HT,AVP_VIEW3D_VAR_SLIDE6_HT,AVP_VIEW3D_VAR_DOOR9_HT,AVP_VIEW3D_VAR_DOOR11_HT};
      for(unsigned i=0;i<8;i++)w32(tex[i],0x12D000u+0x10000u*i);
      for(unsigned i=0;i<5;i++)w32(ht[i],0x02F5BEu+0x100u*i); }
    for(uint32_t a=0x02F5BEu;a<0x02F5BEu+0x900u;a+=2){sys.ram[a]=(uint8_t)(0xE0u+rnd()%16u);sys.ram[a+1]=(uint8_t)(0x10u+rnd()%16u);}
    /* invented 16-bit textures */
    for(uint32_t a=0x0DD000u;a<0x1C0000u;a++)sys.ram[a]=(uint8_t)rnd();
    /* maze: border of plain walls, interior with every wall type */
    for(uint32_t y=0;y<H;y++)for(uint32_t x=0;x<W;x++){
        uint32_t c=MAZE+(y*W+x)*8u;
        sys.ram[c+4]=(uint8_t)(1u+rnd()%2u);sys.ram[c+5]=(uint8_t)(1u+rnd()%2u);
        sys.ram[c+6]=(uint8_t)rnd();sys.ram[c+7]=(uint8_t)rnd();
        if(x==0||y==0||x==W-1u||y==H-1u){for(int f=0;f<4;f++)sys.ram[c+(uint32_t)f]=(uint8_t)(1u+rnd()%3u);}
        else if((x*7u+y*3u)%5u==0u&&!(x==12u&&y==12u)){
            uint8_t t=(uint8_t)((rnd()%16u)<<3|(1u+rnd()%3u));
            for(int f=0;f<4;f++)if(rnd()%2u)sys.ram[c+(uint32_t)f]=t;
        }
    }
    /* objects: image set table -> sets -> frames (signed extents, columns) */
    w32(0x02EFA8u,0x02F000u);                 /* class 0 -> set table */
    w32(0x02F000u,0x02F100u);                 /* animseq 1 -> image set */
    w16(0x02F100u,2u);w32(0x02F104u,0x40u);w32(0x02F108u,0x40u);   /* 2 frames, both at +$40 */
    { uint32_t fr=0x02F140u; sys.ram[fr]=0xF0u;sys.ram[fr+1]=0x10u;
      for(unsigned i=0;i<32;i++){w16(fr+2u+2u*i,0x80u+0x20u*i);
          uint32_t col=0x02F100u+0x80u+0x20u*i;sys.ram[col]=0xE8u;sys.ram[col+1]=0x18u;
          for(unsigned k=2;k<0x20u;k++)sys.ram[col+k]=(uint8_t)rnd();} }
    w32(AVP_VIEW3D_VAR_SPRITE_PARM,0x02F1C8u);w16(0x02F1C8u,0x80u);w16(0x02F1CAu,0u);
    for(unsigned o=0;o<300u;o++){
        uint32_t r=0x03989Cu+o*48u;
        if(o>=24u)continue;
        w32(r,((10u+o%5u)<<16)|(rnd()&0xFFFFu));w32(r+4u,((9u+o/5u)<<16)|(rnd()&0xFFFFu));
        w32(r+8u,1u);w16(r+12u,0u);w16(r+14u,1u);w16(r+16u,(uint16_t)(o%4u));w16(r+18u,(uint16_t)rnd());
        w16(r+42u,0u);w16(r+44u,(uint16_t)(rnd()%32u));
    }
}

static uint64_t render(AvpView3DState *st)
{
    AvpView3DBus bus={&sys,bus_rd,bus_wr};
    sys.blits=0;
    avp_view3d_stage0(st,&bus);
    avp_view3d_stage1(st,&bus);
    avp_view3d_stage2(st,&bus);
    return fnv(sys.ram,sizeof sys.ram);
}

int main(void)
{
    /* 1. divider */
    static const uint32_t edge[]={0,1,2,3,0x7FFFFFFFu,0x80000000u,0xFFFFFFFFu,0x10000u,0xFFFFu,0x40000000u,0xE3u,0x140u};
    for(int mode=0;mode<2;mode++){
        for(unsigned i=0;i<sizeof edge/sizeof *edge;i++)for(unsigned j=0;j<sizeof edge/sizeof *edge;j++){
            uint32_t ra=0,rb=0,qa=avp_view3d_div(edge[i],edge[j],mode,&ra),qb=jrisc_div_quotient(edge[i],edge[j],mode,&rb);
            if(qa!=qb||ra!=rb)FAIL("div edge %08X/%08X mode %d",edge[i],edge[j],mode);
        }
        for(unsigned i=0;i<200000u;i++){
            uint32_t n=rnd()^(rnd()<<17),d=(i&3u)?(rnd()>>(rnd()%31u)):(rnd()^(rnd()<<15));
            uint32_t ra=0,rb=0,qa=avp_view3d_div(n,d,mode,&ra),qb=jrisc_div_quotient(n,d,mode,&rb);
            if(qa!=qb||ra!=rb)FAIL("div %08X/%08X mode %d",n,d,mode);
        }
    }
    /* 2. synthetic scene, several angles */
    { unsigned typesum[16]={0},objects=0,columns=0;
      for(unsigned v=0;v<24u;v++){
        AvpView3DState st,st2;uint64_t h1,h2;
        lcg=1000u+v;build_scene(&st);
        w32(0x7FB8u,v*0x0AAAAAABu);
        st2=st;
        h1=render(&st);
        { unsigned blits=sys.blits;
          if(st.fault)FAIL("view %u: port fault %08X",v,st.fault);
          if(blits<320u)FAIL("view %u: only %u blits",v,blits);
          /* every column wrote its stage-2 record */
          for(unsigned c=0;c<320u;c++)if(!bus_rd(&sys,AVP_VIEW3D_COLUMN_TABLE+8u*c,4))FAIL("view %u: column %u has no record",v,c);
          if(bus_rd(&sys,0x7F80u,4)!=0xFFFFFFFEu)FAIL("view %u: $7F80 handshake",v);
          lcg=1000u+v;build_scene(&st2);
          w32(0x7FB8u,v*0x0AAAAAABu);
          h2=render(&st2);
          if(h1!=h2||sys.blits!=blits)FAIL("view %u: not deterministic",v); }
        for(int t=0;t<16;t++)typesum[t]+=st.wall_type_hits[t];
        objects+=st.objects_drawn;columns+=st.object_columns;
      }
      for(int t=0;t<16;t++)if(!typesum[t])FAIL("wall type %d never reached by the synthetic scenes",t);
      if(!objects||!columns)FAIL("synthetic objects were not drawn (%u objects, %u columns)",objects,columns);
      printf("test_view3d_port_s25: divider exact; 24 synthetic views, %u objects, %u object columns, all 16 wall types\n",
             objects,columns); }
    return 0;
}
