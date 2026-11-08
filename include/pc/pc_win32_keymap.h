#ifndef AVP_PC_WIN32_KEYMAP_H
#define AVP_PC_WIN32_KEYMAP_H
/* Translate Win32 virtual keys to the same X11 KeySym numbers used by the
 * Linux input path. This translator is platform-independent so it can be
 * regression-tested without requiring a Windows SDK on the host. */
unsigned long avp_pc_win32_keysym(unsigned virtual_key,unsigned scan_code,
                                   int shift_down,int extended);
unsigned char avp_pc_win32_keycode(unsigned scan_code,int extended);
#endif
