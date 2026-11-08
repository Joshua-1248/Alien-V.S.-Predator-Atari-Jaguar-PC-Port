#include "local_mp_gamepads.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
static const WORD xinput_masks[]={
    XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_B,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER,XINPUT_GAMEPAD_RIGHT_SHOULDER,
    XINPUT_GAMEPAD_BACK,XINPUT_GAMEPAD_START,
    XINPUT_GAMEPAD_LEFT_THUMB,XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_DOWN,
    XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_RIGHT
};
static int xinput_index(const char *path)
{
    int i=-1,n=0;
    if(!path || sscanf(path,"xinput:%d%n",&i,&n)!=1 ||
       n!=(int)strlen(path) || i<0 || i>=4)return -1;
    return i;
}
int avp_mp_pad_reader_open(AvpMpPadReader *r,const char *path)
{
    XINPUT_STATE s;int i=xinput_index(path);
    if(!r || i<0 || XInputGetState((DWORD)i,&s)!=ERROR_SUCCESS)return 0;
    memset(r,0,sizeof(*r));r->fd=-1;
    snprintf(r->path,sizeof(r->path),"xinput:%d",i);
    snprintf(r->state.path,sizeof(r->state.path),"xinput:%d",i);
    return 1;
}
void avp_mp_pad_reader_close(AvpMpPadReader *r)
{if(r){memset(r,0,sizeof(*r));r->fd=-1;}}
int avp_mp_pad_reader_poll(AvpMpPadReader *r,AvpMpControllerSample *out)
{
    XINPUT_STATE s;DWORD index;unsigned b;
    if(!r||!out||r->path[0]==0)return -1;
    index=(DWORD)xinput_index(r->path);
    if(index>=4u || XInputGetState(index,&s)!=ERROR_SUCCESS){
        avp_mp_pad_reader_close(r);memset(out,0,sizeof(*out));return -1;
    }
    r->state.axis_count=6;
    r->state.axes[0]=s.Gamepad.sThumbLX;
    r->state.axes[1]=s.Gamepad.sThumbLY==-32768?32767:(int16_t)-s.Gamepad.sThumbLY;
    r->state.axes[2]=s.Gamepad.sThumbRX;
    r->state.axes[3]=s.Gamepad.sThumbRY==-32768?32767:(int16_t)-s.Gamepad.sThumbRY;
    r->state.axes[4]=(int16_t)(s.Gamepad.bRightTrigger*128);
    r->state.axes[5]=(int16_t)(s.Gamepad.bLeftTrigger*128);
    r->state.buttons=0;
    for(b=0;b<14;b++)if(s.Gamepad.wButtons&xinput_masks[b])
        r->state.buttons|=1u<<b;
    if(s.Gamepad.bLeftTrigger>=160)r->state.buttons|=1u<<14;
    if(s.Gamepad.bRightTrigger>=160)r->state.buttons|=1u<<15;
    *out=r->state;return 0;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
int avp_mp_pad_reader_open(AvpMpPadReader *r,const char *path)
{
    int fd;struct stat st;int version=0;
    unsigned char axes=0;
    if(!r||!path||!*path||strlen(path)>=AVP_MP_DEVICE_PATH)return 0;
    fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return 0;
    if(fstat(fd,&st)||!S_ISCHR(st.st_mode)||ioctl(fd,JSIOCGVERSION,&version)<0){
        close(fd);return 0;
    }
    memset(r,0,sizeof(*r));r->fd=fd;
    memcpy(r->path,path,strlen(path)+1);
    memcpy(r->state.path,path,strlen(path)+1);
    (void)ioctl(fd,JSIOCGAXES,&axes);
    r->state.axis_count=axes>AVP_MP_COMMAND_AXES?AVP_MP_COMMAND_AXES:axes;
    return 1;
}
void avp_mp_pad_reader_close(AvpMpPadReader *r)
{
    if(!r)return;
    if(r->fd>=0)close(r->fd);
    memset(r,0,sizeof(*r));r->fd=-1;
}
int avp_mp_pad_reader_poll(AvpMpPadReader *r,AvpMpControllerSample *out)
{
    struct js_event e;ssize_t n;
    if(!r||!out||r->fd<0)return -1;
    for(;;){
        n=read(r->fd,&e,sizeof(e));
        if(n<0 && errno==EINTR)continue;
        if(n!=(ssize_t)sizeof(e))break;
        {
            unsigned type=(unsigned)e.type&~JS_EVENT_INIT;
            if(type==JS_EVENT_AXIS && e.number<AVP_MP_COMMAND_AXES){
                r->state.axes[e.number]=e.value;
            }else if(type==JS_EVENT_BUTTON && e.number<32u){
                if(e.value)r->state.buttons|=1u<<e.number;
                else r->state.buttons&=~(1u<<e.number);
            }
        }
    }
    if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){
        *out=r->state;return 0;
    }
    avp_mp_pad_reader_close(r);memset(out,0,sizeof(*out));return -1;
}
#endif
