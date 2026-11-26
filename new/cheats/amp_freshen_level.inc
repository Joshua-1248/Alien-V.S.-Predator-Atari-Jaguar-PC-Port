void avp_amp_dev_freshen_current_level(void)
{
    unsigned index;
    if(cur_level<1 || cur_level>AVP_MAX_LEVEL)return;
    index=(unsigned)cur_level-1u;
    levels_visit&=(u16)~(1u<<index);
    saved_levels[index][0].x=-1;
}

