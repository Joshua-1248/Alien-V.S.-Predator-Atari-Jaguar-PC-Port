#include "music.h"
#include "avp_sfx_ids.h"
#include "avp_runtime.h"
#include <assert.h>
#include <string.h>

static unsigned played,stopped,modified,killed,music_start,music_stop,ambient_start,ambient_kill;
static u32 last_handle;static s32 last_mod;
static void play(void*u,unsigned id){(void)u;(void)id;++played;}
static void stop(void*u,u32 h){(void)u;last_handle=h;++stopped;}
static void mod(void*u,u32 h,s32 v){(void)u;last_handle=h;last_mod=v;++modified;}
static void kill(void*u){(void)u;++killed;}
static void sm(void*u,unsigned id){(void)u;(void)id;++music_start;}
static void xm(void*u){(void)u;++music_stop;}
static void amb(void*u){(void)u;++ambient_start;}
static void kamb(void*u){(void)u;++ambient_kill;}
static unsigned fade_vblanks;
static u32 last_master_volume;
static void fade_vblank(void*u){(void)u;++fade_vblanks;}
static void fade_volume(void*u,u32 v){(void)u;last_master_volume=v;}

static unsigned pause_saved,pause_restored;
static u32 paused_weapon,paused_alarm;
static void save_pause(void *u){(void)u;pause_saved++;}
static void restore_pause(void *u,u32 w,u32 a)
{(void)u;pause_restored++;paused_weapon=w;paused_alarm=a;}
static u32 live[8];
static unsigned last_id,alarm_unloops,unlooped_voice;
static u32 unlooped_handles[32];
static void play_handle(void*u,unsigned id,u32 h,unsigned v)
{(void)u;assert(v<8u);live[v]=h;last_id=id;}
static void unloop(void*u,u32 h)
{(void)u;if(alarm_unloops<32u)unlooped_handles[alarm_unloops]=h;
 ++alarm_unloops;unlooped_voice=(h==live[2])?2u:99u;}
static int is_active(void*u,u32 h)
{(void)u;for(unsigned i=0;i<8u;i++)if(live[i]==h)return 1;return 0;}
static int is_voice_active(void*u,unsigned v)
{(void)u;return v<8u && live[v]!=0u;}

int main(void)
{
    AvpRuntimeOps o;AvpSoundMeta meta[16];u8 arena[64];const char payload[]="abcdef";AvpTransferFile f={payload,sizeof(payload)};
    memset(&o,0,sizeof(o));o.play_sfx=play;o.stop_sfx=stop;o.mod_sfx=mod;o.kill_sounds=kill;o.start_music=sm;o.stop_music=xm;o.ambient=amb;o.kill_ambient=kamb;avp_runtime_bind(&o);
    memset(meta,0,sizeof(meta));for(unsigned i=0;i<16;i++){meta[i].priority=(s16)i;meta[i].synth_type=1;}meta[3].looping=1;avp_music_bind_sfx_meta(meta,16);
    InitSynt();assert(killed==1u&&music_stop==1u);
    ProtectV(2);
    u32 h1=DoEffect(3),h2=DoEffect(4);assert(h1!=0u&&h1!=(u32)-1&&h2!=h1&&played==2u);
    ModSfx(h1,123);assert(modified==1u&&last_handle==h1&&last_mod==123);
    StopLoop(h1);KillSfx(h2);assert(stopped==1u&&last_handle==h2);
    StartAla(5);assert(alarm_id!=0u);KillAlar();assert(alarm_id==0u);
    Ambient();assert(ambient_id!=0u&&ambient_start==1u);KillAmbi();assert(ambient_kill==1u&&ambient_id==0u);
    StartMus(7);assert(current_music==7u&&music_start==1u);StopMusi();assert(current_music==0u);
    /* The title->SELECT handoff changes to a distinct retail RDB score, not
     * a replay of the title descriptor or a synthesized placeholder. */
    assert(AVP_MUSIC_CHARACTER_SELECT==0x0002046cu);
    StartMus(AVP_MUSIC_CHARACTER_SELECT);
    assert(current_music==AVP_MUSIC_CHARACTER_SELECT&&music_start==2u);
    StopMusi();assert(current_music==0u);
    FXFadeDo(0x1000);WaitFXFa();assert(fx_fade_level==0);FXFadeUp(0x1000);WaitFXFa();assert(fx_fade_level==0x7fff);
    /* End_Sele starts this exact 0xFA fade. ScreenOff waits for completion
     * before StopMusi/gameplay. Assert the audio level reaches silent only
     * after 132 source-rate VBL steps, not at the Fire press itself. */
    o.wait_vblank=fade_vblank;o.set_master_volume=fade_volume;avp_runtime_bind(&o);
    fade_vblanks=0u;
    FXFadeDo(0xfau);
    assert(fade_vblanks==0u&&fx_fade_level==0x7fff);
    WaitFXFa();
    assert(fade_vblanks==132u && fx_fade_level==0 && last_master_volume==0u);
    FXFadeUp(0x1000);WaitFXFa();assert(fx_fade_level==0x7fff);
    o.wait_vblank=NULL;o.set_master_volume=NULL;avp_runtime_bind(&o);
    avp_music_bind_transfer_arena(arena,sizeof(arena));const void *a=TransnFi(&f),*b=TransnFi(&f);assert(a&&a==b&&memcmp(a,payload,sizeof(payload))==0);
    /* On the Jaguar, a completed DSP one-shot frees its CPU voice.  The
       PC runtime must recycle that voice instead of dropping future SFX. */
    o.play_sfx_handle=play_handle;o.sfx_handle_active=is_active;
    avp_runtime_bind(&o);memset(live,0,sizeof(live));InitSynt();ProtectV(2u);
    for(unsigned i=0;i<6u;i++)assert(DoEffect(0u)!=(u32)-1);
    assert(DoEffect(0u)!=(u32)-1); /* rapidly retriggering the same weapon effect reuses its own active voice */
    /* Distinct weapons must also be audible when six same-priority ordinary
       effect channels are busy.  In the retail allocator equality is eligible
       for replacement; the old strict comparison starved Predator cues. */
    {u32 pred_laser=DoEffect(AVP_FX_ROCKE);
     assert(pred_laser!=(u32)-1&&pred_laser!=0u);
     assert(live[2]==pred_laser);
     u32 pred_disc=DoEffect(AVP_FX_DISK1);
     assert(pred_disc!=(u32)-1&&pred_disc!=0u);
     assert(live[2]==pred_disc);
     u32 pred_blade=DoEffect(AVP_FX_WRBLA);
     assert(pred_blade!=(u32)-1&&pred_blade!=0u);
     assert(live[2]==pred_blade);
     u32 pulse=DoEffect(AVP_FX_PLSX1);
     assert(pulse!=(u32)-1&&pulse!=0u);
     assert(live[2]==pulse);
    }
    live[2]=0u;                      /* DSP completed this one-shot */
    assert(DoEffect(0u)!=(u32)-1);  /* next SFX must play */
    /* The countdown's synthesized alarm uses its own protected voice.  Its
       unloop must preserve a concurrent weapon in voice 3 and ambience: the
       old PC callback killed every hosted voice at countdown expiration. */
    o.unloop_sfx=unloop;
    avp_runtime_bind(&o);
    {
        u32 weapon=live[3];
        unsigned prev_kills=killed;
        StartAla(AVP_FX_NEWAL);
        assert(alarm_id!=0u&&live[2]==alarm_id&&last_id==AVP_FX_NEWAL);
        assert(live[3]==weapon&&killed==prev_kills);
        KillAlar();
        assert(alarm_id==0u&&alarm_unloops==1u&&unlooped_voice==2u);
        assert(live[3]==weapon&&killed==prev_kills);
    }
    /* Retail IDs exceed synthetic test meta bounds.  The PC startup must
       initialize voice slots before the first real weapon SOUND instruction. */
    avp_music_bind_sfx_meta(NULL,0u);
    memset(live,0,sizeof(live));InitSynt();
    {u32 weapon=TFX(AVP_FX_SHOTG);
     assert(weapon!=(u32)-1&&weapon!=0u&&live[2]==weapon);}
    /* MAZE.S computer-entry unloop_all must release ordinary looping SFX,
       but neither kill the ambient channel nor end a protected countdown.
       Unlike a hard stop, unloop preserves a playing sample's tail. */
    memset(live,0,sizeof(live));InitSynt();
    {
        u32 alarm,weapon;
        unsigned prev_killed=killed,prev_ambient_kill=ambient_kill;
        Ambient();
        StartAla(AVP_FX_NEWAL);
        alarm=alarm_id;
        weapon=DoEffect(AVP_FX_FLAMX);
        assert(weapon!=(u32)-1&&weapon!=0u);
        alarm_unloops=0u;
        memset(unlooped_handles,0,sizeof(unlooped_handles));
        UnLoopAl();
        assert(alarm_id==alarm&&live[2]==alarm);
        assert(ambient_kill==prev_ambient_kill&&killed==prev_killed);
        assert(alarm_unloops==1u&&unlooped_handles[0]==weapon);
        KillAlar();
        assert(alarm_unloops==2u&&unlooped_handles[1]==alarm);
    }
    /* MAZE.S::PlayAvP releases the alarm and ambient handles on campaign
     * exit before MAIN.S enters the ending/Hall of Fame.  In the hosted
     * entry this sequence must not be reduced to KillAll alone: KillAll
     * stops the voices but does not clear either saved CPU handle. */
    memset(live,0,sizeof(live));InitSynt();
    {
        const unsigned before_killed=killed, before_ambient_kill=ambient_kill;
        unsigned before_unloops;
        Ambient();
        StartAla(AVP_FX_NEWAL);
        assert(alarm_id!=0u&&ambient_id!=0u);
        before_unloops=alarm_unloops;
        KillAlar();
        KillAll();
        KillAmbi();
        assert(alarm_id==0u&&ambient_id==0u);
        assert(killed==before_killed+1u);
        assert(ambient_kill==before_ambient_kill+1u);
        assert(alarm_unloops>=before_unloops+1u);
    }
    /* Test the portable bridge too: snapshot before KillSounds, then restore
       the original handle mapping without an unwanted second SFX start. */
    memset(live,0,sizeof(live));InitSynt();
    o.pause_sfx_save=save_pause;o.pause_sfx_restore=restore_pause;
    avp_runtime_bind(&o);
    {
        u32 weapon,alarm;unsigned saved_plays=played;
        weapon=DoEffect(AVP_FX_PLSX1);
        StartAla(AVP_FX_NEWAL);alarm=alarm_id;
        avp_pause_audio_save();
        assert(pause_saved==1u);
        avp_pause_audio_restore();
        assert(pause_restored==1u);
        assert(paused_alarm==alarm);
        /* When the allocation landed in voice 3 rather than voice 1, the
           pause routine must not create a phantom saved weapon voice. */
        assert(paused_weapon==0u);
        assert(played==saved_plays);
        assert(weapon!=0u&&alarm!=0u);
    }
    /* The 68000 DoEffect low-word priority comes from each retail module's
     * own volume/control word, not from the fallback priority zero.  Fill a
     * small synthetic RDB descriptor table (no retail ROM media here) and
     * verify that a low-priority click cannot steal a higher-priority cue. */
    {
        static u8 retail[0x2868u];
        unsigned i;
        u32 high,low;
        for(i=0;i<98u;i++){
            size_t off=(size_t)(AVP_FX_TRAC1-0x20300u)+(size_t)i*80u;
            retail[off+3u]=32u; /* big-endian sampler module type */
            retail[off+4u]=0x80u; /* loop flag plus $3000 priority */
            retail[off+6u]=0x30u;
        }
        /* A quiet click, a loud weapon and an FM module have different
         * source control words and independent loop flags. */
        {
            size_t off=AVP_FX_TRAC2-0x20300u;
            retail[off+6u]=0x10u;
            off=AVP_FX_SHOTG-0x20300u;
            retail[off+6u]=0x60u;
            off=AVP_FX_PHIT3-0x20300u;
            retail[off+6u]=0x70u;
            off=AVP_FX_TRAC1-0x20300u;
            retail[off+3u]=4u;retail[off+4u]=0u;
            retail[off+24u]=0x80u;retail[off+26u]=0x41u;
        }
        avp_music_bind_retail_sfx_descriptors(retail,sizeof(retail));
        memset(live,0,sizeof(live));InitSynt();ProtectV(2u);
        for(i=0;i<6u;i++)assert(TFX(AVP_FX_SHOTG)!=(u32)-1);
        /* ROM $9CC4 CMP.W D4,D1 / $9CC6 BGT: equal-priority voices must
         * NOT be stolen when all six unprotected retail voices are busy. */
        assert(TFX(AVP_FX_SHOTG)==(u32)-1);
        low=TFX(AVP_FX_TRAC2);
        assert(low==(u32)-1); /* lower priority cannot evict a weapon */
        high=TFX(AVP_FX_PHIT3);
        assert(high!=0u&&high!=(u32)-1); /* higher priority is eligible */
        /* A malformed/incomplete RDB must invalidate the retail table,
         * never leave a stale or partially filled priority map. */
        avp_music_bind_retail_sfx_descriptors(retail,sizeof(retail)-1u);
        memset(live,0,sizeof(live));InitSynt();ProtectV(2u);
        for(i=0;i<6u;i++)assert(TFX(AVP_FX_SHOTG)!=(u32)-1);
        assert(TFX(AVP_FX_TRAC2)!=(u32)-1); /* default equal priority */
        avp_music_bind_retail_sfx_descriptors(retail,sizeof(retail));
        memset(live,0,sizeof(live));InitSynt();ProtectV(2u);
        {u32 fm=TFX(AVP_FX_TRAC1);assert(fm!=0u&&fm!=(u32)-1);}
        /* The DSP voice type becomes free after an unlooped tail finishes,
         * even though StopLoop already cleared its 68000 VIdent handle.
         * Filling six channels before this transition used to drop all new
         * lower-priority cues until pause forcibly reset the whole mixer. */
        o.sfx_voice_active=is_voice_active;
        avp_runtime_bind(&o);
        memset(live,0,sizeof(live));InitSynt();ProtectV(2u);
        for(i=0;i<6u;i++)assert(TFX(AVP_FX_SHOTG)!=(u32)-1);
        {u32 ending_handle=live[2];
         StopLoop(ending_handle);
         live[2]=0u; /* the DSP finished the released tail */
         assert(TFX(AVP_FX_TRAC2)!=(u32)-1);
         assert(live[2]!=0u);}
        avp_music_bind_retail_sfx_descriptors(NULL,0u);
    }
    /* R206 regression: MAIN.S post-game Hall of Fame reserves seven of
     * eight Jaguar voices. The hosted NewTitle -> pc_begin_campaign path
     * must restore the default protection count; otherwise every new
     * campaign has only a single SFX channel and effects interrupt peers. */
    memset(live,0,sizeof(live));InitSynt();ProtectV(7u);
    assert(TFX(AVP_FX_SHOTG)!=(u32)-1 && live[7]!=0u);
    KillAll();memset(live,0,sizeof(live));
    ProtectV(2u); /* same call as the host's from-title campaign boundary */
    for(unsigned i=2u;i<8u;i++){
        assert(TFX(AVP_FX_SHOTG)!=(u32)-1);
        assert(live[i]!=0u);
    }
    /* Countdown alarm still explicitly reserves the extra third voice. */
    KillAll();memset(live,0,sizeof(live));
    StartAla(AVP_FX_NEWAL);
    assert(alarm_id && live[2]==alarm_id);
    assert(TFX(AVP_FX_SHOTG)!=(u32)-1 && live[3]!=0u);
    KillAlar();
    avp_runtime_reset_ops();return 0;
}
