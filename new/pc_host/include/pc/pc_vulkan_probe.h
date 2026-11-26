#ifndef AVP_PC_VULKAN_PROBE_H
#define AVP_PC_VULKAN_PROBE_H
/* R320: Vulkan device discovery. NO hardware rendering is performed here.
 * This interface does not expose Vulkan objects to the Jaguar simulation. */
#include <stdint.h>

#define AVP_VK_MAX_GPUS 16u
#define AVP_VK_GPU_KEY_LENGTH 64u
#define AVP_VK_GPU_NAME_LENGTH 256u

typedef struct AvpVulkanGpu {
    uint32_t vendor_id, device_id, device_type;
    uint32_t api_version;
    int surface_compatible; /* required WSI surface + swapchain + graphics/present queue */
    char wsi_detail[48]; /* R332: precise capability probe result */
    char name[AVP_VK_GPU_NAME_LENGTH];
    char key[AVP_VK_GPU_KEY_LENGTH]; /* stable best-effort vendor/device/name/duplicate key */
} AvpVulkanGpu;

typedef struct AvpVulkanDevices {
    unsigned count;
    unsigned surface_compatible_count;
    int loader_found, instance_created, surface_created;
    char status[160];
    AvpVulkanGpu gpu[AVP_VK_MAX_GPUS];
} AvpVulkanDevices;

/* native_display: X11 Display* on Linux; native_window: X11 Window (Linux)
 * or HWND (Windows). A null display/window requests device-only discovery.
 * GPU types 1,2,3 are integrated, discrete and virtual; CPU implementations
 * are deliberately excluded from the Hardware-GPU menu.
 * Never silently substitutes the software renderer for a Vulkan backend. */
void avp_vk_enumerate(AvpVulkanDevices *out, void *native_display,
                      uintptr_t native_window);
int avp_vk_find_selected(const AvpVulkanDevices *devices, const char *key);
/* Future Vulkan backend device choice: respect a compatible explicit choice,
 * otherwise prefer discrete, then integrated, then virtual GPU, but ONLY if
 * the device passed the native-window graphics/present/swapchain probe. -1
 * means no eligible GPU; callers MUST keep the Software renderer active. */
int avp_vk_choose_compatible(const AvpVulkanDevices *devices, const char *key);
#endif
