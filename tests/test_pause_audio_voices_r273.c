/* R273 — exercise the actual MAZESCRN.S pause sound helpers with the real
 * MUSIC.o voice allocator.  No synthetic replacement for the production
 * pause_sfx or reserved voice mechanism is used. */
#ifndef MAZE_SOURCE
#define MAZE_SOURCE "../src/game/mazescrn.c"
#endif
#include MAZE_SOURCE
#include <assert.h>

static unsigned starts, last_voice, last_id;
static u32 last_handle;
static void observe_sfx(void *ctx,unsigned id){
    (void)ctx;(void)id;
    assert(!"pause sound must use the reserved voice allocator");
}
static void observe_voice(void *ctx,unsigned id,u32 handle,unsigned voice){
    (void)ctx;
    ++starts;last_id=id;last_handle=handle;last_voice=voice;
}
int main(void){
    AvpRuntimeOps ops={0};
    ops.play_sfx=observe_sfx;
    ops.play_sfx_handle=observe_voice;
    avp_runtime_bind(&ops);
    InitSynt();
    /* The source pause_sfx path uses ambient voice 0, not TFX or voice 1. */
    pause_sfx(SFX_COCK);
    assert(starts==1u && last_id==AVP_FX_COCK1 && last_voice==0u && last_handle);
#ifndef LEGACY_BUILD
    /* Pause screen's shotgun and laugh confirmation call wep_sfx: voice 1. */
    pause_weapon_sfx(SFX_SHOTGUN);
    assert(starts==2u && last_id==AVP_FX_SHOTG && last_voice==1u && last_handle);
    pause_weapon_sfx(AVP_FX_LAFF);
    assert(starts==3u && last_id==AVP_FX_LAFF && last_voice==1u && last_handle);
    /* Ambient voice 0 remains addressable independently. */
    pause_sfx(SFX_COCK);
    assert(starts==4u && last_id==AVP_FX_COCK1 && last_voice==0u && last_handle);
#endif
    avp_runtime_reset_ops();
    return 0;
}
