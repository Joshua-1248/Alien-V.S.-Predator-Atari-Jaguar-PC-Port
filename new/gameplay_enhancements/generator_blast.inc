/* PC opt-in: use the barrel's native area-damage path at the generator's
 * existing sixth destruction frame, after the generator is removed.
 * No extra explosion AMP, sound, damage falloff or score attribution. */
static int host_generator_blast_radius;
void avp_amp_set_generator_blast_radius(int enabled)
{
    host_generator_blast_radius=enabled!=0;
}
static int host_generator_blast(const AvpAmp *a)
{
    if(!host_generator_blast_radius)return 0;
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(o->area_damage)o->area_damage(o->user,a->xpos,a->ypos,0x10000u,50,0);
    else AreaDamage(a->xpos,a->ypos,0x10000,50,0);
    return 1;
}
