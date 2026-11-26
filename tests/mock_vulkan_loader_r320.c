/* Test-only Vulkan driver fixture, independently written. This is not a game
 * renderer, not shipped in release executables, and does not emulate a GPU. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef void (*Proc)(void);
typedef struct {char name[256];uint32_t version;} Ext;
typedef struct {uint32_t flags,count,timestamp,granularity[3];} Queue;
static int mock_instance;
static int mock_instance_balance;
int avp_test_vk_instance_balance(void) { return mock_instance_balance; }
static int mock_devices[3];
static const char *instance_extensions[]={"VK_KHR_surface","VK_KHR_xlib_surface"};
int32_t vkCreateInstance(const void *info,const void *alloc,void **out)
{(void)info;(void)alloc;*out=&mock_instance;mock_instance_balance++;return 0;}
void vkDestroyInstance(void *inst,const void *alloc){(void)inst;(void)alloc;mock_instance_balance--;}
int32_t vkEnumerateInstanceExtensionProperties(const char *layer,uint32_t *n,Ext *arr)
{(void)layer;if(!arr){*n=2;return 0;}for(uint32_t i=0;i<*n&&i<2u;i++){
 memset(&arr[i],0,sizeof(arr[i]));strncpy(arr[i].name,instance_extensions[i],255);
}*n=2;return 0;}
int32_t vkEnumeratePhysicalDevices(void *inst,uint32_t *n,void **out)
{(void)inst;if(!out){*n=3;return 0;}for(uint32_t i=0;i<*n&&i<3;i++)out[i]=&mock_devices[i];*n=3;return 0;}
void vkGetPhysicalDeviceProperties(void *pd,void *out)
{
    /* Published Vulkan 1.0 prefix: five 32-bit words + 256-byte name. */
    uint8_t *p=(uint8_t *)out;
    int i=(int *)pd-mock_devices;
    uint32_t prefix[5]={ (1u<<22),1u, i==0?0x10005u:0x1002u,
                          0x4000u+(uint32_t)i, i==0?4u:(i==1?1u:2u)};
    const char *name=i==0?"Mock CPU Vulkan":i==1?"Mock Integrated GPU":"Mock Discrete GPU";
    memcpy(p,prefix,sizeof(prefix));strcpy((char*)p+20,name);
}
void vkGetPhysicalDeviceQueueFamilyProperties(void *pd,uint32_t *n,Queue *out)
{(void)pd;if(!out){*n=1;return;}*n=1;memset(out,0,sizeof(*out));out[0].flags=1u;out[0].count=1u;}
int32_t vkEnumerateDeviceExtensionProperties(void *pd,const char *layer,uint32_t *n,Ext *out)
{
    int i=(int *)pd-mock_devices;
    (void)layer;
    /* R333: the mock discrete GPU exposes swapchain at extension index 176.
     * R332 incorrectly rejected its entire list because it exceeds 128. */
    if(!out){*n=(i==2)?177u:0u;return 0;}
    if(i==2){
        const uint32_t supplied=*n<177u?*n:177u;
        for(uint32_t j=0;j<supplied;j++){
            memset(&out[j],0,sizeof(out[j]));
            strcpy(out[j].name,j==176u?"VK_KHR_swapchain":
                                   "VK_EXT_r333_test_placeholder");
        }
        *n=177u;
    }else *n=0u;
    return 0;
}
int32_t vkCreateXlibSurfaceKHR(void *inst,const void *info,const void *alloc,uint64_t *surface)
{(void)inst;(void)info;(void)alloc;*surface=77u;return 0;}
void vkDestroySurfaceKHR(void *inst,uint64_t surface,const void *alloc)
{(void)inst;(void)surface;(void)alloc;}
int32_t vkGetPhysicalDeviceSurfaceSupportKHR(void *pd,uint32_t family,uint64_t surface,uint32_t *supported)
{(void)family;(void)surface;*supported=(pd==&mock_devices[2]);return 0;}
Proc vkGetInstanceProcAddr(void *instance,const char *name)
{
    (void)instance;
#define LOOKUP(n) if(!strcmp(name,#n))return (Proc)n
    LOOKUP(vkCreateInstance);LOOKUP(vkDestroyInstance);
    LOOKUP(vkEnumerateInstanceExtensionProperties);LOOKUP(vkEnumeratePhysicalDevices);
    LOOKUP(vkGetPhysicalDeviceProperties);LOOKUP(vkGetPhysicalDeviceQueueFamilyProperties);
    LOOKUP(vkEnumerateDeviceExtensionProperties);LOOKUP(vkCreateXlibSurfaceKHR);
    LOOKUP(vkDestroySurfaceKHR);LOOKUP(vkGetPhysicalDeviceSurfaceSupportKHR);
#undef LOOKUP
    return NULL;
}
