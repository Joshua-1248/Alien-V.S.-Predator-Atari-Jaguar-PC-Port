#ifndef AVP_PC_JOYBINDING_H
#define AVP_PC_JOYBINDING_H
#include <stdint.h>
/* Nonnegative values retain existing /dev/input/jsN button IDs; -1 remains
 * unbound.  -2,-3 encode negative/positive axis 0, -4,-5 axis 1, etc. */
static inline int avp_pc_axis_button_token(int axis,int positive)
{ return -2-2*axis-(positive?1:0); }
static inline int avp_pc_binding_down(int token,const int16_t *axes,int axis_count,
                                      const unsigned char *buttons,int button_count)
{
    if(token>=0)return buttons&&token<button_count&&buttons[token]!=0;
    if(token==-1||!axes)return 0;
    /* Tokens come from settings.cfg as well as the capture UI. Decode in
     * 64 bits: negating INT_MIN as an int is undefined behavior, and an
     * out-of-range token must never index the axis array. */
    {int64_t n=-(int64_t)token-2;
     int64_t a=n/2;
     int positive=(int)(n&1);
     if(n<0||a>=axis_count)return 0;
     return positive ? axes[a]>=19660 : axes[a]<=-19660;
    }
}
static inline int avp_pc_binding_edge(int token,const int16_t *axes,const int16_t *previous_axes,int axis_count,
                                      const unsigned char *buttons,const unsigned char *previous_buttons,int button_count)
{
    return avp_pc_binding_down(token,axes,axis_count,buttons,button_count)&&
           !avp_pc_binding_down(token,previous_axes,axis_count,previous_buttons,button_count);
}
#endif
