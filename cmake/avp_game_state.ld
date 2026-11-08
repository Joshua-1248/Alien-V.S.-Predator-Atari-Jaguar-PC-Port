/* Session quicksave range.  This gathers writable state from the reconstructed
 * CPU/Jaguar game library only.  X11, ALSA, pthread state and ROM/resource
 * ownership live in the executable and are deliberately outside this range. */
SECTIONS
{
  .avp_game_state : ALIGN(16)
  {
    __avp_game_state_start = .;
    libavp_pc_core.a:*(.data .data.* .bss .bss.* COMMON)
    __avp_game_state_end = .;
  }
}
INSERT AFTER .data;
