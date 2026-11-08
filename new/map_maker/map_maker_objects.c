#include "map_maker_objects.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const char *avp_mm_object_name(unsigned def)
{
    static const char *names[60]={
        "ALIEN", /* 0 */
        "PREDATOR", /* 1 */
        "HUMAN", /* 2 */
        "EGG", /* 3 */
        "FACEHUG", /* 4 */
        "ACRAWL", /* 5 */
        "SMART", /* 6 */
        "BOLT", /* 7 */
        "FIRE", /* 8 */
        "SPARK1", /* 9 */
        "SPARK2", /* 10 */
        "AID", /* 11 */
        "AMMO1", /* 12 */
        "AMMO2", /* 13 */
        "BAG", /* 14 */
        "CAN", /* 15 */
        "CART", /* 16 */
        "CHAIN", /* 17 */
        "COCOON", /* 18 */
        "DISK", /* 19 */
        "DRUM", /* 20 */
        "FOOD", /* 21 */
        "GAS (aliases drum_def in retail source)", /* 22 */
        "GREN (retail creature AC_AMMO1)", /* 23 */
        "GROUP (retail creature AC_CART)", /* 24 */
        "SHELLS", /* 25 */
        "TABLE1", /* 26 */
        "TABLE2", /* 27 */
        "TABLE3", /* 28 */
        "HIDE_ALIEN", /* 29 */
        "HIDE_PREDATOR", /* 30 */
        "HIDE_HUMAN", /* 31 */
        "STATIC_ALIEN", /* 32 */
        "STATIC_PRED", /* 33 */
        "STATIC_HUMAN", /* 34 */
        "BITE_ALIEN", /* 35 */
        "TAIL_ALIEN", /* 36 */
        "SWIPE_ALIEN", /* 37 */
        "DEAD_OFF", /* 38 */
        "DEAD_S", /* 39 */
        "DEAD_MED", /* 40 */
        "BED", /* 41 */
        "CRYO", /* 42 */
        "XGEN", /* 43 */
        "NGEN", /* 44 */
        "SGEN", /* 45 */
        "EGEN", /* 46 */
        "WGEN", /* 47 */
        "CGEN", /* 48 */
        "AQSHIELD", /* 49 */
        "QUEEN", /* 50 */
        "SMARTPRED", /* 51 */
        "DEADALIEN", /* 52 */
        "DEADHUMAN", /* 53 */
        "DPRED", /* 54 */
        "DEADHUG", /* 55 */
        "DEADEGG", /* 56 */
        "DQUEEN", /* 57 */
        "EQUEEN", /* 58 */
        "RQUEEN", /* 59 */
    };
    return def<60u?names[def]:"Invalid definition";
}

/* UI palette IDs are not native AMP definition numbers. In particular,
 * palette 0 is Pickup: Food (AMP 21), while the retail AMP definition 0
 * must remain Alien. Never store palette IDs in the Jaguar object sidecar. */
static const unsigned char mm_catalog[]={
    21, 0,1,2,3,4,5,11,12,13,14,15,16,17,18,19,20,22,23,24,25,
    26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,
    46,47,48,49,50,51,52,53,54,55,56,57,58,59
};
unsigned avp_mm_object_catalog_count(void)
{return (unsigned)(sizeof mm_catalog/sizeof mm_catalog[0]);}
unsigned avp_mm_object_catalog_def(unsigned id)
{return id<avp_mm_object_catalog_count()?mm_catalog[id]:60u;}
unsigned avp_mm_object_catalog_id(unsigned amp_def)
{
    for(unsigned i=0;i<avp_mm_object_catalog_count();i++)
        if(mm_catalog[i]==amp_def)return i;
    return avp_mm_object_catalog_count();
}
const char *avp_mm_object_catalog_name(unsigned id)
{
    if(id==0u)return "Pickup: Food";
    return avp_mm_object_name(avp_mm_object_catalog_def(id));
}
static unsigned be16(const unsigned char *p){return ((unsigned)p[0]<<8)|p[1];}
static void wr16(unsigned char *p,unsigned n){p[0]=(unsigned char)(n>>8);p[1]=(unsigned char)n;}
static unsigned crc_bytes(const unsigned char *p,size_t n)
{
    unsigned crc=~0u;
    for(size_t i=0;i<n;i++){
        crc^=p[i];
        for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static void wr32(unsigned char *p,unsigned n)
{p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n;}
static unsigned rd32(const unsigned char *p)
{return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3];}
int avp_mm_object_place(AvpMapMaker *m,double x,double y)
{
    unsigned xx,yy;AvpMmObjectRecord *a;
    if(!avp_mm_test_start_valid(m,x,y)||m->object_def>=60u||
       (m->object_def>=6u&&m->object_def<=10u)||m->object_group>3u)return 0;
    xx=(unsigned)(x*256.0);yy=(unsigned)(y*256.0);
    if(xx>16383u||yy>16383u)return 0;
    for(unsigned i=0;i<m->object_count;i++){
        a=&m->objects[i];
        if((a->x8>>8)==((unsigned)x)&&(a->y8>>8)==((unsigned)y)){
            a->x8=(u16)xx;a->y8=(u16)yy;
            a->def=(u8)m->object_def;a->group=(u8)m->object_group;
            m->dirty=1;snprintf(m->message,sizeof m->message,"Object marker updated (AMP definition %u).",m->object_def);
            return 1;
        }
    }
    if(m->object_count>=AVP_MM_MAX_OBJECTS)return 0;
    a=&m->objects[m->object_count];
    m->selected_objects[m->object_count]=0;
    m->object_count++;a->x8=(u16)xx;a->y8=(u16)yy;
    a->def=(u8)m->object_def;a->group=(u8)m->object_group;
    m->dirty=1;snprintf(m->message,sizeof m->message,"Object marker placed (AMP definition %u).",m->object_def);
    return 1;
}
int avp_mm_object_remove(AvpMapMaker *m,unsigned x,unsigned y)
{
    if(!m)return 0;
    for(unsigned i=0;i<m->object_count;i++)if((m->objects[i].x8>>8)==x&&
       (m->objects[i].y8>>8)==y){
        for(unsigned j=i+1u;j<m->object_count;j++){
            m->objects[j-1u]=m->objects[j];
            m->selected_objects[j-1u]=m->selected_objects[j];
        }
        --m->object_count;m->selected_objects[m->object_count]=0;m->dirty=1;
        snprintf(m->message,sizeof m->message,"Object marker removed.");return 1;
    }
    return 0;
}
int avp_mm_objects_save(const AvpMapMaker *m,const char *path)
{
    unsigned char head[16],entries[AVP_MM_MAX_OBJECTS*6u];FILE *f;size_t n;
    if(!m||!path||m->level<1u||m->level>15u||m->object_count>AVP_MM_MAX_OBJECTS)return 0;
    n=(size_t)m->object_count*6u;
    for(unsigned i=0;i<m->object_count;i++){
        const AvpMmObjectRecord *a=&m->objects[i];
        if(a->x8>=m->w*256u||a->y8>=m->h*256u||a->def>=60u||a->group>3u)return 0;
        wr16(entries+i*6u,a->x8);wr16(entries+i*6u+2u,a->y8);
        entries[i*6u+4u]=a->def;entries[i*6u+5u]=a->group;
    }
    memcpy(head,"AVPOBJ01",8u);wr16(head+8,m->level);wr16(head+10,m->object_count);
    wr32(head+12,crc_bytes(entries,n));
    f=fopen(path,"wb");if(!f)return 0;
    int ok=fwrite(head,1,sizeof head,f)==sizeof head&&fwrite(entries,1,n,f)==n;
    if(fclose(f))ok=0;
    return ok;
}
int avp_mm_objects_load(AvpMapMaker *m,const char *path)
{
    unsigned char head[16],entries[AVP_MM_MAX_OBJECTS*6u];
    AvpMmObjectRecord objects[AVP_MM_MAX_OBJECTS];FILE *f;unsigned count;size_t n;
    if(!m||!path)return 0;
    f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(head,1,sizeof head,f)==sizeof head&&!memcmp(head,"AVPOBJ01",8u)
       &&be16(head+8)==m->level;
    count=ok?be16(head+10):0u;
    if(count>AVP_MM_MAX_OBJECTS)ok=0;
    n=(size_t)count*6u;
    if(ok)ok=fread(entries,1,n,f)==n&&fgetc(f)==EOF&&crc_bytes(entries,n)==rd32(head+12);
    if(fclose(f))ok=0;
    if(!ok)return 0;
    for(unsigned i=0;i<count;i++){
        AvpMmObjectRecord *a=&objects[i];
        a->x8=(u16)be16(entries+i*6u);a->y8=(u16)be16(entries+i*6u+2u);
        a->def=entries[i*6u+4u];a->group=entries[i*6u+5u];
        if(a->x8>=m->w*256u||a->y8>=m->h*256u||a->def>=60u||a->group>3u ||
           !avp_mm_test_start_valid(m,(double)a->x8/256.0,(double)a->y8/256.0))return 0;
    }
    memcpy(m->objects,objects,(size_t)count*sizeof(*objects));m->object_count=count;
    memset(m->selected_objects,0,sizeof m->selected_objects);
    return 1;
}
