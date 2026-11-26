/* Read-only exposure of the SAME original pickup definitions used by
   Collectables(). Bots may inspect source loot without consuming it or
   changing per-player inventory, original AMP ownership, or world state. */
int avp_collect_peek(unsigned level,unsigned x,unsigned y,s16 def,
                     s16 *type,s16 *amount)
{
    const BodyLoot *b;
    if(!type||!amount||!collect_info_for(def,type,amount))return 0;
    if(*type!=-1)return 1;
    for(b=body_loot_for_level(level);b->x>=0;++b)
        if((unsigned)b->x==x && (unsigned)b->y==y){
            *type=b->type;*amount=b->amount;return 1;
        }
    if(def==39){*type=COLL_SHOTAMMO;*amount=15;return 1;}
    if(def==38){*type=COLL_PULSEAMMO;*amount=60;return 1;}
    if(def==40){*type=COLL_MEDAID;*amount=300;return 1;}
    return 0; /* Unsupported corpse: never fabricate a reward. */
}

