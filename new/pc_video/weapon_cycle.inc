void avp_weapons_cycle(int direction,int maxslot)
{
    int base=new_wepno>=1&&new_wepno<=maxslot?new_wepno:cur_wepno;
    /* Slots 5/6 are Predator cloak and medkit actions, not switchable guns.
     * Slot 0 is the Marine's special weaponless starting state. */
    if(maxslot>4)maxslot=4;
    if(!direction||maxslot<1)return;
    if(base<1||base>maxslot)base=1;
    for(int n=0;n<maxslot;n++){
        base+=direction>0?1:-1;
        if(base>maxslot)base=1;else if(base<1)base=maxslot;
        if(cur_weps&(1u<<base)){new_wepno=(s16)base;return;}
    }
}

