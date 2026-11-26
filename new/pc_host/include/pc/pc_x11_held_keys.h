#ifndef AVP_PC_X11_HELD_KEYS_H
#define AVP_PC_X11_HELD_KEYS_H

/* X11 autorepeat may deliver synthetic releases even while a physical key is
 * still down. Treat the X server keymap as authoritative after a key event,
 * without relying on the repeat timer or advancing the Jaguar simulation. */
static inline int avp_pc_x11_physical_down(const char physical[32],unsigned code)
{
    return physical && code<256u &&
        (((unsigned char)physical[code>>3]>>(code&7u))&1u);
}
static inline void avp_pc_x11_reconcile_key(unsigned char *held,
    unsigned long *sym,int physical_down,unsigned long mapped_sym)
{
    if(!held||!sym)return;
    if(!physical_down){*held=0;*sym=0;}
    else if(!*held && mapped_sym){*held=1;*sym=mapped_sym;}
}

#endif
