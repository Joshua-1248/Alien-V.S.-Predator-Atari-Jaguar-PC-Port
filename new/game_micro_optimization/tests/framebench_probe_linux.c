#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <link.h>
#include <unistd.h>
static double start;
static unsigned count;
static uintptr_t base;
#define NBUCKETS (4u*1024u*1024u/64u)
static unsigned hist[NBUCKETS];
static int sample;
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec*1e-9;}
static int main_base(struct dl_phdr_info *i,size_t s,void *p){(void)s;(void)p;if(!i->dlpi_name[0]){base=i->dlpi_addr;return 1;}return 0;}
static void tick(int sig,siginfo_t *si,void *ctx){(void)sig;(void)si;uintptr_t ip=(uintptr_t)((ucontext_t*)ctx)->uc_mcontext.gregs[REG_RIP];if(ip>=base && (ip-base)/64<NBUCKETS)__atomic_fetch_add(&hist[(ip-base)/64],1,__ATOMIC_RELAXED);}
static void frame(unsigned w,unsigned h){if(w<300||h<200)return;double t=now();if(!start){start=t;return;}if(t-start<3.0)return;if(!count && sample){struct sigaction a={.sa_sigaction=tick,.sa_flags=SA_SIGINFO|SA_RESTART};sigemptyset(&a.sa_mask);sigaction(SIGPROF,&a,0);struct itimerval iv={{0,1000},{0,1000}};setitimer(ITIMER_PROF,&iv,0);}count++;if(t-start>=9.0){double elapsed=t-start-3.0;fprintf(stderr,"FRAMEBENCH frames=%u seconds=%.6f fps=%.3f image=%ux%u\n",count,elapsed,count/elapsed,w,h);if(sample){struct itimerval iv={{0,0},{0,0}};setitimer(ITIMER_PROF,&iv,0);for(unsigned i=0;i<NBUCKETS;i++)if(hist[i])fprintf(stderr,"SAMPLE %x %u\n",i*64,hist[i]);}fflush(stderr);_exit(0);}}
__attribute__((constructor))static void init(void){sample=getenv("FRAMEBENCH_SAMPLE")!=NULL;dl_iterate_phdr(main_base,0);}
Bool XShmPutImage(Display*d,Drawable dr,GC gc,XImage*i,int sx,int sy,int dx,int dy,unsigned w,unsigned h,Bool event){static Bool(*fn)(Display*,Drawable,GC,XImage*,int,int,int,int,unsigned,unsigned,Bool);if(!fn)fn=dlsym(RTLD_NEXT,"XShmPutImage");Bool r=fn(d,dr,gc,i,sx,sy,dx,dy,w,h,event);if(r)frame(w,h);return r;}
int XPutImage(Display*d,Drawable dr,GC gc,XImage*i,int sx,int sy,int dx,int dy,unsigned w,unsigned h){static int(*fn)(Display*,Drawable,GC,XImage*,int,int,int,int,unsigned,unsigned);if(!fn)fn=dlsym(RTLD_NEXT,"XPutImage");int r=fn(d,dr,gc,i,sx,sy,dx,dy,w,h);frame(w,h);return r;}
