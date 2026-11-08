#ifndef AVP_AMP_H
#define AVP_AMP_H
#include "avp_types.h"

#define AVP_NUM_AMPS 300
#define AVP_AMP_GRID_W 64
#define AVP_AMP_GRID_H 64
#define AVP_AMP_OBJ_ROW_BYTES 128
#define AVP_AMP_OBJ_H 64

typedef struct AvpAmp AvpAmp;
typedef void (*AvpAmpModeFn)(AvpAmp *amp);

struct AvpAmp {
    s32 xpos,ypos;
    AvpAmpModeFn mode;
    u16 creature;
    u16 animseq;
    u16 animframe;
    u16 angle;
    s32 timer;
    s16 xvel,yvel;
    s16 ldir;
    s32 xvector,yvector;
    s16 energy,oldenergy;
    u16 flags;
    s16 yoffset;
    s16 astype;
    void *aux_ptr; /* host-readable form of pointer aliases carried in amp_timer */
    u8 host_static; /* source amp_mode == -1 */
};

enum AvpAmpFlag {
    AMP_KILLABLE=0,AMP_DEADGEN,AMP_PLAYER,AMP_DEAD,AMP_COLLECT,AMP_MOTION,AMP_STUN,
    AMP_PHIT,AMP_CHATFLAG,AMP_EGGOPEN,AMP_CLOSEHIT,AMP_INVHIT,AMP_SSHIELD,AMP_ALTFRAME=15
};

enum AvpAnimSeq {
    AS_HIDDEN=0,AS_STAND,AS_DEATH,AS_RUN,AS_WALK,AS_KNOCKBACK,
    AS_FIGHT1,AS_FIGHT2,AS_FIGHT3,AS_FIGHT4,AS_FIGHT5,AS_FIGHT6
};

enum AvpCreatureType {
    AC_ALIEN=0,AC_PREDATOR,AC_HUMAN,AC_EGG,AC_FACEHUG,AC_ACRAWL,AC_SMART,AC_BOLT,
    AC_FIRE,AC_SPARK1,AC_SPARK2,AC_AID,AC_AMMO1,AC_AMMO2,AC_BAG,AC_CAN,AC_CART,
    AC_CHAIN,AC_COCOON,AC_DISK,AC_DRUM,AC_FOOD,AC_GAS,AC_GREN,AC_GROUP,AC_SHELLS,
    AC_TABLE1,AC_TABLE2,AC_TABLE3,AC_DEADOFF,AC_DEADMED,AC_ABLOOD,AC_PBLOOD,
    AC_MBLOOD,AC_CRYO,AC_OPTABLE,AC_BED,AC_AQUEEN,AC_GEN,AC_AQSHIELD
};

void InitAMPs(void);
void initialise(void);
void wipe_flgs(void);
void build_level(void);
AvpAmp *amp_req(void);
void amp_release(unsigned current_amp_number_1based);
void save_level(void);
void restore_level(void);
/* Debug-only, invalidates ONLY the current level save/visited flag.
 * The next restore_level executes the original first-visit population. */
void avp_amp_dev_freshen_current_level(void);
/* Multiplayer-only campaign objective controls. Default state preserves the
 * exact retail single-player AMP modes and restore_level decisions. */
void avp_amp_mp_defer_end_queen(int enabled);
void avp_amp_mp_set_campaign_type(s16 type);
int avp_amp_mp_queen_predator_ending(s16 acting_player_type);

void avp_amp_set_save_callback(void (*fn)(AvpAmp *amps,unsigned count));
void avp_amp_set_restore_callback(void (*fn)(AvpAmp *amps,unsigned count));

/* Presentation-only slot generation; never used by game collision or AI. */
u32 avp_amp_slot_generation(unsigned slot);
extern AvpAmp level1amps[AVP_NUM_AMPS];
extern AvpAmp *amp_data,*amps_at;
extern u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
extern u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H];
extern u16 levels_visit,discflag;
extern u8 inventory_table[16];
extern u32 ampcount;


/* Source-level explicit-argument forms of register-call AMP helpers. */
/* Optional PC-local Deathmatch projectile callbacks. All default to NULL
 * and are never installed for single-player or co-op. Original AMP modes,
 * damage tables and collision handling remain the authorities for enemies. */
typedef void (*AvpAmpMpProjectileSpawn)(void *user,const AvpAmp *projectile);
typedef int (*AvpAmpMpProjectileHit)(void *user,const AvpAmp *projectile);
typedef void (*AvpAmpMpExplosionSpawn)(void *user,const AvpAmp *bolt,
                                        const AvpAmp *explosion_amp);
typedef int (*AvpAmpMpExplosionDamage)(void *user,const AvpAmp *explosion_amp);
void avp_amp_set_mp_projectile_hooks(AvpAmpMpProjectileSpawn spawn,
    AvpAmpMpProjectileHit hit,AvpAmpMpExplosionSpawn explosion_spawn,
    AvpAmpMpExplosionDamage explosion_damage,void *user);
AvpAmp *player_weapon(s32 heading_x,s32 heading_y,u16 projectile_kind,u16 reset_counter,
                      s32 sin_angle,s32 cos_angle);
void lostplayer(AvpAmp *amp);
void UpdateAMPs(void);
/* Multiplayer extension only. Reuses original acid contact math and damage
 * against the currently scoped player; never invoked by single-player. */
int avp_amp_acid_contact_player(const AvpAmp *puddle);
/* Exact native acid mode; false during the preceding death animation. */
int avp_amp_is_acid_puddle(const AvpAmp *puddle);
/* PC-only developer option: suspend hostile AMP state-machine updates. */
/* PC development cheats, inactive by default. All normal Jaguar paths are retained. */
void avp_amp_set_attacks_disabled(int enabled);
int avp_amp_attacks_disabled(void);
/* Optional PC-only Marine pulse-rifle final-frame line-of-sight damage gate. */
void avp_amp_set_marine_pulse_los_hits(int enabled);
int avp_amp_marine_pulse_los_hits(void);
/* Optional PC-only fatal-hit interruption of enemy Marine pulse bursts. */
/* PC-only opt-in: contact, latch and melee hits use radial reach instead
 * of Jaguar's world-axis-aligned square range. Facing/turning never changes
 * the retail hitbox. Default OFF preserves all original comparisons. */
void avp_amp_set_normalize_diagonal_contact_range(int enabled);
/* PC-only enemy pursuit wake distance: 0 original, 1=2x, 2=3x, 3=4x, 4 unlimited. */
void avp_amp_set_enemy_activation_distance(unsigned mode);
unsigned avp_amp_enemy_activation_distance(void);
int avp_amp_normalize_diagonal_contact_range(void);
void avp_amp_set_marine_fire_interrupt(int enabled);
int avp_amp_marine_fire_interrupt(void);
/* Bound only during a native multiplayer AMP-target callback; default off. */
void avp_amp_set_marine_multiplayer_target(int enabled);
/* type==-1 kills all living hostiles; otherwise exact AvpCreatureType. */
unsigned avp_amp_dev_kill_enemies(int type);
/* PC cheat only: remove all hostile AMP actors from the currently bound
 * level without invoking deaths, scoring, pickups, or Queen progression. */
unsigned avp_amp_dev_clear_enemies(void);
/* PC cheat: despawn only damaging level hazards (explosive drums, Queen
 * electric shields/generators, and active Alien acid), no death triggers. */
unsigned avp_amp_dev_clear_hazards(void);
void avp_amp_set_facehug_attach_disabled(int enabled);
int avp_amp_facehug_attach_disabled(void);
void avp_amp_set_enemies_frozen(int enabled);
int avp_amp_enemies_frozen(void);
void chase_player(AvpAmp *amp);
void pre_discmove(AvpAmp *amp);
void discmove(AvpAmp *amp);
void pre_flame_on(AvpAmp *amp);
void flame_on(AvpAmp *amp);
void make_spark_at(s32 x,s32 y,int blood);
void make_spark_hit_at(s32 x,s32 y,const AvpAmp *victim);
void make_spark(s32 x,s32 y,const AvpAmp *victim);
void explosion_at(s32 x,s32 y);
AvpAmp *explosion(s32 x,s32 y,s16 yoff,s16 damage,u32 range);
void avp_amp_set_chase_callback(void (*fn)(AvpAmp *amp,s32 target_x,s32 target_y));

extern u16 ngens;
extern u16 avp_reset_counter;
void avp_amp_set_reset_counter(u16 counter);
AvpAmp *weapon_hit(AvpAmp *projectile);
void cgenmode(AvpAmp *amp);
void generate(AvpAmp *amp);
void side_shield(AvpAmp *amp,s32 dx,s32 dy);
AvpAmp *make_shield(AvpAmp *owner,s32 dx,s32 dy);
void shield2(AvpAmp *amp);
void genmode(AvpAmp *amp);
void exp_handle(AvpAmp *amp);

/* Explicit routine-level lifts retained under their historical names. */
void QFRAME(AvpAmp *amp, u16 frame);
void do_score(AvpAmp *amp);
void amp_setgrid(AvpAmp *amp);


typedef struct AvpAmpPlacement { s16 x,y; u16 def; } AvpAmpPlacement;
typedef struct AvpAmpSavedPlacement { s16 x,y,astype; u16 flags; } AvpAmpSavedPlacement;
typedef struct AvpAmpRandomEntry { s16 def,count; } AvpAmpRandomEntry;
typedef struct AvpAmpTemplate {
    AvpAmpModeFn mode; u16 creature,flags_or;
    u16 animseq,animframe,angle; s32 timer; s16 xvel,yvel,ldir;
    s32 xvector,yvector; s16 energy,oldenergy; u16 flags; s16 yoffset;
    u8 host_static; /* historical amp_mode == -1; distinct from free mode == 0 */
} AvpAmpTemplate;

enum AvpAmpLevelList { AVP_AMP_LIST_HUMAN=0, AVP_AMP_LIST_PREDATOR=1, AVP_AMP_LIST_ALIEN=2, AVP_AMP_LIST_COMMON=3 };
enum AvpAmpFightAction { AVP_AMP_FIGHT_NONE=0, AVP_AMP_FIGHT_FLAME=1, AVP_AMP_FIGHT_CROUCH=2 };
void avp_amp_bind_level_list(unsigned list_kind,unsigned level_1based,const AvpAmpPlacement *list);
void avp_amp_bind_random_list(unsigned player_kind,unsigned level_1based,const AvpAmpRandomEntry *list);
const AvpAmpSavedPlacement *avp_amp_saved_level(unsigned level_1based);
void avp_amp_bind_templates(const AvpAmpTemplate *templates,unsigned count);
void avp_amp_bind_retail_source_templates_and_random(void);
unsigned avp_amp_bound_template_count(void);
unsigned avp_amp_bound_level_list_count(void);
/* Historical template-mode resolver. Authored template numeric payloads may be
 * loaded by the resource layer while mode identity remains executable C. */
AvpAmpModeFn avp_amp_builtin_mode_for_def(unsigned def_index);

void avp_amp_set_beam_counter(u32 value);
/* $F00004 is read again after the Marine line-of-sight calculation.
 * On Jaguar these two volatile hardware reads are not the same sample. */
void avp_amp_set_beam_weapon_sample(u32 (*sample)(void));
void avp_amp_set_fight_callback(void (*fn)(AvpAmp *,int action));
void avp_amp_set_death_callback(void (*fn)(AvpAmp *));
void avp_amp_set_lift_test_callback(int (*fn)(const AvpAmp *));
void level_loop(void);
void next_creature(const AvpAmpPlacement **cursor);
void append_objs(void);
void rebuild_level(void);
void ram_save(void);
void xcocoons(void);
void rand_set(void);
void make_queen(void);
void make_end_queen(void);
void end_preds(void);
void gofight(AvpAmp *amp);
void stun_mode(AvpAmp *amp);
void stun_death(AvpAmp *amp);
void cocoon(AvpAmp *amp);
void eggwait(AvpAmp *amp);
void lockxxx(AvpAmp *amp);

#endif
