#ifndef AVP_PC_HYBRID_SHADER_PATHS_H
#define AVP_PC_HYBRID_SHADER_PATHS_H
/* R368: locate a complete, external Hybrid SPIR-V shader set. No Vulkan SDK
 * types are used here. Content/stage validation remains shader-cache owned.
 * The optional executable's packaged sibling shaders/ directory wins over
 * the developer build-directory fallback; a nonempty override is strict. */
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_HYBRID_SHADER_PATH_CAP 1024u
typedef struct AvpHybridShaderPaths {
    char vertex[AVP_HYBRID_SHADER_PATH_CAP];
    char world[AVP_HYBRID_SHADER_PATH_CAP];
    char foreground[AVP_HYBRID_SHADER_PATH_CAP];
} AvpHybridShaderPaths;
/* executable_path must point to the actual executable, NOT the CWD.
 * If override_dir is nonempty, ONLY that directory is checked. Returns zero
 * and clears all output strings if no complete readable shader set exists. */
int avp_hybrid_shader_paths_find(const char *override_dir,
    const char *executable_path,const char *build_dir,
    AvpHybridShaderPaths *paths);
#ifdef __cplusplus
}
#endif
#endif
