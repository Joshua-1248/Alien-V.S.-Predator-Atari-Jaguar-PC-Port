#ifndef AVP_WIN32_POSIX_H
#define AVP_WIN32_POSIX_H
/* Local Win32 replacements for the small POSIX host-surface the Linux PC
   main loop uses. Paths are returned with forward slashes so existing ROM,
   settings, EEPROM and cache path handling stays platform-neutral. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define ScreenOff avp_winnt_ScreenOff /* winnt.h enumerator vs game routine */
#include <windows.h>
#undef ScreenOff
#include <direct.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&_S_IFMT)==_S_IFREG)
#endif
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define readlink avp_win32_readlink
static inline ssize_t avp_win32_readlink(const char *name,char *out,size_t cap)
{
    char path[32768];DWORD n;size_t i;
    if(strcmp(name,"/proc/self/exe")!=0||!out||cap<2)return -1;
    n=GetModuleFileNameA(NULL,path,(DWORD)sizeof(path));
    if(!n||n>=sizeof(path)||n>=cap)return -1;
    for(i=0;i<(size_t)n;i++)out[i]=path[i]=='\\' ? '/' : path[i];
    return (ssize_t)n;
}
#define mkdir(path,mode) _mkdir(path)
#endif
#endif
