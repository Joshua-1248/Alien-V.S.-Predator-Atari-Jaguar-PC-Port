#include "hud_message.h"
#include "collectables.h"
#include <assert.h>
#include <string.h>

static unsigned calls,clears,x;
static char lines[3][32];
static void clear(void){++clears;}
static void draw(unsigned line,const char *s,unsigned xp)
{
    assert(line<3u && strlen(s)<sizeof(lines[0]));
    ++calls;x=xp;strcpy(lines[line],s);
}

/* The 12 original HUD_MSG pickup streams are consecutive 40-byte records in
   the user's retail compressed RDB.DTA at Jaguar addresses 0x25086..0x25265.
   Each entry carries the initial color and the six subsequent command words. */
typedef struct ExpectedPickup {
    enum AvpCollectNotice notice;
    unsigned detail;
    const char *line[3];
    unsigned first_rgb;
    unsigned hold_rgb;
} ExpectedPickup;
static const ExpectedPickup pickup_rom[]={
    {AVP_CNOTICE_CARD,10u,{"SECURITY","CARD #10","COLLECTED"},0xffffffu,0xffffffu},
    {AVP_CNOTICE_SHOTGUN,1u,{"SHOTGUN","COLLECTED",NULL},0xffff00u,0xffff00u},
    {AVP_CNOTICE_SHOT_AMMO,1u,{"SHOTGUN","ROUNDS","COLLECTED"},0xffffffu,0xffffffu},
    {AVP_CNOTICE_PULSE,2u,{"PULSE RIFLE","COLLECTED",NULL},0xffff00u,0xffff00u},
    {AVP_CNOTICE_PULSE_AMMO,2u,{"PULSE RIFLE","ROUNDS","COLLECTED"},0xffffffu,0xffffffu},
    {AVP_CNOTICE_FLAME,3u,{"FLAME","THROWER","COLLECTED"},0xffffffu,0xffffffu},
    {AVP_CNOTICE_FLAME_AMMO,3u,{"GAS","COLLECTED",NULL},0xffff00u,0xffff00u},
    {AVP_CNOTICE_SMART,4u,{"SMARTGUN","COLLECTED",NULL},0xffff00u,0xffff00u},
    {AVP_CNOTICE_SMART_AMMO,4u,{"SMARTGUN","AMMO","COLLECTED"},0xffffffu,0xffffffu},
    {AVP_CNOTICE_MTRACKER,0u,{"MOTION","TRACKER","COLLECTED"},0xffffffu,0xffffffu},
    {AVP_CNOTICE_FOOD,0u,{"FOOD","COLLECTED",NULL},0xffff00u,0xffffffu},
    {AVP_CNOTICE_MEDAID,0u,{"MEDKIT","COLLECTED",NULL},0xffff00u,0xffff00u},
};

typedef struct ExpectedSpeciesNotice {
    const AvpHudMessageStep *steps;
    const char *lines[3];
    unsigned initial_rgb,hold_ticks,flash_ticks;
} ExpectedSpeciesNotice;
static void check_species_messages(void)
{
    static const ExpectedSpeciesNotice cases[]={
        {avp_msg_gain_combi,{"COMBI STICK","ATTAINED",NULL},0xffff00u,6u,7u},
        {avp_msg_lose_combi,{"COMBI STICK","LOST",NULL},0xffff00u,6u,7u},
        {avp_msg_gain_disc,{"SMART DISC","ATTAINED",NULL},0xffff00u,6u,7u},
        {avp_msg_lose_disc,{"SMART DISC","LOST",NULL},0xffff00u,6u,7u},
        {avp_msg_gain_cannon,{"SHOULDER","CANNON","ATTAINED"},0xffffffu,6u,7u},
        {avp_msg_lose_cannon,{"SHOULDER","CANNON","LOST"},0xffffffu,6u,7u},
        {avp_msg_stun,{"COCOON","THE ENEMY",NULL},0xffff00u,4u,5u},
        {avp_msg_cocooned,{"COCOON","COMPLETED",NULL},0xffff00u,4u,5u},
        {avp_msg_ccnfail,{"COCOON","FAILED",NULL},0xffff00u,4u,5u},
        {avp_msg_cheat_on,{NULL,"CHEAT ON",NULL},0x00ff00u,5u,0u},
        {avp_msg_cheat_off,{NULL,"CHEAT OFF",NULL},0x00ff00u,5u,0u}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
        const ExpectedSpeciesNotice *e=&cases[i];
        const AvpHudMessageStep *step;
        InitHUDMsg();memset(lines,0,sizeof(lines));calls=clears=0u;
        avp_hudmsg_queue(e->steps);
        UpdtHUDMsg();assert(msg_status==AVP_HUDMSG_NEW);
        assert(avp_hudmsg_rgb24()==e->initial_rgb);
        step=avp_hudmsg_current();assert(step);
        for(unsigned j=0;j<3u;j++){
            if(e->lines[j])assert(step->line[j]&&!strcmp(step->line[j],e->lines[j]));
            else assert(!step->line[j]);
        }
        ShowHUDMsg();assert(clears==1u);
        UpdtHUDMsg();assert(msg_status==e->hold_ticks);
        for(unsigned k=0;k<e->hold_ticks;k++)UpdtHUDMsg();
        if(e->flash_ticks){
            assert(msg_status==e->flash_ticks&&avp_hudmsg_rgb24()==0u);
            for(unsigned phase=0;phase<3u;phase++){
                for(unsigned k=0;k<e->flash_ticks;k++)UpdtHUDMsg();
                assert(msg_status==e->flash_ticks);
                assert(avp_hudmsg_rgb24()==((phase&1u)?0u:e->initial_rgb));
            }
            for(unsigned k=0;k<e->flash_ticks;k++)UpdtHUDMsg();
        }
        assert(msg_status==AVP_HUDMSG_STOP);
    }
    InitHUDMsg();avp_hudmsg_queue(avp_msg_dead);
    assert(avp_hudmsg_critical_active());
    UpdtHUDMsg();ShowHUDMsg();assert(avp_hudmsg_critical_active());
    /* R272: HUD.S:405 re-queues the countdown only while msg_ptr==cdm_stop,
     * i.e. while the $0000FF00|$7f record counts -- not on the NEW frame or
     * during the 4-tick hold. */
    InitHUDMsg();avp_hudmsg_queue(avp_msg_countdown);
    assert(avp_hudmsg_critical_active());
    assert(!avp_hudmsg_countdown_active());
    UpdtHUDMsg();ShowHUDMsg();
    assert(!avp_hudmsg_countdown_active());
    UpdtHUDMsg();
    assert(avp_hudmsg_current()==avp_msg_countdown+1u);
    assert(!avp_hudmsg_countdown_active());
    for(unsigned k=0;k<4u;k++)UpdtHUDMsg();
    assert(avp_hudmsg_current()==avp_msg_countdown+2u);
    assert(avp_hudmsg_countdown_active());
    /* R272: PLAYER.S:831 writes new_message unconditionally, so a pickup
     * notice replaces the running countdown message. */
    avp_hudmsg_pickup(AVP_CNOTICE_MEDAID,0u);
    UpdtHUDMsg();
    assert(avp_hudmsg_current()!=avp_msg_countdown+2u);
    assert(!avp_hudmsg_countdown_active());
    avp_hudmsg_queue(avp_msg_dead);UpdtHUDMsg();
    assert(!avp_hudmsg_countdown_active());
    InitHUDMsg();assert(!avp_hudmsg_critical_active());
    assert(!avp_hudmsg_countdown_active());
}

static void check_retail_pickups(void)
{
    for(unsigned k=0;k<sizeof(pickup_rom)/sizeof(pickup_rom[0]);k++){
        const ExpectedPickup *e=&pickup_rom[k];
        const AvpHudMessageStep *p;
        InitHUDMsg();calls=clears=0u;memset(lines,0,sizeof(lines));
        avp_hudmsg_pickup(e->notice,e->detail);
        UpdtHUDMsg();assert(msg_status==AVP_HUDMSG_NEW);
        assert(avp_hudmsg_rgb24()==e->first_rgb);
        p=avp_hudmsg_current();assert(p);
        for(unsigned line=0;line<3u;line++){
            if(e->line[line])assert(p->line[line]&&!strcmp(p->line[line],e->line[line]));
            else assert(!p->line[line]);
        }
        ShowHUDMsg();assert(clears==1u);
        for(unsigned line=0;line<3u;line++)
            assert(!strcmp(lines[line],e->line[line]?e->line[line]:""));
        UpdtHUDMsg();assert(msg_status==4u);assert(avp_hudmsg_rgb24()==e->hold_rgb);
        for(unsigned phase=0;phase<4u;phase++){
            unsigned steps=(phase==0u)?4u:5u;
            for(unsigned tick=0;tick<steps;tick++)UpdtHUDMsg();
            assert(msg_status==5u);
            assert(avp_hudmsg_rgb24()==((phase&1u)?e->hold_rgb:0u));
        }
        for(unsigned tick=0;tick<5u;tick++)UpdtHUDMsg();
        assert(msg_status==AVP_HUDMSG_STOP);
    }
}

int main(void)
{
    InitHUDMsg(); avp_hudmsg_set_draw_callback(draw);avp_hudmsg_set_clear_callback(clear);
    avp_hudmsg_queue(avp_msg_dead);
    UpdtHUDMsg();assert(msg_status==AVP_HUDMSG_NEW);
    assert(avp_hudmsg_rgb24()==0x00ff00u);
    ShowHUDMsg();assert(msg_status==1u);assert(calls==1u);
    assert(!strcmp(lines[1],"GAME OVER"));assert(x==(192u-9u*13u)/2u);
    UpdtHUDMsg();assert(msg_status==254u);
    InitHUDMsg();avp_hudmsg_queue(avp_msg_pressure);UpdtHUDMsg();
    assert(avp_hudmsg_rgb24()==0xffff00u);
    ShowHUDMsg();UpdtHUDMsg();
    for(unsigned k=0;k<4u;k++)UpdtHUDMsg();
    assert(avp_hudmsg_rgb24()==0u);
    InitHUDMsg();avp_hudmsg_queue(avp_msg_notsecure);UpdtHUDMsg();
    assert(avp_hudmsg_rgb24()==0xffffffu);

    check_retail_pickups();
    check_species_messages();

    /* PLAYER.S give_wep passes the weapon notice into give_ammo during
     * its fall-through.  The same notice is therefore written twice. */
    InitHUDMsg();avp_hudmsg_pickup(AVP_CNOTICE_PULSE,2u);
    avp_hudmsg_pickup(AVP_CNOTICE_PULSE,2u);
    UpdtHUDMsg();ShowHUDMsg();assert(!strcmp(lines[0],"PULSE RIFLE"));
    assert(!strcmp(lines[1],"COLLECTED"));
    /* R286 retail PLAYER.S: an independent later pickup writes new_message,
     * even if a freshly acquired weapon wrote it earlier in the same frame. */
    InitHUDMsg();avp_hudmsg_pickup(AVP_CNOTICE_PULSE,2u);
    avp_hudmsg_pickup(AVP_CNOTICE_SHOT_AMMO,1u);
    UpdtHUDMsg();ShowHUDMsg();
    assert(!strcmp(lines[0],"SHOTGUN"));
    assert(!strcmp(lines[1],"ROUNDS"));
    assert(!strcmp(lines[2],"COLLECTED"));
    /* Independent same-weapon ammo pickups must also overwrite the notice
     * once the weapon has already been issued its own magazine. */
    InitHUDMsg();avp_hudmsg_pickup(AVP_CNOTICE_PULSE,2u);
    avp_hudmsg_pickup(AVP_CNOTICE_PULSE_AMMO,2u);
    avp_hudmsg_pickup(AVP_CNOTICE_PULSE_AMMO,2u);
    UpdtHUDMsg();ShowHUDMsg();
    assert(!strcmp(lines[1],"ROUNDS"));
    /* Each displayed notice clears leftover lines from the prior notice. */
    InitHUDMsg();avp_hudmsg_pickup(AVP_CNOTICE_FOOD,0u);
    UpdtHUDMsg();calls=clears=0u;ShowHUDMsg();assert(clears==1u&&calls==2u);
    /* R272: new_message is a plain longword in HUD_MSG.S; every producer
     * (PLAYER.S:831/855/884, MAZE.S:452, HUD.S:407) overwrites it, so the
     * last writer in a frame wins and a pickup replaces a running message. */
    InitHUDMsg();avp_hudmsg_queue(avp_msg_dead);
    avp_hudmsg_pickup(AVP_CNOTICE_SHOTGUN,1u);
    UpdtHUDMsg();ShowHUDMsg();assert(strcmp(lines[1],"GAME OVER"));
    InitHUDMsg();avp_hudmsg_pickup(AVP_CNOTICE_SHOTGUN,1u);
    avp_hudmsg_queue(avp_msg_dead);
    UpdtHUDMsg();ShowHUDMsg();assert(!strcmp(lines[1],"GAME OVER"));
    InitHUDMsg();avp_hudmsg_queue(avp_msg_countdown);
    UpdtHUDMsg();ShowHUDMsg();assert(!strcmp(lines[2],"TIME 1:00"));
    avp_hudmsg_pickup(AVP_CNOTICE_CARD,10u);
    UpdtHUDMsg();assert(avp_hudmsg_current()!=avp_msg_countdown+1u);
    return 0;
}
