size_t avp_computer_medical_terminals(unsigned level,u16 *out,size_t capacity)
{
    size_t count=0u;
    if(level<1u || level>15u)return 0u;
    const AvpComputerTerminal *r=level_terms[level-1u];
    for(size_t i=0u;r && i<level_term_count[level-1u];i++)
        if(r[i].handler==AVP_COMP_MEDICAL){
            if(out && count<capacity)out[count]=r[i].comp_offset;
            count++;
        }
    return count;
}
int avp_computer_is_medical_terminal(unsigned level,u16 offset)
{
    if(level<1u||level>15u)return 0;
    const AvpComputerTerminal *r=level_terms[level-1u];
    for(size_t i=0u;r && i<level_term_count[level-1u];i++)
        if(r[i].comp_offset==offset)return r[i].handler==AVP_COMP_MEDICAL;
    return 0;
}
