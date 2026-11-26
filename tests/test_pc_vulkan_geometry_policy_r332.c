/* No Vulkan SDK required. Every assertion is enabled even in Release. */
#include "pc/pc_vulkan_geometry_policy.h"
#include "jaguar/avp_gpu/world_scene_gpu_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static void w32(uint8_t *p,uint32_t v)
{ for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8)); }
static void f64(uint8_t *p,double d)
{ uint64_t bits;memcpy(&bits,&d,8);w32(p,(uint32_t)bits);w32(p+4,(uint32_t)(bits>>32)); }
static void make(uint8_t b[136])
{
    uint8_t *c=b+32,*h=b+56;
    memset(b,0,136);
    w32(b,AVP_WORLD_GPU_WIRE_MAGIC);w32(b+4,AVP_WORLD_GPU_WIRE_VERSION);
    w32(b+8,1);w32(b+12,1080);w32(b+16,2);w32(b+20,32);w32(b+24,56);
    w32(c,0);w32(c+4,2);f64(c+8,8.25);w32(c+16,1);
    f64(h,0.5);f64(h+8,0.25);w32(h+16,12);w32(h+20,4);
    w32(h+24,1);w32(h+28,0);w32(h+32,(uint32_t)-12);w32(h+36,700);
    h+=40;f64(h,2.0);f64(h+8,0.75);w32(h+16,21);w32(h+20,8);
    w32(h+24,0);w32(h+28,1);w32(h+32,13);w32(h+36,300);
}
int main(void)
{
    uint8_t b[136],changed[136];
    uint32_t flags[3]={0u,0x6u,0x1u},idx=99u;
    assert(sizeof(double)==8);
    make(b);
    assert(avp_vk_geometry_wire_valid(b,sizeof(b)));
    assert(!avp_vk_geometry_wire_valid(b,sizeof(b)-1));
    assert(!avp_vk_geometry_wire_valid(b,0));
    assert(!avp_vk_geometry_wire_valid(NULL,sizeof(b)));
    /* Independent corruption cases must be rejected, not silently truncated. */
    const size_t bad_positions[]={0,4,8,12,16,20,24,28,32,36,48,52,56+16,56+20,56+24,96+28};
    const uint32_t bad_values[]={0,2,0,0,3,36,60,1,1,3,0,1,256,256,2,2};
    for(size_t i=0;i<sizeof(bad_positions)/sizeof(*bad_positions);i++){
        memcpy(changed,b,sizeof(b));w32(changed+bad_positions[i],bad_values[i]);
        assert(!avp_vk_geometry_wire_valid(changed,sizeof(changed)));
    }
    memcpy(changed,b,sizeof(b));f64(changed+56,NAN);
    assert(!avp_vk_geometry_wire_valid(changed,sizeof(changed)));
    memcpy(changed,b,sizeof(b));f64(changed+56+8,INFINITY);
    assert(!avp_vk_geometry_wire_valid(changed,sizeof(changed)));
    assert(avp_vk_geometry_choose_memory(0x2u,0x6u,flags,3,&idx)&&idx==1u);
    idx=99u;assert(!avp_vk_geometry_choose_memory(0x4u,0x6u,flags,3,&idx)&&idx==99u);
    assert(!avp_vk_geometry_choose_memory(0,0,NULL,3,&idx));
    assert(!avp_vk_geometry_choose_memory(0,0,flags,33,&idx));
    puts("R332 geometry wire validation: exact layout, corruption rejection, memory selection PASS");
    return 0;
}
