/* Co-op-only actor scope: when non-NULL, security keycards are shared world
 * objects with independent per-player collection records. NULL retains the
 * exact original single-player consume-on-pickup behavior. This pointer is
 * only bound around an actor's original player tick on the main thread. */
void avp_collect_mp_keycard_owner(u16 *collected_mask);

/* Multiplayer-only collection eligibility/commit hook. A physical world AMP is
 * available once PER player life, not consumed for the entire co-op world.
 * Calling with commit=0 must not mutate ownership. Only call commit=1 after
 * CollectIt() actually grants or accepts the pickup. NULL keeps untouched
 * original single-player behavior. */
typedef int (*AvpCollectMpItemClaimFn)(void *user,unsigned level,
                                    unsigned amp_slot,s16 type,int commit);
void avp_collect_set_mp_item_claim(AvpCollectMpItemClaimFn fn,void *user);
/* Deathmatch ONLY: replace a non-keycard authored corpse's fixed loot with
 * a per-Marine once-only draw. A NULL callback leaves Jaguar original loot
 * unchanged. Return/claim semantics are owned by the host, not the AMP. */
typedef void (*AvpCollectMpBodyLootFn)(void *user,unsigned level,
                                     unsigned amp_slot);
void avp_collect_set_mp_body_loot(AvpCollectMpBodyLootFn fn,void *user);

