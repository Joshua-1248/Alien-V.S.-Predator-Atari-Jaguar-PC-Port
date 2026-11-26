/* R322 standalone Vulkan Xlib diagnostic. No gameplay frame is submitted.
 * Without arguments: retain the original R321 one-shot clear/present.
 * --loop 180 [GPU_KEY]: opt-in persistent GPU clears, single-flight fences,
 * per-image presentation semaphores, and swapchain resize handling. */
#include "pc/pc_vulkan_wsi.h"
#include <X11/Xlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    Display *dpy = NULL;
    Window win = 0;
    AvpVkWsiSmoke report;
    unsigned long loop_count = 0ul;
    int shader_test = 0,ray_columns = 0,projected_wall = 0,two_slot = 0,mixed_wall = 0;
    const char *vertex_spv = NULL, *fragment_spv = NULL;
    const char *preferred = NULL;
    int ok;
    if (argc > 1 && (strcmp(argv[1], "--shader-test") == 0 ||
                     strcmp(argv[1], "--ray-columns") == 0 ||
                     strcmp(argv[1], "--projected-wall") == 0 ||
                     strcmp(argv[1], "--two-slot-wall") == 0 ||
                     strcmp(argv[1], "--mixed-cry-rgb-wall") == 0)) {
        ray_columns = strcmp(argv[1], "--ray-columns") == 0;
        projected_wall = strcmp(argv[1], "--projected-wall") == 0;
        two_slot = strcmp(argv[1], "--two-slot-wall") == 0;
        mixed_wall = strcmp(argv[1], "--mixed-cry-rgb-wall") == 0;
        if (argc < 4 || argc > 5) {
            fprintf(stderr,"Usage: %s --shader-test|--ray-columns|--projected-wall|--two-slot-wall|--mixed-cry-rgb-wall VERT.spv FRAG.spv [GPU_KEY]\n",argv[0]);
            return EXIT_FAILURE;
        }
        shader_test=1;
        vertex_spv=argv[2]; fragment_spv=argv[3];
        preferred=argc==5?argv[4]:NULL;
    } else if (argc > 1 && strcmp(argv[1], "--loop") == 0) {
        char *end = NULL;
        if (argc < 3 || argc > 4) {
            fprintf(stderr, "Usage: %s --loop FRAMES(1..4096) [GPU_KEY]\n", argv[0]);
            return EXIT_FAILURE;
        }
        errno = 0;
        loop_count = strtoul(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || !loop_count || loop_count > 4096ul) {
            fprintf(stderr, "Invalid diagnostic frame count (1..4096)\n");
            return EXIT_FAILURE;
        }
        preferred = argc == 4 ? argv[3] : NULL;
    } else if (argc <= 2) {
        preferred = argc == 2 ? argv[1] : NULL;
    } else {
        fprintf(stderr, "Usage: %s [GPU_KEY] or --loop FRAMES [GPU_KEY]\n", argv[0]);
        return EXIT_FAILURE;
    }
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Vulkan diagnostic: X11 display unavailable\n"); return 77; }
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 50, 50, 640, 480,
            0u, BlackPixel(dpy, DefaultScreen(dpy)), BlackPixel(dpy, DefaultScreen(dpy)));
    if (!win) { XCloseDisplay(dpy); return 77; }
    XStoreName(dpy, win, "AVP Vulkan diagnostic - NOT gameplay");
    XMapRaised(dpy, win);
    XSync(dpy, False);
    /* Transfer the diagnostic's private X11 Display/Window to the Vulkan
     * runner. It closes the X11 display while the final VkInstance is still
     * alive, preventing a dangling ICD close-display hook. */
    AvpVkWsiDiagnosticMode mode = AVP_VK_WSI_DIAG_CLEAR;
    if (shader_test) {
        if (mixed_wall) mode = AVP_VK_WSI_DIAG_MIXED_CRY_RGB;
        else if (two_slot) mode = AVP_VK_WSI_DIAG_TWO_SLOT;
        else if (projected_wall) mode = AVP_VK_WSI_DIAG_PROJECTED_WALL;
        else if (ray_columns) mode = AVP_VK_WSI_DIAG_RAY_COLUMNS;
        else mode = AVP_VK_WSI_DIAG_SHADER;
    } else if (loop_count) mode = AVP_VK_WSI_DIAG_LOOP;
    ok = avp_vk_wsi_run_owned_xlib_diagnostic(dpy, (uintptr_t)win,
            640u, 480u, preferred, mode, (uint32_t)loop_count,
            vertex_spv, fragment_spv, &report);
    dpy = NULL; /* consumed on success and failure */
    win = 0;
    fprintf(stderr, "Vulkan diagnostic: %s; GPU: %s; submitted=%u; presented=%u; resize=%u; GPU source uploads=%u (%u bytes); geometry=%u bytes\n",
        report.detail, report.device_name[0] ? report.device_name : "none",
        report.frames_submitted, report.frames_presented, report.swapchain_recreations,
        report.textures_uploaded, report.texture_bytes_uploaded,
        report.geometry_bytes_uploaded);
    /* Do not conceal runtime/synchronization errors as SKIP. Only a missing
     * instance/eligible device (or absent X server above) is an unavailable
     * test environment. Fail CI if a chosen Vulkan device malfunctions. */
    if (ok) return EXIT_SUCCESS;
    return report.device_created ? EXIT_FAILURE : 77;
}
