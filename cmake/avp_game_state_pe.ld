/* Windows (PE/COFF, GNU ld) session quicksave range.  Same rule as the ELF
 * script: only writable state of the reconstructed CPU/Jaguar game library is
 * gathered; Win32 window/audio/XInput state, CRT state and ROM/resource
 * ownership stay outside.  PE input sections use the .data$suffix grouping. */
SECTIONS
{
  .avpgs BLOCK(__section_alignment__) :
  {
    __avp_game_state_start = .;
    *libavp_pc_core.a:*(.data .data$* .bss .bss$* COMMON)
    __avp_game_state_end = .;
  }
}
INSERT AFTER .data;
