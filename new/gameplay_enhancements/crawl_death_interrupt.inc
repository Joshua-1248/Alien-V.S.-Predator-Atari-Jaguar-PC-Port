/* PC-only opt-in: interrupt a crawling Alien's native attack on lethal damage.
 * Default preserves the complete committed attack sequence. */
static int host_crawl_death_interrupt;
void avp_amp_set_crawl_death_interrupt(int enabled)
{
    host_crawl_death_interrupt=enabled!=0;
}
