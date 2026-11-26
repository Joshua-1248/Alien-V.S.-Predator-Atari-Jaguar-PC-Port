    /* Optional host F2 launch probe. Always part of the ABI because the
     * avp_c runtime and avp_pc executable have different feature defines. */
    int  (*frontend_mp_launch_pending)(void *user);
    /* PC-only F2 setup visibility query. Unlike frontend input polling, this
     * is read after each MJP title tick to suspend the retail idle countdown.
     * NULL retains the original 800-tick Jaguar attract sequence unchanged. */
    int  (*frontend_mp_menu_open)(void *user);
