/* Co-op items have per-player claims, whereas a legacy single-player body
 * can already carry AMP_COLLECT in a retained/rebuilt world. Do not discard
 * a REAL, source-authored security-card corpse for an unclaimed co-op Marine
 * merely because its collected flag survived or objmap now owns another item.
 * Match the real corpse AMP at the exact authored cell; never synthesize a
 * card from the nearby food corpse or from a recycled/absent AMP slot.
 * Return 0 unless the actual player is still eligible for this exact item. */
static u16 coop_unclaimed_card_body(unsigned x,unsigned y)
{
    const BodyLoot *b;
    unsigned i;
    if(!mp_item_claim || player_type!=PT_HUMAN)return 0;
    for(b=cur_bodies;b->x>=0;++b){
        if((unsigned)b->x!=x||(unsigned)b->y!=y ||
           b->type<COLL_S1 || b->type>COLL_S10)continue;
        for(i=0;i<AVP_NUM_AMPS;++i){
            const AvpAmp *a=&amp_data[i];
            /* A corpse is an authored collectable: 38/39/40. Type 18 is
             * the live cocoon and must NOT stand in for a card body. */
            if(a->astype!=38 && a->astype!=39 && a->astype!=40)continue;
            if(((u32)a->xpos>>16)!=x || ((u32)a->ypos>>16)!=y)
                continue;
            /* Co-op card ownership belongs to the REAL authored corpse,
             * not to its incidental mode/host_static bits. Level resets,
             * world snapshots and entrance cleanup may leave both bits
             * cleared before the player first reaches the corpse. Requiring
             * either one made Food in objmap win until the next respawn.
             * Type AND exact authored cell are still mandatory: an empty,
             * moved or reused AMP slot can never create a phantom card. */
            if(mp_item_claim(mp_item_user,(unsigned)cur_level,i,b->type,0))
                return (u16)(i+1u);
        }
    }
    return 0;
}

