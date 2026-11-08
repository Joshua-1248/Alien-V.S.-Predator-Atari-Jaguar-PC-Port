#ifndef AVP_PC_EEPROM_H
#define AVP_PC_EEPROM_H

#include "avp_types.h"
#include <stddef.h>

int avp_pc_eeprom_open_for_executable(const char *argv0,char *out_path,size_t out_path_cap);
void avp_pc_eeprom_close(void);
u16 avp_pc_eeprom_read_word(u16 index);
void avp_pc_eeprom_write_word(u16 index,u16 value);

#endif
