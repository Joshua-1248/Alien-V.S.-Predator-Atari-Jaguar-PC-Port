#ifndef AVP_DEV_CHEATS_H
#define AVP_DEV_CHEATS_H

void avp_dev_cheats_restore_health(void);
void avp_dev_cheats_set_tracker(int enabled);
int avp_dev_cheats_tracker_enabled(void);
int avp_dev_cheats_tracker_borrowed(void);
void avp_dev_cheats_tracker_borrowed_restore(int borrowed);
void avp_dev_cheats_tracker_collected(void);
void avp_dev_cheats_tracker_restored(void);
void avp_dev_cheats_adjust_security(int delta);
void avp_dev_cheats_restore_ammo(void);
void avp_dev_cheats_give_weapons(void);
void avp_dev_cheats_set_infinite_health(int enabled);
void avp_dev_cheats_set_infinite_ammo(int enabled);
int avp_dev_cheats_infinite_health(void);
int avp_dev_cheats_infinite_ammo(void);
void avp_dev_cheats_tick(void);

#endif
