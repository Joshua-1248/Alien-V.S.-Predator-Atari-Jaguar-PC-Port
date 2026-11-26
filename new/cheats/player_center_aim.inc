/* RE39 R196 optional host aim rules, both off for retail fidelity.
 * The broader option supersedes the melee-specific option. */
static int pc_melee_center_aim,pc_all_center_aim;
void avp_player_set_center_aim_options(int melee,int all_attacks)
{
    pc_melee_center_aim=melee!=0;
    pc_all_center_aim=all_attacks!=0;
}
int avp_player_center_aim_applies(s16 species,s16 weapon)
{
    if(pc_all_center_aim)return 1;
    if(!pc_melee_center_aim)return 0;
    return (species==PT_ALIEN && weapon>=1 && weapon<=3) ||
           (species==PT_PREDATOR && (weapon==1 || weapon==4));
}
