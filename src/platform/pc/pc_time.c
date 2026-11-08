#define _POSIX_C_SOURCE 200809L
#include "pc/pc_platform.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifdef _WIN32
u64 avp_pc_now_ns(void){LARGE_INTEGER freq,now;if(!QueryPerformanceFrequency(&freq)||!QueryPerformanceCounter(&now))return (u64)GetTickCount64()*1000000ull;return (u64)(now.QuadPart/freq.QuadPart)*1000000000ull+(u64)((now.QuadPart%freq.QuadPart)*1000000000ull/freq.QuadPart);}
void avp_pc_sleep_ns(u64 ns){if(ns>=1000000ull)Sleep((DWORD)(ns/1000000ull));else Sleep(0);}
#else
u64 avp_pc_now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (u64)t.tv_sec*1000000000ull+(u64)t.tv_nsec;}
void avp_pc_sleep_ns(u64 ns){struct timespec t;t.tv_sec=(time_t)(ns/1000000000ull);t.tv_nsec=(long)(ns%1000000000ull);while(nanosleep(&t,&t)!=0){} }

#endif
