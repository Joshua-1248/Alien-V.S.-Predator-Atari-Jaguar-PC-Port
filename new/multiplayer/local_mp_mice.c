#include "local_mp_mice.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
unsigned avp_mp_mice_discover(AvpMpPhysicalMouse out[AVP_MP_DISCOVER_MICE])
{
    UINT count=0, i, n=0; RAWINPUTDEVICELIST *list;
    if(!out||GetRawInputDeviceList(NULL,&count,sizeof(RAWINPUTDEVICELIST))==(UINT)-1 || !count)return 0;
    /* Enumeration only; actual WM_INPUT handling is a future host hook. */
    list=(RAWINPUTDEVICELIST*)calloc(count,sizeof(*list));
    if(!list)return 0;
    if(GetRawInputDeviceList(list,&count,sizeof(*list))==(UINT)-1){free(list);return 0;}
    for(i=0;i<count && n<AVP_MP_DISCOVER_MICE;i++)if(list[i].dwType==RIM_TYPEMOUSE){
        UINT chars=AVP_MP_DEVICE_PATH;char path[AVP_MP_DEVICE_PATH];
        if(GetRawInputDeviceInfoA(list[i].hDevice,RIDI_DEVICENAME,path,&chars)==(UINT)-1 || chars>=AVP_MP_DEVICE_PATH)continue;
        memset(&out[n],0,sizeof(out[n]));
        snprintf(out[n].path,sizeof(out[n].path),"%s",path);
        snprintf(out[n].label,sizeof(out[n].label),"Raw Input mouse %u",n+1);
        out[n].readable=0; n++;
    }
    free(list);return n;
}
int avp_mp_mouse_reader_open(AvpMpMouseReader *r,const char *path){(void)r;(void)path;return 0;}
void avp_mp_mouse_reader_close(AvpMpMouseReader *r){if(r)r->fd=-1;}
int avp_mp_mouse_reader_poll(AvpMpMouseReader *r,const AvpMpInputConfig *c,AvpMpMouseState s[AVP_MP_INPUT_PLAYERS])
{(void)r;(void)c;(void)s;return -1;}
#else
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int add_glob(const char *pattern,AvpMpPhysicalMouse out[AVP_MP_DISCOVER_MICE],unsigned n)
{
    glob_t list;size_t i;
    memset(&list,0,sizeof(list));
    if(glob(pattern,0,NULL,&list))return (int)n;
    for(i=0;i<list.gl_pathc && n<AVP_MP_DISCOVER_MICE;i++){
        const char *path=list.gl_pathv[i];struct stat st;int duplicate=0,fd;unsigned j;
        if(strlen(path)>=AVP_MP_DEVICE_PATH || stat(path,&st))continue;
        for(j=0;j<n;j++){
            struct stat prior;
            if(!stat(out[j].path,&prior)&&prior.st_rdev==st.st_rdev){duplicate=1;break;}
        }
        if(duplicate)continue;
        fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        memset(&out[n],0,sizeof(out[n]));
        snprintf(out[n].path,sizeof(out[n].path),"%s",path);
        if(fd>=0){
            char name[sizeof(out[n].label)]={0};
            if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)>=0 && name[0])
                snprintf(out[n].label,sizeof(out[n].label),"%s",name);
            else snprintf(out[n].label,sizeof(out[n].label),"Mouse %u",n+1);
            out[n].readable=1;
            close(fd);
        }else{
            snprintf(out[n].label,sizeof(out[n].label),"Mouse %u (permission denied)",n+1);
        }
        n++;
    }
    globfree(&list);
    return (int)n;
}
unsigned avp_mp_mice_discover(AvpMpPhysicalMouse out[AVP_MP_DISCOVER_MICE])
{
    unsigned n;
    if(!out)return 0;
    memset(out,0,sizeof(*out)*AVP_MP_DISCOVER_MICE);
    n=(unsigned)add_glob("/dev/input/by-id/*-event-mouse",out,0);
    n=(unsigned)add_glob("/dev/input/by-path/*-event-mouse",out,n);
    return n;
}
int avp_mp_mouse_reader_open(AvpMpMouseReader *r,const char *path)
{
    int fd;
    if(!r||!path||!*path||strlen(path)>=AVP_MP_DEVICE_PATH)return 0;
    r->fd=-1;r->path[0]=0;
    fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return 0;
    /* Never treat a regular file as a physical mouse input stream. */
    {struct stat st;unsigned long event_bits[(EV_MAX + (sizeof(unsigned long)*8u)) / (sizeof(unsigned long)*8u)]={0};
     const unsigned bits_per_word=(unsigned)(sizeof(unsigned long)*8u);
     if(fstat(fd,&st) || !S_ISCHR(st.st_mode) ||
        ioctl(fd,EVIOCGBIT(0,sizeof(event_bits)),event_bits)<0 ||
        !(event_bits[EV_REL/bits_per_word]&(1ul<<(EV_REL%bits_per_word))) ||
        !(event_bits[EV_KEY/bits_per_word]&(1ul<<(EV_KEY%bits_per_word)))){
         close(fd);return 0;
     }
    }
    r->fd=fd;memcpy(r->path,path,strlen(path)+1);return 1;
}
void avp_mp_mouse_reader_close(AvpMpMouseReader *r){if(!r)return;if(r->fd>=0)close(r->fd);r->fd=-1;r->path[0]=0;}
int avp_mp_mouse_reader_poll(AvpMpMouseReader *r,const AvpMpInputConfig *cfg,
                             AvpMpMouseState states[AVP_MP_INPUT_PLAYERS])
{
    struct input_event e;int count=0;ssize_t n;
    if(!r||r->fd<0||!cfg||!states)return -1;
    while((n=read(r->fd,&e,sizeof(e)))==(ssize_t)sizeof(e)){
        if(e.type==EV_REL){
            if(e.code==REL_X)count+=avp_mp_input_mouse_motion(cfg,states,r->path,e.value,0);
            else if(e.code==REL_Y)count+=avp_mp_input_mouse_motion(cfg,states,r->path,0,e.value);
        }else if(e.type==EV_KEY){
            unsigned button=0;
            if(e.code==BTN_LEFT)button=1;
            else if(e.code==BTN_RIGHT)button=3;
            else if(e.code==BTN_MIDDLE)button=2;
            else if(e.code==BTN_SIDE)button=8;
            else if(e.code==BTN_EXTRA)button=9;
            if(button)count+=avp_mp_input_mouse_button(cfg,states,r->path,button,e.value!=0);
        }
    }
    if(n<0 && (errno==EAGAIN||errno==EWOULDBLOCK))return count;
    return -1; /* EOF, unplug, short packet or kernel error. */
}
#endif
