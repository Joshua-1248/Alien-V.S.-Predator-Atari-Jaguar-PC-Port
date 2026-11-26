#ifdef AVP_PC_HOST
/* One-shot exception for the PC's emulated cold-black fields. On the retail
 * 68000 the title score is started in cartridge boot before $01F118 Title;
 * the hosted entry calls NewTitle only AFTER its black startup interval.
 * Restarting there would discard notes already advanced in the preroll. */
static unsigned pc_title_music_prestarted;
/* INTRO returns to the title animation with its score still running.
 * It is not a fresh music selection: do not rewind the live score stream. */
static unsigned pc_title_music_continues_after_attract;
void avp_pc_title_music_prestart(void)
{
    StartMus(AVP_MUSIC_TITLE3);
    pc_title_music_prestarted=1u;
}
#endif

