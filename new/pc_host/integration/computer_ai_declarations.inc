/* Read-only retail terminal metadata for AI path decisions. Offsets are
 * original DOORS.S panel byte offsets; never invent terminals or healing. */
size_t avp_computer_medical_terminals(unsigned level,u16 *out,size_t capacity);
int avp_computer_is_medical_terminal(unsigned level,u16 offset);
int avp_computer_first_aid_ceiling(unsigned security);
