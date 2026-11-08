#include "mjp.h"
#include "avp_runtime.h"
#include "avp_types.h"
#include "joypad.h"
#include "eeprom.h"
#include "player.h"
#include "music.h"
#include <assert.h>
#include <string.h>

void Do_Credits(void); /* hosted credits presentation, tested directly */

volatile u32 joy_edge,joy2_edge,joy_cur,joy2_cur;
volatile s8 joy_special_mode,reset_enabled,pad_flag2,pad_flag3;
volatile u16 reset_wait;
volatile u32 reset_time,frame_counter;
void (*x_read)(void);
s16 player_type;
s32 score;
u8 cartcopy[AVP_EE_SIZE];
static unsigned ee_writes;
void Write_EE(void){++ee_writes;}

static unsigned vblanks,blits,loads;
static u16 files_seen[16];
static unsigned modes_seen[16],roles_seen[16];
static s32 choice_reply;
static char fame_draw[5][2][16];

static int scripted_pad;
static unsigned selection_reads,selection_moves,selection_direction;
static s32 last_visible_species=-1;
static unsigned cancel_events,credit_events,credit_begin_events,credit_end_events,credit_open_events,credit_close_events,selection_sound_events,menu_events;
static unsigned select_cross_at,select_fade_at,select_brief_at,select_cross_events;
static unsigned fade_start_count;
static s32 last_fade_step;
void FXFadeDo(s32 step){++fade_start_count;last_fade_step=step;}
static unsigned selection_fade_waits,selection_fade_up_count,selection_fade_vblanks;
static int selection_fade_pending,selection_fade_order_expected;
static unsigned selection_music_starts;
static unsigned selection_music_id;
static int selection_music_running;
void StartMus(unsigned id){
    ++selection_music_starts;
    selection_music_id=id;
    selection_music_running=1;
    /* Original START precedes FXFadeUp, so it must not be deferred to SOBJ. */
    assert(!selection_fade_pending);
}
void WaitFXFa(void);
void FXFadeUp(s32 step){
    WaitFXFa();
    ++selection_fade_up_count;
    assert(step==0xfau);
    assert(selection_music_running);
    selection_fade_pending=1;
}
void WaitFXFa(void){
    ++selection_fade_waits;
    if(selection_fade_pending){
        selection_fade_pending=0;
        selection_fade_vblanks+=132u;
        vblanks+=132u;
    }
}
/* Record the source-authored attract program, without replacing its graphics. */
static u32 intro_text_ids[8];
static unsigned intro_from[9],intro_to[9],intro_finishes;
static unsigned intro_text_count,intro_transition_count;
/* R280: every MJPX (picture X, CROSS stage) the CPU publishes. */
static s32 slide_x[512],slide_stage[512];
static unsigned slide_count;
static const char *empty_retail_text(void *user,u32 text_id)
{
    static const char empty[2]={0,0};
    (void)user;(void)text_id;
    return empty;
}
static void frontend_event(void *u,unsigned event,s32 a,s32 b)
{
    (void)u;
    if(event==0x4d4a5054u && intro_text_count<8u)
        intro_text_ids[intro_text_count++]=(u32)a; /* MJPT */
    if(event==0x4d4a5050u && intro_transition_count<9u){ /* MJPP */
        intro_from[intro_transition_count]=(unsigned)a;
        intro_to[intro_transition_count]=(unsigned)b;
        ++intro_transition_count;
    }
    if(event==0x4d4a5053u)++intro_finishes; /* MJPS */
    if(event==0x4d4a5058u && slide_count<512u){ /* MJPX */
        slide_x[slide_count]=a;slide_stage[slide_count]=b;++slide_count;
    }
    if(event==0x544d454eu)++menu_events; /* TMEN: enter actual menu plane */
    if(selection_fade_order_expected && event==0x534f424au && (((u32)a>>16)&0xffffu)==4u){
        /* R277: retail $01C0B0 FXFadeUp, $01C0B6 Do_Selec, $01C0BC WaitFXFa:
         * the first portrait list is published while the fade is RUNNING. */
        assert(selection_fade_pending==1);
        assert(selection_fade_up_count==1u);
        assert(selection_music_running);
        assert(selection_music_starts==1u); /* no SOBJ music restart */
    }
    if(event==0x53454c56u)last_visible_species=a;
    if(event==0x53565854u)++cancel_events;
    if(event==0x43525047u)++credit_events; /* old paginated plane must stay unused */
    if(event==0x43524247u)++credit_begin_events; /* CRBG */
    if(event==0x4352454eu)++credit_end_events; /* CREN */
    if(event==0x43524544u)++credit_open_events;
    if(event==0x43524558u)++credit_close_events;
    if(event==0x53454c53u){assert(a==0x221b8);++selection_sound_events;}
    if(event==0x53474346u){select_cross_at=vblanks;++select_cross_events;}
    if(event==0x53474649u)select_fade_at=vblanks;
    if(event==0x53454c50u)select_brief_at=vblanks;
}
void readpad(void){
    if(scripted_pad==2){
        /* Credits is modal INSIDE the Jaguar title selector: 3 Down,
         * Fire, release, C to exit, release, then new Fire on title.
         * It must never return a synthetic Choice=5 to MAIN. */
        unsigned t=vblanks;
        joy_cur=(t==3u||t==5u||t==7u)?~(1u<<JOY_DOWN):
                (((t>=9u&&t<=10u)||t==22u)?~(1u<<FIRE_A):
                 ((t>=16u&&t<=17u)?~(1u<<FIRE_C):0xffffffffu));
    }else if(scripted_pad==3){
        unsigned i=selection_reads++;
        if(i<2u*selection_moves && !(i&1u))
            joy_cur=~(1u<<(selection_direction?JOY_RIGHT:JOY_LEFT));
        else if(i>=2u*selection_moves)
            joy_cur=~(1u<<FIRE_A);
        else joy_cur=0xffffffffu;
    }else if(scripted_pad==7){
        /* A second, distinct Fire press while CROSS is in progress retains
         * the original Pause() skip path. The selection Fire is consumed. */
        unsigned elapsed=vblanks-select_cross_at;
        joy_cur=(!select_cross_events || elapsed<5u || elapsed>=10u)
            ? ~(1u<<FIRE_A):0xffffffffu;
    }else if(scripted_pad==6){
        /* Hold selection Fire, release, then press freshly after the prompt. */
        joy_cur=(vblanks<=2u||vblanks>=5u)?~(1u<<FIRE_A):0xffffffffu;
    }else if(scripted_pad==5){
        joy_cur=(vblanks==5u)?~(1u<<FIRE_B):0xffffffffu;
    }else if(scripted_pad==4){
        /* TITLE Down/Fire -> RESTORE, three Down -> CANCEL, keep C held
         * into the first title tick, then Down/Down/Fire -> HIGH SCORES. */
        unsigned t=vblanks;
        joy_cur=(t==3u||t==8u||t==10u||t==12u||t==17u||t==19u)
            ? ~(1u<<JOY_DOWN)
            : ((t==5u||t==21u)?~(1u<<FIRE_A)
               : ((t==14u||t==15u)?~(1u<<FIRE_C):0xffffffffu));
    }else if(!scripted_pad)joy_cur=0xffffffffu;
    joy_edge=joy_cur;
}
void VSync(void){ ++vblanks; }

static void wait_blit(void *u){(void)u;++blits;}
static void load_resource(void *u,unsigned mode,unsigned file,unsigned role)
{
    (void)u;
    assert(loads<16u);
    modes_seen[loads]=mode;files_seen[loads]=(u16)file;roles_seen[loads]=role;++loads;
}
static int choose(void *u,unsigned mode,s32 *choice)
{
    (void)u;(void)mode;*choice=choice_reply;return 1;
}


static unsigned text_min_x=~0u;
static const char *one_glyph_text(void *user,u32 text_id)
{
    static const char t[3]={'A',0,0};
    (void)user;(void)text_id;
    return t;
}
static void draw_text(void *u,unsigned line,const char *text,unsigned x)
{
    (void)u;
    if(x<text_min_x)text_min_x=x;
    if(line<5u){unsigned col=(x>=100u)?1u:0u;strncpy(fame_draw[line][col],text,15u);fame_draw[line][col][15]=0;}
}

static int fame_name(void *u,char out[7])
{
    (void)u;memcpy(out,"ABCDEF",7);return 1;
}

int main(void)
{
    AvpRuntimeOps ops;
    memset(&ops,0,sizeof(ops));
    ops.wait_blitter=wait_blit;
    ops.frontend_load_resource=load_resource;
    ops.frontend_choice=choose;
    ops.draw_text=draw_text;
    ops.frontend_event=frontend_event;
    avp_runtime_bind(&ops);

    choice_reply=4;
    MTEST(AVP_MJP_TITLE);
    assert(loads==9u);
    { const u16 want[9]={208,207,237,236,233,205,232,234,238};
      for(unsigned i=0;i<9u;i++){assert(files_seen[i]==want[i]);assert(modes_seen[i]==AVP_MJP_TITLE);assert(roles_seen[i]==i);} }
    assert(Choice==4);
    assert(blits==3u); /* Clear_Bu clears back/front/alternate retail buffers. */
    assert(vblanks==1u && menu_events==1u);

    /* The idle timeout remains Choice=4 (attract). Explicit CREDITS must
     * keep its presentation entirely inside Do_Title, then continue the
     * original selector without restarting MAIN or selecting NEW GAME with
     * the held C input. The host credits artwork/timing remain unverified. */
    ops.frontend_choice=NULL;ops.frontend_event=frontend_event;avp_runtime_bind(&ops);
    scripted_pad=2;vblanks=0;loads=0;credit_events=0;
    credit_open_events=credit_close_events=0;
    Do_Title();
    assert(Choice==0);
    assert(vblanks==22u);
    assert(credit_open_events==1u && credit_close_events==1u);
    assert(credit_events==0u && credit_begin_events==1u && credit_end_events==1u);
    assert(loads==2u);
    assert(files_seen[0]==238u&&modes_seen[0]==AVP_MJP_CREDITS); /* R280: credits bitmap */
    assert(files_seen[1]==208u&&modes_seen[1]==AVP_MJP_TITLE);
    /* Direct host credits presentation still exists as an explicitly
     * provisional frontend seam; it must not invoke the idle intro. */
    scripted_pad=5;vblanks=0;credit_events=0;loads=0;
    MTEST(AVP_MJP_CREDITS);
    assert(loads==1u && files_seen[0]==238u && modes_seen[0]==AVP_MJP_CREDITS);
    /* On an untouched Jaguar pad the credits scroll themselves to the end;
       they never require page changes, so the title selector cannot hang. */
    scripted_pad=0;joy_cur=0xffffffffu;vblanks=0;
    credit_begin_events=credit_end_events=credit_events=0;
    Do_Credits();
    /* R280 retail $01F5BC: Wait_Up, 300 rising VBLs, 1373 data-line VBLs,
     * final Wait_Up ($01F75A). */
    assert(vblanks==1u+300u+1373u+1u);
    assert(credit_begin_events==1u && credit_end_events==1u && credit_events==0u);

    assert(credit_events==0u && credit_begin_events==1u && credit_end_events==1u);
    /* Backing out of RESTORE must return to TITLE, not select a new game
     * through the still-held CANCEL/FIRE button. The subsequent separate
     * HIGH SCORES choice proves the menu stayed active until new input. */
    ops.frontend_event=frontend_event;avp_runtime_bind(&ops);
    scripted_pad=4;vblanks=0;cancel_events=0;
    Do_Title();
    assert(cancel_events==1u);
    assert(Choice==2);
    assert(vblanks==21u);
    scripted_pad=0;joy_cur=0xffffffffu;
    ops.frontend_choice=choose;avp_runtime_bind(&ops);

    /* MAIN.S hidden EEPROM-clear contract: player_type is primed to -1 and
     * MJP returns -99 when the Hall-of-Fame clear action is chosen. */
    loads=blits=vblanks=0;
    player_type=-1;choice_reply=-99;
    MTEST(AVP_MJP_FAME);
    assert(player_type==-99);
    assert(loads==2u);
    { const u16 want[2]={234,235};
      for(unsigned i=0;i<2u;i++)assert(files_seen[i]==want[i]); }
    assert(blits==3u && vblanks==1u);
    /* Exact retail Hall-of-Fame default packing. */
    memset(cartcopy,0,sizeof(cartcopy));
    Init_Hig();
    memset(fame_draw,0,sizeof(fame_draw));
    player_type=PT_HUMAN;score=0;choice_reply=0;Do_Fame();
    /* Retail $01CC70 produces fixed ten-character score fields. */
    assert(strcmp(fame_draw[0][1],"   1220000")==0);
    assert(strcmp(fame_draw[1][1],"    800000")==0);
    assert(strcmp(fame_draw[2][1],"    600000")==0);
    { const u32 want[10]={0x188513ffu,0x00129da0u,0x40d1e3ffu,0x000c3500u,
                          0x9f48bd64u,0x000927c0u,0x5206125fu,0x00061a80u,
                          0x1447351fu,0x00030d40u};
      const u8 *p=cartcopy+60;
      for(unsigned i=0;i<10u;i++,p+=4){u32 v=((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];assert(v==want[i]);}
    }
    Unpack_F();
    Pack_Fam(); /* round trip must remain byte exact */
    { const u8 *p=cartcopy+60; assert(p[0]==0x18 && p[1]==0x85 && p[2]==0x13 && p[3]==0xff); }

    /* A post-game score insertion is owned by MJP C, not by the host UI.
     * The host supplies only the six entered characters. */
    ops.frontend_fame_name=fame_name;avp_runtime_bind(&ops);
    player_type=PT_PREDATOR;score=2000000;
    MTEST(AVP_MJP_FAME);
    { const u8 *p=cartcopy+60;
      u32 ident=((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];
      u32 sc=((u32)p[4]<<24)|((u32)p[5]<<16)|((u32)p[6]<<8)|p[7];
      assert(ident==0x80110c85u);
      assert(sc==2000000u);
    }

    /* Mode 3 loads exactly one species backdrop, and none for player_type=-1. */
    loads=0;player_type=PT_HUMAN;score=0;choice_reply=0;MTEST(AVP_MJP_FAME);
    assert(loads==3u && files_seen[0]==234 && files_seen[1]==235 && files_seen[2]==229);
    loads=0;player_type=PT_ALIEN;MTEST(AVP_MJP_FAME);
    assert(loads==3u && files_seen[2]==228);
    loads=0;player_type=PT_PREDATOR;MTEST(AVP_MJP_FAME);
    assert(loads==3u && files_seen[2]==227);

    /* Strict comparison: tying the fifth score does not insert or write EEPROM. */
    Init_Hig();ee_writes=0;player_type=PT_HUMAN;score=200000;
    Do_Fame();assert(ee_writes==0u);

    /* A qualifying score shifts the table, starts the six-character name at A,
     * and commits/Pack_Fam/Write_EE on A/B/C. */
    Init_Hig();ee_writes=0;player_type=PT_HUMAN;score=1300000;
    joy_cur=~(1u<<FIRE_A);scripted_pad=1; /* editor sees confirm */
    Do_Fame();
    assert(ee_writes==1u);
    { const u8 *p=cartcopy+60;
      u32 packed=((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];
      u32 sc=((u32)p[4]<<24)|((u32)p[5]<<16)|((u32)p[6]<<8)|p[7];
      assert((packed>>30)==0u);assert(sc==1300000u);
    }
    scripted_pad=0;joy_cur=0xffffffffu;

    /* Make_New and New_Make are both in-place in retail: the caller selects
     * Ob_Addr explicitly; neither routine owns an allocator. */
    {
        AvpMjpObject objs[20];
        memset(objs,0,sizeof(objs));
        avp_mjp_bind_objects(objs,20);
        Ob_Addr=&objs[0];
        Ob_Data=0x1000;Ob_Pos_X=100;Ob_Pos_Y=50;Ob_Height=20;Ob_Width=40;Ob_Trans=0;
        AvpMjpObject *a=Make_New();
        assert(a==&objs[0]);
        Ob_Scale=12;
        AvpMjpObject *b=New_Make();
        assert(b==a);
        assert(b->scaled==1u && b->scale_x==12 && b->scale_y==12);
        assert(b->x==92 && b->y==46);
        Ob_Addr=&objs[1];
        assert(Make_New()==&objs[1]);

        memset(objs,0,sizeof(objs));
        Make_Rel();
        for(unsigned i=0;i<9u;i++)assert(objs[1u+i*2u].width==16);
        for(unsigned i=0;i<9u;i++)assert(objs[i*2u].width==0);
    }

    /* R254: 68000 New_Make subtraction and position updates are longword
     * modulo-2^32, including non-gameplay signed-limit inputs. */
    {
        static const s32 scales[]={0,1,7,8,9,12,16,31,0x7fffffff,(-0x7fffffff-1),-1,-12};
        static const s32 xs[]={0,100,-100,0x7fffffff,(-0x7fffffff-1)};
        static const s32 ys[]={0,50,-50,0x7fffffff,(-0x7fffffff-1)};
        AvpMjpObject obj;
        for(unsigned i=0;i<sizeof(scales)/sizeof(scales[0]);i++)
            for(unsigned j=0;j<sizeof(xs)/sizeof(xs[0]);j++)
                for(unsigned k=0;k<sizeof(ys)/sizeof(ys[0]);k++){
                    u32 d=(u32)scales[i]-8u;
                    u32 expected_x=(u32)xs[j]-(d<<1);
                    u32 expected_y=(u32)ys[k]-d;
                    memset(&obj,0,sizeof(obj));
                    avp_mjp_bind_objects(&obj,1);
                    Ob_Addr=&obj;
                    Ob_Data=0x1000u;Ob_Pos_X=xs[j];Ob_Pos_Y=ys[k];
                    Ob_Width=40;Ob_Height=20;Ob_Trans=0;
                    assert(Make_New()==&obj);
                    Ob_Scale=scales[i];
                    assert(New_Make()==&obj);
                    assert((u32)obj.x==expected_x && (u32)obj.y==expected_y);
                    assert(obj.scale_x==scales[i] && obj.scale_y==scales[i]);
                }
    }

    /* RDB.DTA's eight original transmission strings and retail $01D846
     * page transitions are already reconstructed. Guard their exact order,
     * source-selected page indices and CPU duration independently of the
     * still-incomplete Jaguar Object Processor drawing path. An empty text
     * fixture avoids introducing new artificial per-glyph timing. */
    {
        static const u32 ids[8]={0x2AA18u,0x2AA89u,0x2AB0Fu,0x2AB83u,
            0x2AC14u,0x2ACA6u,0x2AD25u,0x2ADA3u};
        /* R280: retail opens from the cleared $4BE18 buffer ($01D880) and
         * $01DB40 closes onto it; neither is a FILES page-0 self-cross. */
        static const unsigned from[9]={AVP_MJP_PAGE_BLANK,0,1,2,3,4,5,6,0};
        static const unsigned to[9]  ={0,1,2,3,4,5,6,0,AVP_MJP_PAGE_BLANK};
        unsigned start;
        ops.frontend_text=empty_retail_text;
        ops.frontend_event=frontend_event;
        avp_runtime_bind(&ops);
        scripted_pad=0;joy_cur=0xffffffffu;
        intro_text_count=intro_transition_count=intro_finishes=0;
        slide_count=0u;
        vblanks=loads=0;start=vblanks;
        MTEST(AVP_MJP_INTRO);
        /* R280: retail $01DF2C picture X = $12+4*D1.  Opening and $01DB40
         * close: $4BED4=0, D1=10.  Page 1: $4BED4=-1, upper 10.  Pages
         * then alternate +1 (D1=D5, then $7FB4+8) and -1 (24-D5, then
         * 24-($7FB4+8)); the last page has lower limit 10. */
        assert(slide_count==9u*24u);
        for(unsigned t=0u;t<9u;t++)for(unsigned k=0u;k<24u;k++){
            const unsigned n=t*24u+k;
            const int stage=k<8u?0:(int)k-7;      /* $7FB4 = 1..16 */
            const int v=k<8u?(int)k:stage+8;      /* D5, or $7FB4+8 */
            int d1;
            if(t==0u||t==8u)d1=10;
            else if(t==1u)d1=(24-v)<10?(24-v):10;
            else if(t==7u)d1=(24-v)>10?(24-v):10;
            else if(t&1u)d1=24-v;                 /* pages 3,5: 114 -> 18 */
            else d1=v;                            /* pages 2,4,6: 18 -> 114 */
            assert(slide_stage[n]==stage);
            assert(slide_x[n]==0x12+4*d1);
        }
        assert(slide_x[24u+23u]==18 && slide_x[48u]==18 && slide_x[48u+23u]==114);
        assert(slide_x[7u*24u+23u]==58);
        assert(loads==8u);
        assert(intro_text_count==8u && intro_transition_count==9u && intro_finishes==9u);
        for(unsigned i=0u;i<8u;i++)assert(intro_text_ids[i]==ids[i]);
        for(unsigned i=0u;i<9u;i++){
            assert(intro_from[i]==from[i]);assert(intro_to[i]==to[i]);
        }
        /* Quick_St and scene entry: two VBL, 9 $01DF2C transitions of
         * 8 slide VBLs + 16 CROSS stages of two VBLs each (BigPEmu
         * reference), eight (300+1)-tick story waits. */
        assert(vblanks-start==2u+9u*40u+8u*301u);
        ops.frontend_text=NULL;
        avp_runtime_bind(&ops);
    }

    /* Recovered $01E166 semantics: Intro and mode 7 can be skipped on the
     * first timed-wait tick.  Quick_St itself consumes the preceding tick. */
    scripted_pad=1;joy_cur=~(1u<<FIRE_A);vblanks=0;loads=0;
    MTEST(AVP_MJP_INTRO);
    assert(loads==8u);
    assert(vblanks==2u+40u+1u); /* entry, opening $01DF2C, skipped wait tick */
    /* R280: Do_Intro $01D84C sets the Show_Tex cursor origin $4BEB0=$18. */
    ops.frontend_text=one_glyph_text;avp_runtime_bind(&ops);
    text_min_x=~0u;
    MTEST(AVP_MJP_INTRO);
    assert(text_min_x==0x18u);
    ops.frontend_text=NULL;avp_runtime_bind(&ops);

    vblanks=0;loads=0;slide_count=0u;
    MTEST(AVP_MJP_SIM_TERMINATED);
    /* R280: Do_Lose $01DA78 sets $4BED4=0, so D1=10: centred X=58. */
    assert(slide_count==48u);
    for(unsigned n=0u;n<slide_count;n++)assert(slide_x[n]==58);
    assert(loads==2u);
    assert(vblanks==2u+40u+1u+40u); /* entry, open, skip tick, $01DB40 close */

    /* Base Explodes does not use the skippable $01E166 path: retail waits
     * exactly 600 ticks, then waits for A/B/C.  With A already held the final
     * input loop consumes one more tick, in addition to Quick_St. */
    vblanks=0;loads=0;
    MTEST(AVP_MJP_BASE_EXPLODES);
    assert(loads==2u);
    assert(vblanks==2u+40u+600u+1u+40u);

    /* Escape keeps BEE0 clear throughout its timed presentation.  A held from
     * entry therefore cannot shorten its 70/30/300-tick sequence. */
    vblanks=0;loads=0;slide_count=0u;
    MTEST(AVP_MJP_ESCAPE);
    assert(slide_count==7u*24u); /* Do_Escap $01DB82: $4BED4=0 */
    for(unsigned n=0u;n<slide_count;n++)assert(slide_x[n]==58);
    assert(loads==6u);
    assert(vblanks==2578u+7u*32u); /* 7 $01DF2C calls, 16 two-VBL CROSS stages each (R280) */

    /* Full Do_Win continues through $020190.  With the hardware completion
     * backend absent, each OP poll resolves after its first CPU tick. */
    vblanks=0;loads=0;blits=0;player_type=PT_ALIEN;
    MTEST(AVP_MJP_ALIEN_WIN);
    assert(loads==2u);
    assert(vblanks==604u); /* setup tick + first OP poll + 600 hold + held-A input tick + second OP poll */
    assert(blits==5u);     /* three-buffer Clear_Bu plus two explicit retail Waitbl phases */

    scripted_pad=0;joy_cur=0xffffffffu;

    /* Selection must launch the species visibly on the frontmost portrait.
     * Starting without rotating selects Alien; one or two left wraps select
     * Marine and Predator. Right wraps use the original direction-dependent
     * slot instead of the broken fixed slot-2 shortcut introduced in R21. */
    {
        AvpMjpObject objects[20];
        struct { unsigned moves,dir; s32 species; } cases[]={
            {0u,0u,PT_ALIEN},
            {1u,0u,PT_HUMAN},
            {2u,0u,PT_PREDATOR},
            {3u,0u,PT_ALIEN},
            {1u,1u,PT_ALIEN},
            {2u,1u,PT_PREDATOR},
            {3u,1u,PT_HUMAN}
        };
        ops.frontend_choice=NULL;
        ops.frontend_event=frontend_event;
        avp_runtime_bind(&ops);
        for(unsigned k=0;k<sizeof(cases)/sizeof(cases[0]);k++){
            memset(objects,0,sizeof(objects));
            avp_mjp_bind_objects(objects,20);
            selection_reads=0;
            selection_moves=cases[k].moves;
            selection_direction=cases[k].dir;
            last_visible_species=-1;
            scripted_pad=3;joy_cur=0xffffffffu;
            select_cross_events=0u;
            Do_Selec();
            /* Holding confirmation throughout both GPU jobs cannot skip
             * their authored 50 + 20 Jaguar video ticks. */
            assert(select_cross_events==1u);
            assert(select_fade_at-select_cross_at==50u);
            assert(select_brief_at-select_fade_at==20u);
            assert(Choice==cases[k].species);
            assert(last_visible_species==cases[k].species);
        }
        /* A fresh press (release at VBL 5, repress at VBL 10) can still
         * explicitly skip the remaining CROSS animation, as retail Pause
         * permits. A continuously held confirmation cannot skip it. */
        memset(objects,0,sizeof(objects));
        avp_mjp_bind_objects(objects,20);
        select_cross_events=0u;
        scripted_pad=7;vblanks=0;joy_cur=0xffffffffu;
        Do_Selec();
        assert(select_cross_events==1u);
        assert(select_fade_at-select_cross_at==10u);
        assert(select_brief_at-select_fade_at==20u);
        scripted_pad=0;joy_cur=0xffffffffu;
    }

    /* Mode 1: wait -> StartMus -> fade-up($FA) -> Do_Selec -> wait (retail
     * $01C096..$01C0BC).  The carousel runs during the master fade. */
    {
        AvpMjpObject objects[20];
        unsigned before_up=selection_fade_up_count;
        unsigned before_waits=selection_fade_waits;
        unsigned before_vbls=selection_fade_vblanks;
        unsigned before_music=selection_music_starts;
        memset(objects,0,sizeof(objects));
        avp_mjp_bind_objects(objects,20u);
        scripted_pad=0;joy_cur=0xffffffffu;
        loads=0u;
        choice_reply=PT_HUMAN;
        selection_fade_order_expected=1;
        ops.frontend_choice=choose;
        avp_runtime_bind(&ops);
        MTEST(AVP_MJP_SELECT);
        selection_fade_order_expected=0;
        assert(selection_music_starts==before_music+1u);
        assert(selection_music_id==AVP_MUSIC_CHARACTER_SELECT);
        assert(selection_fade_up_count==before_up+1u);
        assert(selection_fade_waits==before_waits+3u);
        assert(selection_fade_vblanks==before_vbls+132u);
        assert(Choice==PT_HUMAN);
        ops.frontend_choice=NULL;
        avp_runtime_bind(&ops);
    }

    /* End_Sele must not treat the selection-confirmation hold as PRESS FIRE. */
    scripted_pad=6;vblanks=0;joy_cur=~(1u<<FIRE_A);
    ops.frontend_event=frontend_event;avp_runtime_bind(&ops);
    selection_sound_events=0u;
    End_Sele();
    assert(selection_sound_events==1u);
    assert(vblanks==5u);
    assert(fade_start_count==1u&&last_fade_step==0xfau);
    scripted_pad=0;joy_cur=0xffffffffu;

    avp_runtime_reset_ops();
    return 0;
}
