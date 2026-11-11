/* High-level readable C reconstruction of the retail MJP front-end block.
 * Retail addresses/function boundaries come from the exact MJP reconstruction.
 * Object list phrase encoding and image pixels are platform/resource backends. */
#include "mjp.h"
#include "avp_runtime.h"
#include "joypad.h"
#include "player.h"
#include "objects.h"
#include "eeprom.h"
#include "music.h"
#include <string.h>

char mjp_games[4]="xxx";s8 grunke;s32 Count_Do,Count,Choice;AvpMjpObject *Ob_Addr;u32 Ob_Data;s32 Ob_Pos_X,Ob_Pos_Y,Ob_Height,Ob_Width,Ob_Trans,Ob_Scale;
/* Retail MJP global at $0004BE24.  The host object builder keeps it as a normal
 * signed Y coordinate instead of exposing its historical RAM address. */
s32 High_Line;
static s32 Zero_Line;
static u32 mjp_pad;
static AvpMjpMode current_mjp_mode=AVP_MJP_TITLE;
static int escape_skip_enabled;
static s32 mjp_pad_wait;
static AvpMjpObject *pool;static unsigned pool_cap;static const u8 *save_slots;static size_t save_stride=20;static const char *current_text;static unsigned selected_character;
static int text_abort;
/* Retail Do_Escap $01DC52/$01DCA0/$01DD78 increments the actual
 * Show_Tex Y cursor by the $4BEA8 14-pixel line step between successive
 * countdown records on the SAME page. The renderer's line index must carry
 * that source-owned advance, instead of restarting each word on row zero. */
static unsigned mjp_escape_text_line;
/* Original $4BEC8: the spoken Five/Four/Three/Two/One/Zero cues suppress
 * TYP1B even though later Escape report pages enable it ($01E226). */
static unsigned mjp_suppress_typing_sound;

typedef struct AvpMjpFameEntry { char species; char name[7]; s32 score; } AvpMjpFameEntry;
static AvpMjpFameEntry fame[5];
static const char fame_alphabet[33]="ABCDEFGHIJKLMNOPQRSTUVWXYZ!#&$. ";
static const AvpMjpFameEntry fame_default[5]={
    {'m',"MIKE  ",1220000},{'a',"ANDY  ",800000},{'p',"PURPLE",600000},
    {'a',"JAMES ",400000},{'m',"KEONI ",200000}
};

static u32 load_be32_mjp(const u8 *p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static void store_be32_mjp(u8 *p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
static unsigned fame_species_code(char c){return c=='m'?0u:(c=='a'?1u:2u);}
static char fame_species_char(unsigned v){static const char map[4]={'m','a','p','p'};return map[v&3u];}
static unsigned fame_char_index(char c){for(unsigned i=0;i<32u;i++)if(fame_alphabet[i]==c)return i;return 31u;}
static u32 fame_pack_word(const AvpMjpFameEntry *e){
    static const unsigned sh[6]={25,20,15,10,5,0};
    u32 v=(u32)fame_species_code(e->species)<<30;
    for(unsigned i=0;i<6u;i++)v|=(u32)fame_char_index(e->name[i])<<sh[i];
    return v;
}
static void fame_unpack_word(AvpMjpFameEntry *e,u32 v){
    static const unsigned sh[6]={25,20,15,10,5,0};
    e->species=fame_species_char(v>>30);
    for(unsigned i=0;i<6u;i++)e->name[i]=fame_alphabet[(v>>sh[i])&31u];
    e->name[6]=0;
}

static u32 op_a[32][8],op_b[32][8];
static u32 mjp_link,mjp_phase,mjp_phase2,mjp_list_done;
static s32 mjp_d2_residue;
static s32 wow_mode;
static int obj_index(const AvpMjpObject *o){
    if(!pool||!o||o<pool||o>=pool+pool_cap)return -1;
    return (int)(o-pool);
}
static u32 *raw_for(const AvpMjpObject *o){int i=obj_index(o);return (i>=0&&i<32)?op_a[i]:NULL;}

void avp_mjp_set_lines(s32 zero_line,s32 high_line){Zero_Line=zero_line;High_Line=high_line;}
void avp_mjp_set_gpu_phase(u32 phase){mjp_phase=phase;}
void avp_mjp_bind_objects(AvpMjpObject*o,unsigned n){
    pool=o;pool_cap=n;Ob_Addr=NULL;memset(op_a,0,sizeof(op_a));memset(op_b,0,sizeof(op_b));
    mjp_link=mjp_phase=mjp_phase2=mjp_list_done=0;mjp_d2_residue=0;wow_mode=0;
}void avp_mjp_bind_save_slots(const u8*s,size_t stride){save_slots=s;if(stride)save_stride=stride;}
static void evt(unsigned e,s32 a,s32 b){const AvpRuntimeOps*o=avp_runtime_ops();if(o->frontend_event)o->frontend_event(o->user,e,a,b);}
void mjppad(void){readpad();mjp_pad=~joy_cur;}
void Load_GPU(void){evt(0x4d475055,0,0);}
void Clear(void){Waitbl();evt(0x434c5231,0,0);}
void Clear2(void){Waitbl();evt(0x434c5232,0,0);}
static void clear2_target(unsigned target){Waitbl();evt(0x434c5232,(s32)target,0x118);}
void Clear_Bu(void){for(unsigned i=0;i<3u;i++)clear2_target(i);}
AvpMjpObject *Make_New(void){
    /* Exact CPU-side OP phrase construction from retail $01E240.  The caller
     * chooses Ob_Addr; the routine never allocates. */
    AvpMjpObject *o=Ob_Addr;u32 *r;int idx;
    if(!o)return NULL;
    idx=obj_index(o);r=raw_for(o);
    memset(o,0,sizeof(*o));
    o->data=Ob_Data;o->x=Ob_Pos_X;o->y=Ob_Pos_Y;o->height=Ob_Height;o->width=Ob_Width;o->flags=(u32)Ob_Trans;
    if(r){
        u32 addr=0x0004FC00u+(u32)idx*0x20u;
        u32 d1=(addr+0x20u)>>3,d2=Ob_Data>>3,d0=d1>>8;
        d0|=d2<<11;r[0]=d0;
        d0=0;d0|=(u32)Ob_Pos_Y<<4;d0|=(u32)Ob_Height<<14;d0|=d1<<24;r[1]=d0;
        d0=(u32)Ob_Width>>2;d2=d0;d0>>=4;d0|=(u32)Ob_Trans;r[2]=d0;
        d0=(u32)Ob_Pos_X|0x00004000u|0x00008000u;d0|=d2<<18;d0|=d2<<10;r[3]=d0;
        r[4]=r[5]=r[6]=r[7]=0;
    }
    return o;
}
void Make_Sca(void){
    u32 *r=raw_for(Ob_Addr);if(!Ob_Addr)return;
    Ob_Addr->scaled=1;Ob_Addr->scale_x=Ob_Addr->scale_y=0;
    if(r){r[1]|=1u;r[4]=0;}
}
/* Decode a 68000 longword as signed without relying on an out-of-range cast. */
static s32 mjp_longword(u32 word)
{
    return word<=0x7fffffffu ? (s32)word : (s32)((s64)word-INT64_C(4294967296));
}
AvpMjpObject *New_Make(void){
    /* Retail $01E2F2: signed scale/position words, 32-bit wrapping SUB/LSL. */
    u32 *r=raw_for(Ob_Addr);
    if(!Ob_Addr)return NULL;
    Ob_Addr->scaled=1;Ob_Addr->scale_x=Ob_Addr->scale_y=Ob_Scale;
    {
        u32 d=(u32)Ob_Scale-8u;
        u32 d2=d<<1;
        Ob_Addr->x=mjp_longword((u32)Ob_Addr->x-d2);
        Ob_Addr->y=mjp_longword((u32)Ob_Addr->y-d);
        if(r){r[1]|=1u;r[4]=0;r[5]=0x00200000u|(u32)Ob_Scale|((u32)Ob_Scale<<8);r[3]-=d2;r[1]-=d2<<3;}
        /* New_Make returns with (scale-8)*16 in D2, consumed by Sel_Upda. */
        mjp_d2_residue=mjp_longword(d2<<3);
    }
    return Ob_Addr;
}
void Change_Y(s32 y){u32*r=raw_for(Ob_Addr);if(Ob_Addr)Ob_Addr->y=y;if(r)r[1]=(r[1]&0xffffc007u)|((u32)y<<3);}
void Hide_Obj(void){u32*r=raw_for(Ob_Addr);if(Ob_Addr)Ob_Addr->hidden=1;if(r)r[1]|=1u<<12;}
void Unhide_O(void){u32*r=raw_for(Ob_Addr);if(Ob_Addr)Ob_Addr->hidden=0;if(r)r[1]&=~(1u<<12);}
void No_Scale(void){u32*r=raw_for(Ob_Addr);if(Ob_Addr)Ob_Addr->scaled=0;if(r)r[1]&=~1u;}
void Change_D(u32 d){u32*r=raw_for(Ob_Addr);mjp_link=d;if(r)r[0]=(r[0]&0x000007ffu)|(mjp_link<<8);}
void Change_X(s32 x){u32*r=raw_for(Ob_Addr);if(Ob_Addr)Ob_Addr->x=x;if(r)r[3]=(r[3]&0xfffff000u)|((u32)x&0xfffu);}
void Change_S(s32 x,s32 y){u32*r=raw_for(Ob_Addr);if(Ob_Addr){Ob_Addr->scaled=1;Ob_Addr->scale_x=x;Ob_Addr->scale_y=y;}if(r){u32 d1=(u32)x|((u32)x<<8)|(((u32)y+32u)<<16);r[5]=d1;}}

static void copy_op_records(unsigned count,unsigned longs_per_record,int copy_tail)
{
    if(op_a[0][0]==0u){mjp_list_done=1;return;}
    for(unsigned i=0;i<count&&i<32u;i++){
        op_b[i][0]=op_a[i][0]+2u;
        for(unsigned j=1;j<longs_per_record&&j<8u;j++)op_b[i][j]=op_a[i][j];
    }
    if(copy_tail && count<32u){for(unsigned j=0;j<6u;j++)op_b[count][j]=op_a[count][j];}
    mjp_list_done=1;
}
void Lister4(void){
    if(op_a[0][0]!=0u && wow_mode!=0){op_a[2][0]=(op_a[2][0]&0x7ffu)|(mjp_link<<8);}
    copy_op_records(6u,4u,0);evt(0x4c535434,(s32)mjp_list_done,0);
}
void Lister3(void){copy_op_records(8u,6u,1);evt(0x4c535433,(s32)mjp_list_done,0);}
void Lister(void){copy_op_records(10u,6u,0);evt(0x4c535452,(s32)mjp_list_done,0);}
void Update(void){
    /* $01E7B4: phase-dependent x phrase plus the packed scale pair. */
    u32 d2=op_a[2][3]&0xfffff000u;
    u32 d1=((32u-mjp_phase)<<3)+0x2au;op_a[2][3]=d2|d1;
    d1=44u-mjp_phase;op_a[2][5]=d1|(d1<<8)|((d1+32u)<<16);
    evt(0x55504454,(s32)mjp_phase,0);
}
void Update2(void){
    /* $01E706: exact integer phase math used by Clear_Bu/title transitions. */
    u32 p=mjp_phase2,d0=p*4u+(p>>1),d1=((u32)High_Line<<1)+0xd8u-d0;
    op_a[4][1]=(op_a[4][1]&0xffffc00fu)|(d1<<3);
    d1=32u-(p>>1);op_a[4][5]=d1|(d1<<8)|((d1+32u)<<16);
    d0=p;
    {
        u32 low=op_a[4][3]&0xfffff000u;
        if(d0==0u)d1=0x190u;
        else{d1=8u+d0*3u;if((d0&1u)==0u)d1+=3u;d0>>=1;d1+=d0*3u;}
        op_a[4][3]=low|d1;
    }
    evt(0x55504432,(s32)mjp_phase2,0);
}
void Sel_Upda(void){
    /* Retail $01E462: patch slot-4 link, then (once shared GPU phase >=2)
     * scale and move slot 4 with exact integer formulas.  The second scale
     * component is the live D2 register residue; retain it explicitly rather
     * than inventing a symmetric scale. */
    op_a[4][0]=(op_a[4][0]&0x7ffu)|(mjp_link<<8);
    if(mjp_phase>=2u && pool&&pool_cap>4u){
        s32 d=(s32)(16u-mjp_phase);
        Ob_Addr=pool+4;
        Change_S((s32)mjp_phase+16,mjp_d2_residue);
        Change_X(d*5);
        Change_Y(d*5+High_Line);
    }
    evt(0x53454c55,(s32)mjp_phase,0);
}
void Make_TLi(void){
    /* Retail $01E530..$01E704.  Resource addresses TL picture 0/1 and the
     * back-buffer pixel pointer are relocatable; all CPU object coordinates,
     * sizes, hidden/scaled state, stop record and timing remain explicit. */
    if(!pool||pool_cap<9u){evt(0x544c4953,0,0);return;}
    Ob_Addr=pool+2;Ob_Data=0;Ob_Pos_Y=Zero_Line+0x20;Ob_Pos_X=2;Ob_Width=320;Ob_Height=0x6e;Ob_Trans=0x8000;Make_New();Make_Sca();
    evt(0x544c5230,2,0); /* bind retail $4BF40 resource */
    Ob_Addr=pool+4;Ob_Data=0;Ob_Pos_Y=Zero_Line+0x5e;Ob_Pos_X=0x190;Ob_Width=320;Ob_Height=0x52;Ob_Trans=0x8000;Make_New();Make_Sca();Hide_Obj();
    evt(0x544c5231,4,0); /* bind retail $4BF44 resource */
    Ob_Addr=pool+6;Ob_Data=0;Ob_Pos_Y=Zero_Line+0x62;Ob_Pos_X=0;Ob_Width=320;Ob_Height=0xb6;Ob_Trans=0x8000;Make_New();Hide_Obj();
    /* Slot 8 is the literal stop object {0,4}. */
    memset(op_a[8],0,sizeof(op_a[8]));op_a[8][1]=4u;
    evt(0x4f4c5052u,0x7548,0); /* publish PRE_LIST */
    One_Tick();mjp_phase=0;Update();One_Tick();
    Ob_Addr=pool;Ob_Data=0;Ob_Pos_Y=Zero_Line;Ob_Pos_X=0;Ob_Width=320;Ob_Height=0x118;Ob_Trans=0;Make_New();
    evt(0x544c4953,1,0);
}

void Make_Bet(void){
    u32 *r;
    Ob_Data=0;Ob_Pos_Y=High_Line;Ob_Pos_X=320;Ob_Height=280;Ob_Trans=0;Ob_Width=16;
    if(!Make_New())return;
    r=raw_for(Ob_Addr);
    if(r){r[2]|=1u<<16;r[3]&=0xffff8fffu;}
    Ob_Addr->flags|=1u<<16;
}

void EncodeGa(void){static const char mapx[]="mapx";const u8 *slots=save_slots?save_slots:cartcopy;size_t stride=save_slots?save_stride:AVP_SAVE_SIZE;for(unsigned i=0;i<3;i++){unsigned t=3;const u8*p=slots+i*stride;u32 exists=((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];if(exists){u32 q=((u32)p[16]<<24)|((u32)p[17]<<16)|((u32)p[18]<<8)|p[19];t=q&3u;}mjp_games[i]=mapx[t];}mjp_games[3]=0;}
void One_Tick(void){VSync();evt(0x5449434b,0,0);}
void Waitbl(void){const AvpRuntimeOps*o=avp_runtime_ops();if(o->wait_blitter)o->wait_blitter(o->user);else evt(0x424c4954,0,0);}
void Waitnow(void){do{mjppad();}while((mjp_pad&(1u<<PAUSE))==0u);}
void Wait_Up(void){const u32 release_mask=(1u<<FIRE_A)|(1u<<FIRE_B)|(1u<<FIRE_C);do{One_Tick();mjppad();}while(mjp_pad&release_mask);}
void Quick_St(void){One_Tick();evt(0x51535452,0,0);}
void Make_Rel(void){
    /* Retail places the nine records at OP-list offsets +$20,+$60,...,+$220:
     * object slots 1,3,5,...,17.  It does not consume a sequential allocator. */
    evt(0x52454c,9,0);
    if(pool){
        for(unsigned i=0;i<9u;i++){
            unsigned slot=1u+i*2u;
            if(slot>=pool_cap)break;
            Ob_Addr=&pool[slot];
            Make_Bet();
        }
    }
}
void Pause(void){
    /* Retail $01F05E: a confirm/skip press returns immediately; it does not
     * perform Make_Rel.  Otherwise consume exactly Count_Do ticks. */
    while(Count_Do!=0){mjppad();if(mjp_pad&0x22002000u)return;One_Tick();--Count_Do;}
}
static int mjp_typewriter_sound_mode(void)
{
    return current_mjp_mode==AVP_MJP_SELECT ||
           current_mjp_mode==AVP_MJP_INTRO ||
           current_mjp_mode==AVP_MJP_ESCAPE ||
           current_mjp_mode==AVP_MJP_SIM_TERMINATED;
}
static void mjp_text_char(unsigned line,unsigned x,unsigned char c)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(o->draw_text){char t[2]={(char)c,0};o->draw_text(o->user,line,t,x);}
}
void New_Text(const char*t){current_text=t;}
/* Show_Tex cursor X origin, retail $4BEB0 (copied to $4BE94 at $01D29C and
 * at each new line, $01D2D4).  Do_Intro/Do_Escap set $18; other scenes keep
 * the port's established origin until individually verified. */
static unsigned mjp_text_x0;
/* Only modes that actually draw the green Show_Tex square need a cursor
 * retirement callback. Keep SELECT/FAME/other MJP screens free from new
 * incidental event-triggered host repaints. */
static void mjp_cursor_retire(void)
{
    if(current_mjp_mode==AVP_MJP_INTRO ||
       current_mjp_mode==AVP_MJP_ESCAPE ||
       current_mjp_mode==AVP_MJP_SIM_TERMINATED ||
       current_mjp_mode==AVP_MJP_BASE_EXPLODES)
        evt(0x4d4a505au,0,0); /* MJPZ */
}
void Show_Tex(void)
{
    /* Retail $01D1E0 walks a sequence of NUL-terminated lines terminated by
     * an empty line.  Its MJP font is seven pixels wide; '@' advances by half
     * that width, and space/'~'/'@' all select the blank underscore cell.
     * Outside Hall of Fame, text advances one VBL per glyph.  A/B/C makes the
     * remainder of a non-intro message fast; in Intro it aborts the message. */
    const unsigned glyph_width=7u;
    const AvpRuntimeOps *o=avp_runtime_ops();
    const unsigned char *p=(const unsigned char*)current_text;
    unsigned line=0;
    /* Retail Show_Tex at $01D3FE checks $4BEC8 and branches straight
     * to the next character, bypassing BOTH the DEL cursor and the VBlank
     * wait. R95 muted only TYP1B, unintentionally delaying the escape
     * countdown by every padding-space and letter glyph. */
    int fast=(current_mjp_mode==AVP_MJP_FAME || mjp_suppress_typing_sound);
    text_abort=0;
    /* MJPZ is PC presentation metadata: the original screen contains a
     * cursor only while Show_Tex is actively revealing characters. Retiring
     * it changes no 68000 timing, audio, text, or screen-clearing operations.
     * Always retire the previous message's cursor, including empty input. */
    mjp_cursor_retire(); /* no cursor until a live glyph advances */
    if(!p)return;
    for(;;){
        unsigned x=mjp_text_x0;
        while(*p){
            unsigned char c=*p++;
            unsigned char draw=(c==' '||c=='~'||c=='@')?'_':c;
            /* Retail Show_Tex $01D2FE..$01D376 skips TFX for ASCII
             * space (mapped to underscore) AND a literal underscore.
             * Tilde/@ are mapped later, after the TFX decision. */
            if(mjp_typewriter_sound_mode() && !mjp_suppress_typing_sound && !fast && c!=' ' && c!='_' && c!=0 && o->frontend_event)
                o->frontend_event(o->user,0x4d4a5041u,0x000221B8, -1); /* MJPA */
            /* One_Tick's text position belongs to the original global Y cursor.
             * Advance the host line only for the first spoken escape page's
             * successive messages, preserving all authored line breaks. */
            const unsigned display_line=line+
                (current_mjp_mode==AVP_MJP_ESCAPE?mjp_escape_text_line:0u);
            mjp_text_char(display_line,x,draw);
            x+=(c=='@')?(glyph_width>>1):glyph_width;
            /* Presentation metadata, not an extra text/audio/video tick:
             * The ROM's visible DEL/0x7F green block follows the next
             * glyph X. It is distinct from silent/transparent ASCII '_'. */
            if(!fast && (current_mjp_mode==AVP_MJP_INTRO ||
                         current_mjp_mode==AVP_MJP_ESCAPE ||
                         current_mjp_mode==AVP_MJP_SIM_TERMINATED ||
                         current_mjp_mode==AVP_MJP_BASE_EXPLODES))
                evt(0x4d4a5043u,(s32)x,(s32)display_line); /* MJPC */
            if(!fast){
                One_Tick();mjppad();
                if(mjp_pad&0x22002000u){
                    if(current_mjp_mode==AVP_MJP_INTRO){
                        text_abort=1;
                        mjp_cursor_retire(); /* skipped text */
                        return;
                    }
                    fast=1;
                }
            }
        }
        /* The retail renderer emits its underscore/blank cursor cell at each
         * line terminator before testing for the final empty line. */
        mjp_text_char(line+(current_mjp_mode==AVP_MJP_ESCAPE?mjp_escape_text_line:0u),x,'_');
        ++p;
        if(*p==0){
            mjp_cursor_retire(); /* text completed, no cursor on hold */
            return;
        }
        ++line;
    }
}
static void fame_format_score(s32 score_value,char out[11])
{
    /* Retail local $01CC70 converts each score through the literal divisor
     * table 1,000,000,000 .. 1.  Each digit is produced by repeated
     * subtraction; leading zeroes remain spaces until the first non-zero
     * digit, except that the units digit is always emitted. */
    static const u32 divs[10]={1000000000u,100000000u,10000000u,1000000u,100000u,10000u,1000u,100u,10u,1u};
    u32 rem=(u32)score_value;
    int started=0;
    for(unsigned i=0;i<10u;i++){
        unsigned digit=0;
        while(rem>=divs[i]){rem-=divs[i];++digit;}
        if(!started && digit==0u && divs[i]!=1u)out[i]=' ';
        else{started=1;out[i]=(char)('0'+digit);}
    }
    out[10]=0;
}
static void fame_render_table(void)
{
    /* The original 68000 builds five 23-byte display records and then walks
     * them through Fame_Tex.  Portable C stores the same semantic fields in
     * AvpMjpFameEntry and reconstructs the CPU-produced decimal field here;
     * only glyph pixels/blitter execution are delegated to the host. */
    const AvpRuntimeOps *o=avp_runtime_ops();
    for(unsigned i=0;i<5u;i++){
        char score_text[11];
        fame_format_score(fame[i].score,score_text);
        evt(0x464d5350u,(s32)i,(s32)fame_species_code(fame[i].species)); /* FMSP */
        if(o->draw_text){
            char name[7];memcpy(name,fame[i].name,6);name[6]=0;
            o->draw_text(o->user,i,name,45u);
            o->draw_text(o->user,i,score_text,128u);
        }
    }
}
void Fame_Tex(void)
{
    /* Retail $01D462 owns the species/font choice and the character walk;
     * the blitter register program for each glyph remains a Jaguar backend
     * boundary.  When called for authored text, preserve that CPU walk here. */
    const AvpRuntimeOps *o=avp_runtime_ops();
    const unsigned char *p=(const unsigned char*)current_text;
    if(!p){fame_render_table();return;}
    if(*p){
        unsigned species=fame_species_code((char)*p++);
        evt(0x464d4654u,(s32)species,0); /* FMFT: font/species selection */
    }
    unsigned x=0;
    while(*p){
        unsigned char c=*p++;
        if(o->draw_text){char t[2]={(char)c,0};o->draw_text(o->user,0,t,x);}
        x+=11u;
    }
}
void Pack_Fam(void){
    /* Retail $01CCE2: five {packed identity/name, score} pairs at cartcopy+60. */
    u8 *p=cartcopy+AVP_N_SAVES*AVP_SAVE_SIZE;
    for(unsigned i=0;i<5u;i++){store_be32_mjp(p,fame_pack_word(&fame[i]));store_be32_mjp(p+4,(u32)fame[i].score);p+=8;}
}
void Unpack_F(void){
    /* Retail $01CD1E/$01CDC6. */
    const u8 *p=cartcopy+AVP_N_SAVES*AVP_SAVE_SIZE;
    for(unsigned i=0;i<5u;i++){fame_unpack_word(&fame[i],load_be32_mjp(p));fame[i].score=(s32)load_be32_mjp(p+4);p+=8;}
}
void Init_Hig(void){
    /* Retail $01D18A copies the five default 23-byte display records and five
     * score longs, then Pack_Fam writes their packed EEPROM form. */
    memcpy(fame,fame_default,sizeof(fame));
    Pack_Fam();
}
static int mjp_text_id(u32 text_id);
static int get_choice(AvpMjpMode m)
{
    const AvpRuntimeOps*o=avp_runtime_ops();
    return o->frontend_choice?o->frontend_choice(o->user,(unsigned)m,&Choice):0;
}
static int mjp_pressed(unsigned bit){return (mjp_pad&(1u<<bit))!=0u;}
static void wait_bit_release(unsigned bit){do{One_Tick();mjppad();}while(mjp_pressed(bit));}

static char fame_prev_char(char c)
{
    switch(c){
    case 'A':return ' '; case ' ':return '&'; case '&':return '#'; case '#':return '$';
    case '$':return '!'; case '!':return '.'; case '.':return 'Z'; default:return (char)(c-1);
    }
}
static char fame_next_char(char c)
{
    switch(c){
    case 'Z':return '.'; case '.':return '!'; case '!':return '$'; case '$':return '#';
    case '#':return '&'; case '&':return ' '; case ' ':return 'A'; default:return (char)(c+1);
    }
}
static void fame_repeat_wait(u32 held,s32 *repeat_delay)
{
    s32 count=*repeat_delay;
    while(count-- > 0){
        One_Tick();mjppad();
        if(mjp_pad!=held){*repeat_delay=27;return;}
    }
    if(*repeat_delay!=7)*repeat_delay-=10;
}
static void fame_edit_name(unsigned slot)
{
    const AvpRuntimeOps*o=avp_runtime_ops();
    char host_name[7];
    if(o->frontend_fame_name && o->frontend_fame_name(o->user,host_name)){
        for(unsigned i=0;i<6u;i++)fame[slot].name[i]=host_name[i]?host_name[i]:' ';
        fame[slot].name[6]=0;
        return;
    }
    unsigned cursor=0;
    char current='A';
    char displayed[6]={0,0,0,0,0,0};
    u32 last_pad=~0u;
    s32 repeat_delay=27;
    memset(fame[slot].name,' ',6);fame[slot].name[6]=0;
    for(;;){
        One_Tick();mjppad();
        for(unsigned j=0;j<6u;j++)if(displayed[j]!=fame[slot].name[j]){
            displayed[j]=fame[slot].name[j];
            evt(0x464e4348u,(s32)j,(s32)(unsigned char)displayed[j]); /* FNCH */
        }
        evt(0x464e414du,(s32)cursor,(s32)(unsigned char)current); /* FNAM */
        u32 pad=mjp_pad;
        if(pad!=last_pad)repeat_delay=27;
        if(pad&(1u<<JOY_UP)){current=fame_prev_char(current);last_pad=pad;fame_repeat_wait(pad,&repeat_delay);continue;}
        if(pad&(1u<<JOY_DOWN)){current=fame_next_char(current);last_pad=pad;fame_repeat_wait(pad,&repeat_delay);continue;}
        if(pad&(1u<<JOY_LEFT)){
            if(cursor){fame[slot].name[cursor]=current;--cursor;current=fame[slot].name[cursor];}
            last_pad=pad;fame_repeat_wait(pad,&repeat_delay);continue;
        }
        if(pad&(1u<<JOY_RIGHT)){
            if(cursor<5u){fame[slot].name[cursor]=current;++cursor;current=fame[slot].name[cursor];}
            last_pad=pad;fame_repeat_wait(pad,&repeat_delay);continue;
        }
        if(pad&0x22002000u){fame[slot].name[cursor]=current;break;}
        last_pad=pad;
    }
    for(unsigned j=0;j<6u;j++)
        evt(0x464e4348u,(s32)j,(s32)(unsigned char)fame[slot].name[j]);
    evt(0x464e444eu,0,0); /* FNDN: editor completed */
}

void Do_Credits(void); /* Backend-only display, entered inside Do_Title. */
static s32 title_pad_wait;
static int title_save_menu;
static void title_move_up(void){if(Choice>0)--Choice;evt(0x544d5550u,Choice,-1);}
static void title_move_down(void){if(Choice<3)++Choice;evt(0x544d444eu,Choice,1);}
static void title_input_loop(void)
{
    title_pad_wait=0;
    for(;;){
        One_Tick();
#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
        /* The original Jaguar menu counts 800 idle ticks before entering the
         * attract sequence. F2 is a PC-only overlay: leaving that counter
         * running behind it used to dismiss the setup menu unexpectedly.
         * Reset (not merely freeze) the idle counter while F2 is visible so
         * closing F2 starts a fresh idle interval. Without a PC callback the
         * retail counter remains exactly as before. */
        {const AvpRuntimeOps *o=avp_runtime_ops();
         if(o->frontend_mp_menu_open && o->frontend_mp_menu_open(o->user))
             title_pad_wait=0;
         else ++title_pad_wait;}
#else
        ++title_pad_wait;
#endif
#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
        {const AvpRuntimeOps *o=avp_runtime_ops();
         if(!title_save_menu && o->frontend_mp_launch_pending &&
            o->frontend_mp_launch_pending(o->user)){
             Choice=6;return; /* PC-only F2 Start Game sentinel */
         }}
#endif
        mjppad();
        if(mjp_pressed(JOY_UP)){title_move_up();title_pad_wait=0;wait_bit_release(JOY_UP);continue;}
        if(mjp_pressed(JOY_DOWN)){title_move_down();title_pad_wait=0;wait_bit_release(JOY_DOWN);continue;}
        if(mjp_pad&0x22002000u){title_pad_wait=0;return;}
        if(!title_save_menu && title_pad_wait>=0x320){Choice=4;return;}
    }
}
static int title_save_slot(void)
{
    title_save_menu=1;Choice=0;EncodeGa();evt(0x53415645u,0,0);Wait_Up();
    for(;;){
        title_input_loop();
        if(Choice==3){
            title_save_menu=0;
            evt(0x53565854u,0,0);
            /* The CANCEL key also means FIRE on the title screen.  Consume
             * its release before returning, or that same held press selects
             * NEW GAME and erroneously opens character selection. */
            Wait_Up();
            return 0;
        }
        if(Choice>=0 && Choice<3 && mjp_games[Choice]!='x'){
            Choice=-(Choice+1);title_save_menu=0;return 1;
        }
    }
}
void Do_Title(void)
{
    Clear_Bu();EncodeGa();evt(0x544d454eu,0,0);title_save_menu=0;Choice=0;
    /* Retail title setup advances at least one VBL before the input selector. */
    One_Tick();
    /* A native menu may report the final retail Choice directly.  Otherwise
     * execute the recovered selector, including the 800-tick attract timeout
     * and the negative save-slot encoding. */
    if(get_choice(AVP_MJP_TITLE)){
        if(Choice<0){s32 slot=-Choice-1;if(slot<0||slot>=3||mjp_games[slot]=='x')Choice=0;}
        else if(Choice!=0&&Choice!=2&&Choice!=4
#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
                &&Choice!=6
#endif
                )Choice=0;
        return;
    }
    Wait_Up();
    for(;;){
        title_input_loop();
#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
        if(Choice==6)return; /* PC extension: return to native launcher */
#endif
        if(Choice==0||Choice==2||Choice==4)return;
        if(Choice==1){if(title_save_slot())return;Choice=0;continue;}
        if(Choice==3){
            /* Retail Do_Title handles CREDITS within the title selector and
             * resumes that selector when the presentation closes. R138
             * incorrectly returned a new Choice=5 to MAIN, restarting the
             * entire menu and reinitializing its input/resource state.
             *
             * The credits *presentation* is still an unverified PC backend
             * replacement; confine it to that seam, not Jaguar CPU flow. */
            const AvpRuntimeOps *o=avp_runtime_ops();
            evt(0x43524544u,0,0); /* CRED: retail menu credit selection */
            if(o->frontend_load_resource)
                o->frontend_load_resource(o->user,AVP_MJP_CREDITS,238u,0u); /* credits bitmap */
            Do_Credits();
            if(o->frontend_load_resource)
                o->frontend_load_resource(o->user,AVP_MJP_TITLE,208u,0u);
            evt(0x43524558u,0,0); /* CREX: PC presentation cleanup */
            Choice=0;           /* $01F198 -> $01F12A selector; Do_Credits ended with $01FDFC */
            continue;
        }
    }
}

/* Retail RDB.DTA $2B0B8: nine authored 32-step tracks.  $4BF20 is a
 * byte offset (0,4,...,124) into one track, not an angle/interpolation phase. */
static const s32 select_track[9][32]={
 {125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125},
 {125,122,120,117,115,112,110,107,104,102,99,97,94,91,89,86,84,81,79,76,73,71,68,66,63,60,58,55,53,50,48,45},
 {45,48,50,53,55,58,60,63,66,68,71,73,76,79,81,84,86,89,91,94,97,99,102,104,107,110,112,115,117,120,122,125},
 {30,35,41,46,52,57,63,68,74,79,85,90,96,101,107,112,118,123,129,134,140,145,151,156,162,167,173,178,184,189,195,200},
 {200,196,193,189,186,182,178,175,171,167,164,160,157,153,149,146,142,139,135,131,128,124,121,117,113,110,106,102,99,95,92,88},
 {88,86,84,82,81,79,77,75,73,71,69,67,66,64,62,60,58,56,54,52,51,49,47,45,43,41,39,37,36,34,32,30},
 {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10},
 {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,12,13,14,15,16,17,18,19,20,21},
 {21,21,21,20,19,18,17,16,15,14,13,12,11,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10}
};
static unsigned select_phase,select_direction;
static unsigned select_species[3];
/* The selected character is the portrait on the centre/front track.  At
 * phase zero this is track 2; at the right-turn wrap ($7c) it is track 1.
 * Keep the selection and the visible SELV pose sourced from the same track,
 * including when Fire is pressed before any carousel rotation. */
static unsigned select_front_species(void)
{
    unsigned i=(select_phase>>2)&31u;
    unsigned best=0u;
    s32 scale=select_track[6][i];
    if(select_track[7][i]>scale){best=1u;scale=select_track[7][i];}
    if(select_track[8][i]>scale)best=2u;
    return select_species[best];
}
/* Forward the real 32-step OP object state without changing the selector's
 * CPU-side phase, sort order, or the animation table recovered from RDB.DTA.
 * Slots 0,2,4 correspond to the three authored selection objects. */
static void select_emit_object(unsigned slot)
{
    const AvpMjpObject *o;
    u32 packed;
    if(!pool||slot>=pool_cap)return;
    o=&pool[slot];
    packed=((u32)slot<<16)|(((u32)o->scale_x&255u)<<8)|(o->data&255u);
    evt(0x534f424au,(s32)packed,
        (s32)(((u32)(u16)o->x<<16)|(u16)o->y)); /* SOBJ */
}
static void select_emit_objects(void)
{
    select_emit_object(0u);
    select_emit_object(2u);
    select_emit_object(4u);
}
static void select_authored_object(unsigned object_slot,unsigned track,unsigned species)
{
    unsigned i=(select_phase>>2)&31u;
    if(!pool||object_slot>=pool_cap||track>=3u)return;
    Ob_Addr=&pool[object_slot];
    Ob_Data=(u32)species; /* resource identity is rebound by the platform layer */
    Ob_Width=320;Ob_Height=239;Ob_Trans=0;
    Ob_Pos_Y=select_track[track][i];
    Ob_Pos_X=select_track[3u+track][i]-3;
    Make_New();
    Ob_Scale=select_track[6u+track][i];
    New_Make();
}
static void select_pose(void)
{
    /* Retail changes the object/depth ordering at phase $40. */
    if(select_phase<=0x40u){
        select_authored_object(0,0,select_species[0]);
        select_authored_object(2,1,select_species[1]);
        select_authored_object(4,2,select_species[2]);
    }else{
        select_authored_object(0,2,select_species[2]);
        select_authored_object(2,0,select_species[0]);
        select_authored_object(4,1,select_species[1]);
    }
    select_emit_objects();
    evt(0x53454c56u,(s32)select_front_species(),(s32)select_phase); /* SELV: host presentation seam */
}
static void select_expand(void)
{
    /* $01E9B0..$01EABA: eleven authored integer expansion steps. */
    for(Count=0;Count<11;){
        s32 d2,d4;
        One_Tick();
        if(pool&&pool_cap>0u){Ob_Addr=pool;Change_S(Count,Count);d2=((10*320)>>5)-((Count*320)>>5);Change_X((d2>>1)+23);}
        if(pool&&pool_cap>2u){Ob_Addr=pool+2;Change_S(Count,Count);d2=((10*320)>>5)-((Count*320)>>5);Change_X((d2>>1)+193);}
        d4=(Count<<1)+1;
        if(pool&&pool_cap>4u){Ob_Addr=pool+4;Change_S(d4,d4);d2=((21*320)>>5)-((d4*320)>>5);Change_X((d2>>1)+59);}
        select_emit_objects();
        ++Count;
        if(Count==1){One_Tick();evt(0x4f4c5052u,0x7548,0);} /* publish PRE_LIST */
    }
}
/* Retail Pause() permits a *new* Fire skip, but the key that chose the
 * character must not also dismiss the immediately following GPU transition.
 * Preserve the authored VBL counts and intentional skip behavior, while
 * treating the selection-confirmation hold as already consumed. */
static void select_animation_pause(unsigned ticks)
{
    const u32 fire_mask=(1u<<FIRE_A)|(1u<<FIRE_B)|(1u<<FIRE_C);
    u32 held=mjp_pad&fire_mask;
    while(ticks--){
        One_Tick();mjppad();
        u32 pressed=mjp_pad&fire_mask;
        if(pressed&~held)return; /* a new press, not the held selector press */
        held=pressed;
    }
}

void Do_Selec(void)
{
    unsigned chosen;
    Quick_St();
    select_phase=0;select_direction=0;mjp_phase=0;wow_mode=0;
    select_species[0]=PT_PREDATOR;select_species[1]=PT_HUMAN;select_species[2]=PT_ALIEN;
    clear2_target(1);Make_Rel();
    /* Text object/list setup and actual resource addresses are presentation/resource seams. */
    evt(0x53454c45u,0,0);
    select_pose();select_expand();
    if(get_choice(AVP_MJP_SELECT)){
        if(Choice!=PT_HUMAN&&Choice!=PT_ALIEN&&Choice!=PT_PREDATOR)Choice=PT_HUMAN;
        selected_character=(unsigned)Choice;return;
    }
    for(;;){
        int wrapped=0;
        /* Retail selector is stationary until the player requests a direction.
         * The previous hosted reconstruction entered the rotation loop with
         * select_direction=0 and therefore scrolled by itself. */
        One_Tick();mjppad();
        if(mjp_pressed(FIRE_A)||mjp_pressed(FIRE_B)||mjp_pressed(FIRE_C))break;
        if(mjp_pressed(JOY_LEFT))select_direction=1u;
        else if(mjp_pressed(JOY_RIGHT))select_direction=0u;
        else continue;
        wait_bit_release(select_direction?JOY_LEFT:JOY_RIGHT);
        do{
            One_Tick();select_pose();
            if(select_direction){
                select_phase+=4u;
                if(select_phase>=0x80u){
                    unsigned a=select_species[0],b=select_species[1],c=select_species[2];
                    select_phase=0;select_species[0]=c;select_species[1]=a;select_species[2]=b;wrapped=1;
                }
            }else{
                if(select_phase>=4u)select_phase-=4u;
                else{
                    unsigned a=select_species[0],b=select_species[1],c=select_species[2];
                    select_phase=0x7cu;select_species[0]=b;select_species[1]=c;select_species[2]=a;wrapped=1;
                }
            }
        }while(!wrapped);
    }
    /* The original left/right exits select different species-table slots.
     * Resolve the frontmost authored portrait directly so Fire before any
     * rotation and both exit directions agree with what is on screen. */
    chosen=select_front_species();
    selected_character=chosen;Choice=(s32)chosen;
    mjp_phase=1;wow_mode=1;
    if(pool&&pool_cap>0u){Ob_Addr=pool;Hide_Obj();}
    if(pool&&pool_cap>2u){Ob_Addr=pool+2;Hide_Obj();}
    /* Hold-safe version of the original 50/20-VBL, skippable Pause jobs. */
    evt(0x53474346u,(s32)chosen,0); /* selection GPU crossfade */
    select_animation_pause(50u);
    evt(0x53474649u,(s32)chosen,0); /* fade-in GPU job */
    if(pool&&pool_cap>4u){Ob_Addr=pool+4;No_Scale();}
    Choice=(chosen==PT_HUMAN)?0:(chosen==PT_ALIEN?4:8);
    select_animation_pause(20u);
    evt(0x53454c50u,Choice,0); /* description + bottom prompt resource seam */
    Waitbl();
}
void End_Sele(void)
{
    /* Retail $01EEE6..$01EF54: select SFX, bottom prompt, wait for A/B/C,
     * then start FX fade $FA. Sound/font/blitter execution remain backends. */
    evt(0x53454c53u,0x221b8,-1);
    evt(0x53454c54u,0x2b74e,0);
    /* Selection can leave A/B/C held. PRESS FIRE is a separate gate after
     * the authored loading/briefing stages: require release first. */
    Wait_Up();
    for(;;){One_Tick();mjppad();if(mjp_pad&0x22002000u)break;}
    /* The original FX fade command is executable audio control, not just a
     * display event. ScreenOff() waits for it to complete before stopping
     * character-selection music and entering the gameplay presentation. */
    FXFadeDo(0xfa);
}

/* MJP local $01E170.  The retail loop executes One_Tick, increments Pad_Wait,
 * and returns only when Pad_Wait is strictly greater than the programmed
 * count.  A nominal 300 wait therefore consumes 301 ticks.  Skip polling is
 * mode-dependent in the original: Intro and Simulation Terminated always
 * accept A/B/C, while Escape only does so after its final BEE0 transition. */
static int mjp_timed_wait(s32 count)
{
    mjp_pad_wait=0;
    for(;;){
        One_Tick();
        ++mjp_pad_wait;
        if(mjp_pad_wait>count)return 0;
        if(current_mjp_mode==AVP_MJP_INTRO ||
           current_mjp_mode==AVP_MJP_SIM_TERMINATED ||
           (current_mjp_mode==AVP_MJP_ESCAPE && escape_skip_enabled)){
            mjppad();
            if(mjp_pad&0x22002000u){mjp_pad_wait=-1;return -1;}
        }
    }
}
static int mjp_wait_300(void){return mjp_timed_wait(300);}
extern volatile u32 one_second_ticks;
static void mjp_wait_exact(unsigned ticks){while(ticks--)One_Tick();}
/* Original Do_Escap uses 70 $01E15A VBlank waits between consecutive
 * spoken cues, or a 30+40-field hold/slide. Those are 70/60 seconds on
 * a 60-Hz Jaguar. The user requests literal 1.000-second spoken intervals
 * in the PC port, so compensate at this ONE presentation seam, without
 * changing the original 68000 ROM or generic MJP tick clock.
 * The current CROSS host reconstruction costs 8+16*2 = 40 fields. */
static unsigned mjp_escape_cue_interval(void)
{
    return one_second_ticks==50u?50u:60u;
}
static int mjp_escape_before_slide_wait(void)
{
    const unsigned slide_fields=8u+16u*2u;
    const unsigned target=mjp_escape_cue_interval();
    /* mjp_timed_wait(N) waits N+1 fields, and then the slide costs 40. */
    return mjp_timed_wait((s32)(target-slide_fields-1u));
}
static void mjp_scene_begin(unsigned tag)
{
    /* Retail common local $01D740: Quick_St, Make_Rel, build the two initial
     * display objects, execute the extra $01D7D2 VBL, then load the GPU helper.
     * Phrase packing remains a backend seam; CPU ordering/timing does not. */
    Quick_St();Make_Rel();evt(0x4d4a5042u,(s32)tag,0); /* MJPB */
    One_Tick();Load_GPU();
}
static void mjp_scene_end(unsigned tag)
{
    /* Shared $01DB62 cleanup: clear the back buffer, then Clear_Bu. */
    evt(0x4d4a5045u,(s32)tag,0); /* MJPE */
    mjp_text_x0=0u;
    Clear();Clear_Bu();
}
static int mjp_text_id(u32 text_id)
{
    /* Authored text bytes stay resource-side; the retail pointer is retained
     * as the stable identity of the exact string selected by the 68000. */
    const AvpRuntimeOps *o=avp_runtime_ops();
    evt(0x4d4a5054u,(s32)text_id,0); /* MJPT */
    if(o->frontend_text){New_Text(o->frontend_text(o->user,text_id));Show_Tex();}
    else text_abort=0;
    return text_abort;
}
static void mjp_audio_id(u32 sound_id,s32 command)
{
    /* $009C6C performs the actual sound/backend operation. */
    evt(0x4d4a5041u,(s32)sound_id,command); /* MJPA */
}
/* Retail page-slide words: $4BED4 direction (0, +1, -1), $4BED8 upper and
 * $4BEDC lower limit for D1.  The picture object's X is $12+4*D1
 * ($01DF6A/$01DFC2, $01E07E/$01E088, $01E12A/$01E134). */
static s32 mjp_slide_dir,mjp_slide_max,mjp_slide_min;
static s32 mjp_slide_x(s32 n)
{
    s32 d1;
    if(mjp_slide_dir==1)d1=n;                 /* $01DF96 / $01E0C4: unclamped */
    else if(mjp_slide_dir==-1){               /* $01DF9C / $01E0C8 */
        d1=24-n;
        if(!(d1<mjp_slide_max))d1=mjp_slide_max;   /* CMP $4BED8, BLT */
        if(!(d1>mjp_slide_min))d1=mjp_slide_min;   /* CMP $4BEDC, BGT */
    }else d1=10;                              /* $01DF90 / $01E0BE */
    return 0x12+4*d1;
}
static void mjp_transition(unsigned from,unsigned to)
{
    /* Retail local $01DF2C.  First epoch: D5=0..7 builds the picture object
     * at the slide position, then waits one VBL ($01E15A).  Second epoch:
     * the CROSS GPU program publishes progress $7FB4=1..16, each packed
     * RGB16 blend stage ending on a VBlank acknowledgment ($7FA8, R237),
     * while the 68000 rebuilds the object at D1=$7FB4+8; completion
     * ($7F80=2) follows the stage-16 acknowledgment.  Then $01E152 NEG
     * $4BED4.  The ROM proves >=1 VBL per stage; the BigPEmu reference
     * (2026-10-30 capture, intro pages 1 and 2) holds every stage for
     * exactly two VBLs: blending 144 rows exceeds one frame. */
    evt(0x4d4a5050u,(s32)from,(s32)to); /* MJPP */
    for(s32 d5=0;d5<8;++d5){
        evt(0x4d4a5058u,mjp_slide_x(d5),0); /* MJPX: picture X, CROSS stage */
        One_Tick();
    }
    for(s32 stage=1;stage<=16;++stage){
        evt(0x4d4a5058u,mjp_slide_x(stage+8),stage);
        One_Tick();One_Tick();       /* two VBLs per GPU stage */
    }
    evt(0x4d4a5053u,(s32)from,(s32)to); /* MJPS: GPU status 2 */
    mjp_slide_dir=-mjp_slide_dir;
}
static void mjp_wait_win_transition(unsigned phase)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    /* Do_Win's two OP loops differ from $01DF2C: they call One_Tick on every
     * pass before Change_D and then poll a Jaguar OP status word for value 2.
     * Keep that loop in C and delegate only the hardware status bit. */
    do{
        One_Tick();
        evt(0x4d4a5750u,(s32)phase,0); /* MJWP */
        if(!o->frontend_transition_done)break;
    }while(!o->frontend_transition_done(o->user,phase));
}
static int mjp_page(unsigned from,unsigned to,u32 text_id)
{
    /* Retail local $01E1F2: transition, clear, show text, then $01E166. */
    mjp_transition(from,to);Clear();if(mjp_text_id(text_id)<0 || text_abort)return -1;
    return mjp_wait_300();
}

void Do_Intro(void)
{
    static const struct {u8 from,to;u32 text;} pages[]={
        {0,1,0x0002AA89u},{1,2,0x0002AB0Fu},{2,3,0x0002AB83u},
        {3,4,0x0002AC14u},{4,5,0x0002ACA6u},{5,6,0x0002AD25u},
        {6,0,0x0002ADA3u}
    };
    mjp_scene_begin(0x494e5452u); /* INTR */
    mjp_text_x0=0x18u;                          /* $01D84C */
    /* $01D846 clears Buffer1, Buffer2 and Back_Buf before its first transition. */
    Clear();Clear();Clear();
    mjp_slide_min=0;mjp_slide_dir=0;           /* $01D89A, $01D8A4 */
    mjp_transition(AVP_MJP_PAGE_BLANK,0);       /* D6=$4BE18, D7=page 0 */
    mjp_slide_dir=-1;                           /* $01D8B4 */
    mjp_text_id(0x0002AA18u);
    if(text_abort || mjp_wait_300()<0){mjp_scene_end(0x494e5452u);return;}
    for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i){
        if(i==0u)mjp_slide_max=10;              /* $01D904 */
        if(i==1u)mjp_slide_max=0x63;            /* $01D930 */
        if(i==6u)mjp_slide_min=10;              /* $01D9E4 */
        if(mjp_page(pages[i].from,pages[i].to,pages[i].text)<0){
            mjp_scene_end(0x494e5452u);return;
        }
    }
    /* Normal Intro completion takes the shared $01DB40 transition before the
     * $01DB62 cleanup; an early skip jumps directly to cleanup above. */
    mjp_slide_dir=0;                            /* $01DB40 */
    mjp_transition(0,AVP_MJP_PAGE_BLANK);       /* D6=page 0, D7=$4BE18 */
    mjp_scene_end(0x494e5452u);
}
/* Retail $01F7E8: one VBL ($01E15A), read the pad; PAUSE (bit 28) freezes
 * the scroll until it is released, pressed and released again.  Returns
 * nonzero when A/B/C is down. */
static int credits_wait(void)
{
    One_Tick();mjppad();
    if(mjp_pad&(1u<<PAUSE)){
        do{One_Tick();mjppad();}while(mjp_pad&(1u<<PAUSE));
        do{One_Tick();mjppad();}while(!(mjp_pad&(1u<<PAUSE)));
        do{One_Tick();mjppad();}while(mjp_pad&(1u<<PAUSE));
    }
    return (mjp_pad&0x22002000u)!=0u;
}
/* Retail Do_Title CREDITS, $01F5BC..$01F7D0.  FILES 238 is a pre-rendered
 * 320x1373 4-bpp anti-aliased bitmap shown through the 16-step grey CLUT
 * written to $F00400 (index 0 transparent over the dimmed title).  A 280-line
 * ($118) object rises from Y+300 ($12C) to Y+1, one line per VBL; then its
 * data pointer advances one 160-byte line per VBL for $55D (1373) lines,
 * past the image into the blitter-cleared tail.  A/B/C exits at any VBL. */
void Do_Credits(void)
{
    s32 y=0x12c,line=0;
    Wait_Up();                                  /* $01F5BC: $01FDFC */
    evt(0x43524247u,0,0);                       /* CRBG */
    evt(0x43525359u,y,0);                       /* CRSY: object Y, data line */
    for(;;){                                    /* $01F6EC */
        if(credits_wait())goto done;
        evt(0x43525359u,y,0);
        if(--y==0)break;
    }
    while(line<0x55d){                          /* $01F726 */
        if(credits_wait())goto done;
        ++line;
        evt(0x43525359u,1,line);
    }
done:
    Wait_Up();                                  /* $01F75A: $01FDFC */
    evt(0x4352454eu,0,0);                       /* CREN */
}

void Do_Fame(void)
{
    /* Retail tests player_type == -1 before touching the score table.  That
     * title-menu branch is a distinct EEPROM-clear/help screen at $01D09C. */
    if(player_type==-1){
        Quick_St();Make_Rel();Clear_Bu();
        mjp_text_id(0x0002A994u);
        Choice=0;
        if(get_choice(AVP_MJP_FAME)){if(Choice==-99)player_type=-99;return;}
        Wait_Up();
        for(;;){
            One_Tick();mjppad();
            if(mjp_pressed(FIRE_A)){player_type=-99;return;}
            if(mjp_pressed(FIRE_B))return;
        }
    }

    /* Normal post-game path: build the front-end lists, unpack the five saved
     * entries, determine the strict insertion point, generate the exact
     * ten-character decimal fields, and render the table through Fame_Tex. */
    Quick_St();Make_Rel();Unpack_F();
    unsigned insert=0;
    while(insert<5u && fame[insert].score>=score)++insert;
    fame_render_table();

    if(insert==5u){
        /* Retail $01C770 selects authored text $2A925 when the score does not
         * enter the table, then waits for A/B/C. */
        mjp_text_id(0x0002A925u);
        Choice=0;
        if(get_choice(AVP_MJP_FAME))return;
        Wait_Up();do{One_Tick();mjppad();}while((mjp_pad&0x22002000u)==0u);
        return;
    }

    /* Shift both score and 23-byte display-record semantics down one slot.
     * The portable struct carries the identity/name/score fields that affect
     * subsequent CPU decisions; glyph storage remains resource/backend data. */
    for(unsigned i=4u;i>insert;--i)fame[i]=fame[i-1u];
    fame[insert].score=score;
    fame[insert].species=player_type==PT_HUMAN?'m':(player_type==PT_ALIEN?'a':'p');
    memset(fame[insert].name,' ',6);fame[insert].name[6]=0;
    Count=(s32)insert;evt(0x46494e53u,score,(s32)insert);
    fame_edit_name(insert);Pack_Fam();Fame_Tex();Write_EE();
}

void Do_Escap(void)
{
    /* Retail $01DB78..$01DF2A.  The first pass is intentionally unskippable.
     * On normal completion BEE0 becomes one and the assembly recursively JSRs
     * its local $01DBDE body; subsequent passes are skippable.  A loop plus an
     * unwind-clear count preserves that control flow without recursive C. */
    unsigned unwind_clears=0;
    escape_skip_enabled=0;
    mjp_slide_dir=0;              /* $01DB82 */
    mjp_scene_begin(0x45534350u); /* ESCP */
    mjp_slide_min=10;mjp_slide_max=10; /* $01DBA6, $01DBB0 */
    mjp_text_x0=0x18u;            /* $01DB92 */
    mjp_escape_text_line=0u;
    mjp_suppress_typing_sound=1u; /* $4BEC8=1: countdown words are silent TYP1B */
    Clear();Clear();              /* Buffer1 / Buffer2 */

    for(;;){
        Clear();                  /* $01DBDE Back_Buf */
        mjp_transition(AVP_MJP_PAGE_BLANK,0); /* D6=$4BE18 -> first escape page */
        Clear();

        /* $01DC28..$01DCEE: the spoken sample starts BEFORE Show_Tex.
         * The original countdown text cells are displayed on successive
         * Y=$A6,$B4,$C2 lines; $4BEC8=1 suppresses ordinary TYP1B here. */
        mjp_audio_id(0x00022848u,0x2600);mjp_text_id(0x0002ADDAu);
        ++mjp_escape_text_line;
        mjp_wait_exact(mjp_escape_cue_interval());
        mjp_audio_id(0x00022898u,0x2600);mjp_text_id(0x0002AE01u);
        ++mjp_escape_text_line;
        mjp_wait_exact(mjp_escape_cue_interval());
        mjp_audio_id(0x000228E8u,0x2600);mjp_text_id(0x0002AE28u);
        if(mjp_escape_before_slide_wait()<0)break;

        mjp_transition(0,1);Clear();
        mjp_escape_text_line=0u; /* retail resets $4BEB4=$A6 for new page */
        mjp_audio_id(0x00022938u,0x2600);mjp_text_id(0x0002AE4Fu);
        ++mjp_escape_text_line;
        mjp_wait_exact(mjp_escape_cue_interval());
        mjp_audio_id(0x00022988u,0x2600);mjp_text_id(0x0002AE76u);
        if(mjp_escape_before_slide_wait()<0)break;

        mjp_transition(1,2);Clear();
        mjp_escape_text_line=0u;
        mjp_audio_id(0x00021718u,-1);
        mjp_text_id(0x0002AE9Du);
        if(mjp_wait_300()<0)break;
        if(mjp_wait_300()<0)break;
        mjp_suppress_typing_sound=0u; /* $01E226: report text enables TYP1B */
        if(mjp_page(2,3,0x0002AEC4u)<0)break;
        if(mjp_page(3,4,0x0002AF39u)<0)break;
        if(mjp_wait_300()<0)break;
        if(mjp_wait_300()<0)break;
        if(mjp_wait_300()<0)break;

        Clear();mjp_transition(4,AVP_MJP_PAGE_BLANK); /* $01DEFC: page 4 -> $4BE18 */
        mjp_suppress_typing_sound=1u;
        mjp_escape_text_line=0u;
        escape_skip_enabled=1;   /* retail $01DF14 */
        ++unwind_clears;         /* one $01DF24 clear after this nested call */
    }

    while(unwind_clears--)Clear();
    mjp_text_x0=0u;
    mjp_escape_text_line=0u;
    mjp_suppress_typing_sound=0u;
    evt(0x4d4a5045u,0x45534350u,0);
}

void Do_Lose(void)
{
    const int terminated=(current_mjp_mode==AVP_MJP_SIM_TERMINATED);
    mjp_scene_begin(0x4c4f5345u); /* LOSE */
    mjp_slide_min=10;mjp_slide_max=10; /* $01DA0C, $01DA16 */
    Clear();Clear();Clear();
    mjp_slide_dir=0;              /* $01DA78 */
    mjp_transition(AVP_MJP_PAGE_BLANK,0);Clear(); /* D6=$4BE18, D7=page 0 */
    /* The retail Do_Lose sets $4BEB0=$18 at $01DAAE before the text walk.
     * In particular the Marine self-destruct report was hosted at X=0. */
    if(!terminated)mjp_text_x0=0x18u;
    mjp_text_id(terminated?0x0002AFC3u:0x0002B036u);
    if(terminated){
        if(mjp_wait_300()<0)goto done;
        if(mjp_wait_300()<0)goto done;
    }else{
        /* Base Explodes: exactly 600 ticks, then no timeout while waiting for
         * any A/B/C press. */
        mjp_wait_exact(600);
        do{One_Tick();mjppad();}while((mjp_pad&0x22002000u)==0u);
    }
done:
    mjp_slide_dir=0;              /* shared $01DB40 */
    mjp_transition(0,AVP_MJP_PAGE_BLANK);mjp_scene_end(0x4c4f5345u);
}

void Do_Win(void)
{
    /* Exact retail routine is $01FED8..$020190.  An earlier recovered-boundary
     * manifest incorrectly truncated it at $01FEFF; direct binary disassembly
     * shows the continuation and final RTS at $020190. */
    evt(0x57494e20u,(s32)player_type,0x4220); /* WIN: display-state $4220 */
    Clear_Bu();Make_Rel();

    /* Build the full-screen win object, load the transition GPU helper and poll
     * the OP completion state while Change_D tracks the backend-updated source. */
    evt(0x4d4a5700u,(s32)current_mjp_mode,0); /* MJW0 */
    One_Tick();                              /* retail $01FF4A */
    Load_GPU();
    mjp_wait_win_transition(0);

    /* Retail then builds the second full-screen object.  Alien win alone adds
     * the authored $2B7D8 text record at left=35, top=177, line advance=23. */
    evt(0x4d4a5701u,(s32)current_mjp_mode,0); /* MJW1 */
    if(current_mjp_mode==AVP_MJP_ALIEN_WIN){
        evt(0x4d4a574cu,35,177);              /* MJWL */
        evt(0x4d4a5741u,23,0);                /* MJWA */
        mjp_text_id(0x0002B7D8u);
        evt(0x46414d45u,0,0);                 /* Fame_Tex presentation */
    }

    /* $02007C: exactly 600 VBLs.  Retail calls mjppad on every one of
     * those ticks even though the wait itself cannot be skipped, then enters
     * a separate unbounded A/B/C wait which also refreshes the pad each VBL. */
    for(unsigned i=0;i<600u;++i){One_Tick();mjppad();}
    do{One_Tick();mjppad();}while((mjp_pad&0x22002000u)==0u);

    /* The two blitter register programs are pure Jaguar presentation work.
     * Preserve their ordering around the second OP transition. */
    Waitbl();evt(0x4d4a5742u,0,0);            /* MJWB phase 0 */
    Waitbl();evt(0x4d4a5742u,1,0);            /* MJWB phase 1 */
    evt(0x4d4a5702u,(s32)current_mjp_mode,0); /* MJW2 */
    mjp_wait_win_transition(1);
}
typedef struct AvpMjpResource {u16 file;u8 role;} AvpMjpResource;
static const AvpMjpResource res_title[]={{208,0},{207,1},{237,2},{236,3},{233,4},{205,5},{232,6},{234,7},{238,8}};
static const AvpMjpResource res_select[]={{235,0},{206,1},{215,2},{214,3},{213,4},{210,5},{209,6}};
static const AvpMjpResource res_intro[]={{230,0},{211,1},{212,2},{224,3},{219,4},{225,5},{220,6},{235,7}};
static const AvpMjpResource res_fame_base[]={{234,0},{235,1}};
static const AvpMjpResource res_credits[]={{238,0}};
static const AvpMjpResource res_escape[]={{216,0},{222,1},{217,2},{223,3},{218,4},{235,5}};
static const AvpMjpResource res_predwin[]={{226,0},{234,1}};
static const AvpMjpResource res_alienwin[]={{221,0},{234,1}};
static const AvpMjpResource res_terminated[]={{230,0},{235,1}};
static const AvpMjpResource res_explodes[]={{231,0},{235,1}};
static void load_mode_resources(AvpMjpMode m){
    const AvpMjpResource *r=NULL;unsigned n=0;const AvpRuntimeOps*o=avp_runtime_ops();
    switch(m){
    case AVP_MJP_TITLE:r=res_title;n=sizeof(res_title)/sizeof(*r);break;
    case AVP_MJP_SELECT:r=res_select;n=sizeof(res_select)/sizeof(*r);break;
    case AVP_MJP_INTRO:r=res_intro;n=sizeof(res_intro)/sizeof(*r);break;
    case AVP_MJP_CREDITS:r=res_credits;n=sizeof(res_credits)/sizeof(*r);break;
    case AVP_MJP_FAME:r=res_fame_base;n=sizeof(res_fame_base)/sizeof(*r);break;
    case AVP_MJP_ESCAPE:r=res_escape;n=sizeof(res_escape)/sizeof(*r);break;
    case AVP_MJP_PRED_WIN:r=res_predwin;n=sizeof(res_predwin)/sizeof(*r);break;
    case AVP_MJP_ALIEN_WIN:r=res_alienwin;n=sizeof(res_alienwin)/sizeof(*r);break;
    case AVP_MJP_SIM_TERMINATED:r=res_terminated;n=sizeof(res_terminated)/sizeof(*r);break;
    case AVP_MJP_BASE_EXPLODES:r=res_explodes;n=sizeof(res_explodes)/sizeof(*r);break;
    }
    if(o->frontend_load_resource){
        for(unsigned i=0;i<n;i++)o->frontend_load_resource(o->user,(unsigned)m,r[i].file,r[i].role);
        if(m==AVP_MJP_FAME){
            unsigned file=0;
            if(player_type==PT_HUMAN)file=229u;
            else if(player_type==PT_ALIEN)file=228u;
            else if(player_type==PT_PREDATOR)file=227u;
            if(file)o->frontend_load_resource(o->user,(unsigned)m,file,2u);
        }
    }
}
void avp_mjp_prepare_title(void){load_mode_resources(AVP_MJP_TITLE);}
void MTEST(AvpMjpMode m){
    current_mjp_mode=m;
    /* Exact recovered retail mode map and FILES resource identity.  Decoding and
     * pixel storage are resource/backend work; the 68000-side selection of the
     * assets and controller remains explicit here. */
    load_mode_resources(m);
#ifdef AVP_PC_HOST
    /* Keep the original MJP text SFX commands unchanged; restrict only
     * hosted overlap of TYP1B for Intro, matching the one-lane escape
     * report's user-confirmed auditory cadence. MTEST owns cleanup for
     * both ordinary scene completion and an A/B/C interruption. */
    if(m==AVP_MJP_INTRO)avp_music_set_intro_typ1b_single_lane(1);
#endif
    switch(m){
    case AVP_MJP_TITLE:Do_Title();break;
    case AVP_MJP_SELECT:
        /* Retail mode 1: $01C08C install Lister3 VBlank list writer;
         * $01C096 WaitFXFa; $01C0A4 StartMus($2046C); $01C0B0 FXFadeUp($FA);
         * $01C0B6 JSR $1E868 = Do_Selec; $01C0BC WaitFXFa.  The carousel
         * runs WHILE the VBlank-chained master fade rises from 0 (about 132
         * VBlanks), so the first portrait is shown with the score still
         * quiet.  R272-R276 waited for the whole fade before Do_Selec. */
        WaitFXFa();
        StartMus(AVP_MUSIC_CHARACTER_SELECT);
        FXFadeUp(0xfau);
        Do_Selec();
        WaitFXFa();
        break;
    case AVP_MJP_INTRO:Do_Intro();break;case AVP_MJP_FAME:Do_Fame();break;
    case AVP_MJP_CREDITS:Do_Credits();break;
    case AVP_MJP_ESCAPE:Do_Escap();break;
    case AVP_MJP_PRED_WIN:case AVP_MJP_ALIEN_WIN:Do_Win();break;
    case AVP_MJP_SIM_TERMINATED:case AVP_MJP_BASE_EXPLODES:Do_Lose();break;
    }
#ifdef AVP_PC_HOST
    if(m==AVP_MJP_INTRO)avp_music_set_intro_typ1b_single_lane(0);
#endif
}
