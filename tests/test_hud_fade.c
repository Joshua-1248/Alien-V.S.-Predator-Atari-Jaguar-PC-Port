#include "jaguar/avp_gpu/hud_fade.h"
#include <assert.h>
int main(void)
{
    /* The original Predator picture stores the eye and medpak pixels at
     * HUD-local coordinates. Zero pixels are transparent in both regions. */
    assert(avp_gpu_predator_faded_icon(0u,202,164,6,6)==0u);
    assert(avp_gpu_predator_faded_icon(225u,202,164,0,6)==225u);
    assert(avp_gpu_predator_faded_icon(225u,202,164,4,0)==229u);
    assert(avp_gpu_predator_faded_icon(225u,227,192,6,0)==231u);
    assert(avp_gpu_predator_faded_icon(225u,228,164,6,0)==225u);
    assert(avp_gpu_predator_faded_icon(225u,202,193,6,0)==225u);
    assert(avp_gpu_predator_faded_icon(225u,31,75,6,3)==228u);
    assert(avp_gpu_predator_faded_icon(225u,65,128,0,6)==231u);
    assert(avp_gpu_predator_faded_icon(225u,66,75,0,6)==225u);
    assert(avp_gpu_predator_faded_icon(225u,31,129,0,6)==225u);
    assert(avp_gpu_predator_faded_icon(225u,202,164,99,0)==231u);
    assert(avp_gpu_predator_faded_icon(225u,202,164,-5,0)==225u);
    return 0;
}
