    /* Choose an unclaimed authored card before the objmap's last writer.
     * Keep the selected identity for the AMP_COLLECT legacy-world override. */
    coop_card_no=coop_unclaimed_card_body(x,y);
    if(coop_card_no)no=coop_card_no;
