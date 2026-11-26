static int hostile_amp_type(const AvpAmp *a)
{
    if(a->flags&(1u<<AMP_PLAYER))return 0;
    switch(a->creature){
    case AC_ALIEN:case AC_PREDATOR:case AC_HUMAN:
    case AC_EGG:case AC_FACEHUG:case AC_ACRAWL:
    case AC_AQUEEN:case AC_GEN:case AC_AQSHIELD:
        return 1;
    default:return 0;
    }
}
