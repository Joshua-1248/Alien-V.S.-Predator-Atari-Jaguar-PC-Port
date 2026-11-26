static u16 *mp_keycard_owner;
void avp_collect_mp_keycard_owner(u16 *collected_mask)
{ mp_keycard_owner=collected_mask; }

static AvpCollectMpItemClaimFn mp_item_claim;
static void *mp_item_user;
static AvpCollectMpBodyLootFn mp_body_loot;
static void *mp_body_user;
void avp_collect_set_mp_body_loot(AvpCollectMpBodyLootFn fn,void *user)
{ mp_body_loot=fn;mp_body_user=fn?user:NULL; }

void avp_collect_set_mp_item_claim(AvpCollectMpItemClaimFn fn,void *user)
{ mp_item_claim=fn; mp_item_user=fn?user:NULL; }

