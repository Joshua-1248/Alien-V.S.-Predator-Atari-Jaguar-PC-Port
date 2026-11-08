/*
 * Test-only Musashi translation-unit shim.  The original m68kcpu.c invokes
 * M68K_INSTRUCTION_CALLBACK(pc) but does not declare that external callback.
 * GCC historically allowed this implicit declaration; Clang 17+ refuses it.
 *
 * Keep the third-party emulator untouched.  Include its implementation only
 * in this separate test-only target after providing the correct signature.
 */
extern void avp_m68k_instr_hook(unsigned int pc);
#include "m68kcpu.c"
