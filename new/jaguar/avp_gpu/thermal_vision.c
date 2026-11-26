#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include "jaguar/avp_gpu/host_distance.h"
#include <float.h>
#include <math.h>

int avp_thermal_is_mode(int enabled,int species,unsigned cloak_state,
                        u16 vmode,u16 xfade)
{
    /* RE39 R162: the ORIGINAL retail table words are correct, but R161
     * misnamed their on-screen output. The user's three same-scene screenshots
     * show blue Thermal at {RGB16, X_FADE=0x26f8}; applying the Jaguar
     * X_FADE modifier to the third (unmodified) screenshot reproduces the
     * blue frame's pixel-word distribution. No original filter is changed. */
    return enabled && species==PT_PREDATOR && cloak_state==2u &&
           vmode==0x06c7u && xfade==0x26f8u;
}
int avp_electromagnetic_is_mode(int enabled,int species,unsigned cloak_state,
                         u16 vmode,u16 xfade)
{
    /* RE39 R162: the pink/yellow Electromagnetic screenshot is the RGB16
     * {0x06c7,0x9558} output. R161's {RGB24,0x8880} predicate selected the
     * GREEN Night Vision screenshot instead. Both now share the original
     * 320-word RGB16 compositor, with separate palettes and enable flags. */
    return enabled && species==PT_PREDATOR && cloak_state==2u &&
           vmode==0x06c7u && xfade==0x9558u;
}
int avp_infrared_is_mode(int enabled,int species,unsigned cloak_state,
                        u16 vmode,u16 xfade)
{
    /* R164: retail RDB.DTA final entry; screenshot-identified pink/yellow
     * Infrared, NOT RGB24 Night Vision or blue Thermal. */
    return enabled && species==PT_PREDATOR && cloak_state==2u &&
           vmode==0x06c7u && xfade==0x8880u;
}
int avp_predator_vision_style_ex(int thermal_enabled,int electromagnetic_enabled,
                                int infrared_enabled,int species,unsigned cloak_state,
                                u16 vmode,u16 xfade)
{
    if(avp_thermal_is_mode(thermal_enabled,species,cloak_state,vmode,xfade))
        return AVP_VISION_ENHANCED_THERMAL;
    if(avp_electromagnetic_is_mode(electromagnetic_enabled,species,cloak_state,vmode,xfade))
        return AVP_VISION_ENHANCED_ELECTROMAGNETIC;
    if(avp_infrared_is_mode(infrared_enabled,species,cloak_state,vmode,xfade))
        return AVP_VISION_ENHANCED_INFRARED;
    return AVP_VISION_ORIGINAL;
}
int avp_predator_vision_style(int thermal_enabled,int electromagnetic_enabled,
                             int species,unsigned cloak_state,u16 vmode,u16 xfade)
{
    return avp_predator_vision_style_ex(thermal_enabled,electromagnetic_enabled,0,
                                       species,cloak_state,vmode,xfade);
}

/* RE39 R170: scenery-only Predator enhancement. Thermal backgrounds now
 * retain their original Jaguar filtering even when this toggle is enabled;
 * only Electromagnetic and Infrared backgrounds are enhanced. Living-actor
 * heat LUTs and the three independent actor toggles remain unaffected. */
int avp_predator_background_style(int enabled,int species,unsigned cloak_state,
                                   u16 vmode,u16 xfade)
{
    if(!enabled)return AVP_VISION_ORIGINAL;
    return avp_predator_vision_style_ex(0,1,1,species,cloak_state,
                                        vmode,xfade);
}
static unsigned avp_pred_bg_clamp(int v)
{
    return v<=0?0u:v>=255?255u:(unsigned)v;
}
u32 avp_predator_background_color(unsigned style,u32 original,
                                   unsigned native_x,unsigned native_y)
{
    const unsigned r=(original>>16)&255u,g=(original>>8)&255u,b=original&255u;
    const unsigned luma=(54u*r+183u*g+19u*b+128u)/256u;
    unsigned level,red,green,blue;
    switch(style){
    case AVP_VISION_ENHANCED_THERMAL:
        /* R169: subtle blue-only brightness lift, +12 on the 8-bit blue
         * channel over R168. Preserve red/green and luma-driven texture
         * contrast, without introducing new noise or affecting actors. */
        level=avp_pred_bg_clamp(20+(int)(luma*108u/100u));
        red=3u+(level*7u)/100u;
        green=18u+(level*30u)/100u;
        blue=70u+(level*68u)/100u;
        break;
    case AVP_VISION_ENHANCED_ELECTROMAGNETIC:
        {
            /* RE39 R170: scenery-only blue-black / violet / magenta contrast,
             * calibrated against the user's supplied reference photograph.
             * Original texture luminance determines the color; unlike R166/167
             * there is NO position-dependent noise or splotch pattern. Dark
             * surfaces remain blue-black while edges and lit texture details
             * can reach luminous violet/fuchsia. Do not affect actor spectra. */
            static const struct {unsigned i,r,g,b;} stops[]={
                {0u,2u,0u,6u}, {30u,4u,0u,29u},
                {65u,10u,0u,71u}, {100u,26u,0u,127u},
                {135u,55u,0u,183u}, {170u,103u,0u,220u},
                {205u,163u,8u,247u}, {234u,209u,26u,251u},
                {255u,243u,57u,255u}
            };
            unsigned k;
            /* Bias the low end toward black while opening up mid/high
             * contrast, rather than washing every surface bright purple. */
            level=luma<=35u?0u:avp_pred_bg_clamp((int)((luma-35u)*255u/180u));
            for(k=1u;k<sizeof(stops)/sizeof(stops[0]);k++)
                if(level<=stops[k].i)break;
            if(k>=sizeof(stops)/sizeof(stops[0]))k=(unsigned)(sizeof(stops)/sizeof(stops[0])-1u);
            {
                const unsigned span=stops[k].i-stops[k-1u].i;
                const unsigned offset=level-stops[k-1u].i;
                red=(stops[k-1u].r*(span-offset)+stops[k].r*offset+span/2u)/span;
                green=(stops[k-1u].g*(span-offset)+stops[k].g*offset+span/2u)/span;
                blue=(stops[k-1u].b*(span-offset)+stops[k].b*offset+span/2u)/span;
            }
            (void)native_x;(void)native_y;
            break;
        }
    case AVP_VISION_ENHANCED_INFRARED:
        {
            /* R174: keep Infrared scenery cooler/purpler through the lower and
             * middle range, so the scene reads with "purple farther, red/pink
             * nearer" emphasis. Because there is no true depth pass for static
             * scenery here, use the framebuffer's own brightness as the cue:
             * ordinary/darker walls and floors remain violet, while brighter
             * surfaces climb into pink/red-magenta later in the curve. No
             * positional patterning or actor-spectrum changes. */
            static const struct {unsigned i,r,g,b;} stops[]={
                {0u,6u,1u,34u}, {40u,20u,2u,70u},
                {80u,60u,5u,128u}, {115u,110u,8u,162u},
                {145u,165u,14u,176u}, {170u,214u,26u,164u},
                {195u,240u,48u,150u}, {225u,252u,82u,145u},
                {255u,255u,120u,150u}
            };
            unsigned k;
            level=luma;
            for(k=1u;k<sizeof(stops)/sizeof(stops[0]);k++)
                if(level<=stops[k].i)break;
            if(k>=sizeof(stops)/sizeof(stops[0]))k=(unsigned)(sizeof(stops)/sizeof(stops[0])-1u);
            {
                const unsigned span=stops[k].i-stops[k-1u].i;
                const unsigned offset=level-stops[k-1u].i;
                red=(stops[k-1u].r*(span-offset)+stops[k].r*offset+span/2u)/span;
                green=(stops[k-1u].g*(span-offset)+stops[k].g*offset+span/2u)/span;
                blue=(stops[k-1u].b*(span-offset)+stops[k].b*offset+span/2u)/span;
            }
            (void)native_x;(void)native_y;
            break;
        }
    default:return original;
    }
    return (avp_pred_bg_clamp((int)red)<<16)|
           (avp_pred_bg_clamp((int)green)<<8)|avp_pred_bg_clamp((int)blue);
}
void avp_predator_background_composite_rgb16(u32 *scene,const u32 *mask,
                                   unsigned width,unsigned height,unsigned style)
{
    if(!scene||!mask||!width||!height||
       style<AVP_VISION_ENHANCED_THERMAL||style>AVP_VISION_ENHANCED_INFRARED)return;
    /* Match the Jaguar RGB16 output: a sampled line-buffer word occupies a
     * contiguous output span. Masking the SOURCE sample avoids coloring HUD,
     * weapons, pickups, projectiles, or actor sprites at any host resolution.
     * Inverse bounds and source columns are independent of Y: compute once. */
    unsigned bounds[321],samples[320];
    for(unsigned i=0;i<=320u;i++)
        bounds[i]=(unsigned)(((uint64_t)i*width+319u)/320u);
    for(unsigned i=0;i<320u;i++){
        unsigned sx=(unsigned)(((uint64_t)(2u*i+1u)*width)/640u);
        samples[i]=sx<width?sx:width-1u;
    }
    for(unsigned y=0;y<height;y++){
        u32 *row=scene+(size_t)y*width;
        const u32 *protect=mask+(size_t)y*width;
        const unsigned native_y=(unsigned)(((uint64_t)y*228u)/height);
        for(unsigned i=0;i<320u;i++){
            const unsigned x0=bounds[i],x1=bounds[i+1u];
            if(x0==x1||protect[samples[i]])continue;
            const u32 ink=avp_predator_background_color(style,row[x0],i,native_y);
            for(unsigned x=x0;x<x1;x++)row[x]=ink;
        }
    }
}
/* RE39 R175: Infrared background is a genuine distance-based presentation
 * enhancement. The previous R172-R174 color curves used source luminance as
 * a stand-in for proximity, so BRIGHT distant walls became pink and DARK near
 * walls stayed violet. The renderer already knows each wall's perpendicular
 * distance and each floor/ceiling row's projected distance: use those instead.
 * No new static noise, tiling, simulation, or actor changes are introduced. */
u32 avp_predator_infrared_background_depth_color(u32 original,double distance)
{
    const unsigned r=(original>>16)&255u,g=(original>>8)&255u,b=original&255u;
    const unsigned luma=(54u*r+183u*g+19u*b+128u)/256u;
    /* RE39 R176: calibrate scenery to the LEFT HALF of the new thermal photo.
     * Its cold ground is dark blue-violet, not the bright pink that R175
     * covered most near-floor rows with. Nearby walls can still become warm
     * magenta; distant walls are decidedly cooler. Preserve texture contrast
     * WITHIN each distance band; do not use luma as a stand-in for depth. */
    const unsigned near_r=122u+(luma*116u)/255u;
    const unsigned near_g=11u+(luma*44u)/255u;
    const unsigned near_b=97u+(luma*59u)/255u;
    const unsigned far_r=6u+(luma*46u)/255u;
    const unsigned far_g=7u+(luma*12u)/255u;
    const unsigned far_b=56u+(luma*99u)/255u;
    double t;
    unsigned weight;
    if(!isfinite(distance) || distance<0.0)distance=128.0;
    /* <=1 cell: magenta/pink; from 1 to 5 cells, fade to blue-violet.
     * R175's 2..8-cell warm range reached too far into the scene. */
    t=(distance-1.0)/4.0;
    if(t<0.0)t=0.0;
    if(t>1.0)t=1.0;
    t=t*t*(3.0-2.0*t); /* smoothstep; no sudden color boundary */
    weight=(unsigned)(t*256.0+0.5);
    if(weight>256u)weight=256u;
    return (((near_r*(256u-weight)+far_r*weight+128u)/256u)<<16)|
           (((near_g*(256u-weight)+far_g*weight+128u)/256u)<<8)|
            ((near_b*(256u-weight)+far_b*weight+128u)/256u);
}
void avp_predator_infrared_background_composite_depth_rgb16(
    u32 *scene,const u32 *mask,const float *wall_depth,
    unsigned width,unsigned height,int draw_distance_mode)
{
    unsigned bounds[321],samples[320];
    if(!scene||!mask||!width||!height)return;
    if((size_t)width>SIZE_MAX/(size_t)height)return;
    /* Preserve Jaguar 320-word RGB16 resampling and the established
     * sprite/HUD/weapon/pickup mask, exactly as in the older compositor. */
    for(unsigned i=0;i<=320u;i++)
        bounds[i]=(unsigned)(((uint64_t)i*width+319u)/320u);
    for(unsigned i=0;i<320u;i++){
        const unsigned sx=(unsigned)(((uint64_t)(2u*i+1u)*width)/640u);
        samples[i]=sx<width?sx:width-1u;
    }
    for(unsigned y=0;y<height;y++){
        u32 *row=scene+(size_t)y*width;
        const u32 *protect=mask+(size_t)y*width;
        const float *walls=wall_depth?wall_depth+(size_t)y*width:NULL;
        double flat_dist;
        const double retail_row=((double)y+0.5)*228.0/(double)height;
        if(!avp_host_flat_row_depth(retail_row,draw_distance_mode,&flat_dist))
            flat_dist=avp_host_ray_limit(draw_distance_mode);
        /* The reference's ground and ambient surfaces remain cool even at
         * close range: a nearby FLOOR is not a nearby warm body. Retain the
         * projected geometric distance, with a presentation-only 4-cell
         * ambient offset on flats. WALL depths below override this offset. */
        flat_dist+=4.0;
        for(unsigned i=0;i<320u;i++){
            const unsigned x0=bounds[i],x1=bounds[i+1u];
            const unsigned sx=samples[i];
            double d=flat_dist;
            if(x0==x1||protect[sx])continue;
            /* FLT_MAX denotes a pixel not covered by a wall, in which case
             * projected ceiling/floor distance is authoritative. */
            if(walls && walls[sx]>0.0f && walls[sx]<FLT_MAX*0.5f)
                d=(double)walls[sx];
            const u32 ink=avp_predator_infrared_background_depth_color(row[x0],d);
            for(unsigned x=x0;x<x1;x++)row[x]=ink;
        }
    }
}

void avp_predator_background_mark_scaled_overlay(u32 *mask,unsigned stride,
                                   unsigned width,unsigned height,
                                   const u32 *before,const u32 *after)
{
    if(!mask||!before||!after||!stride||!width||!height)return;
    /* The native 320x228 HUD compositor expands each altered texel over
     * inverse nearest-neighbor bounds, exactly like overlay_scale_composite. */
    for(unsigned ny=0;ny<228u;ny++){
        const unsigned y0=(unsigned)(((uint64_t)ny*height+227u)/228u);
        const unsigned y1=(unsigned)(((uint64_t)(ny+1u)*height+227u)/228u);
        if(y0==y1)continue;
        for(unsigned nx=0;nx<320u;nx++){
            if(before[(size_t)ny*320u+nx]==after[(size_t)ny*320u+nx])continue;
            const unsigned x0=(unsigned)(((uint64_t)nx*width+319u)/320u);
            const unsigned x1=(unsigned)(((uint64_t)(nx+1u)*width+319u)/320u);
            for(unsigned y=y0;y<y1;y++)
                for(unsigned x=x0;x<x1;x++)mask[(size_t)y*stride+x]=1u;
        }
    }
}

int avp_thermal_is_living(const AvpAmp *a)
{
    if(!a || a->animseq==AS_HIDDEN || a->animseq==AS_DEATH ||
       (a->flags&(1u<<AMP_DEAD)))return 0;
    switch(a->creature){
    case AC_HUMAN:case AC_PREDATOR:case AC_ALIEN:case AC_ACRAWL:
    case AC_AQUEEN:case AC_FACEHUG:case AC_EGG:case AC_COCOON:
        return 1;
    default:return 0;
    }
}
unsigned avp_thermal_intensity(const AvpAmp *a,double distance,u32 source_rgb)
{
    double temp,transmittance,texture,reading;
    unsigned r=(source_rgb>>16)&255u,g=(source_rgb>>8)&255u,b=source_rgb&255u;
    if(!avp_thermal_is_living(a) || !isfinite(distance))return 0u;
    /* Fictional species do not have measured emissivity or body temperature.
     * These are a deliberately gameplay-friendly heat palette and range
     * attenuation, NOT a radiometric sensor or a true temperature readout.
     * Body-shaped shading comes from original AAF sprite texel brightness.
     * Distance reduces contrast via atmospheric attenuation; it is NOT a
     * distance-squared brightness law for thermal radiance. */
    switch(a->creature){
    case AC_HUMAN:temp=244.0;break;
    case AC_PREDATOR:temp=234.0;break;
    case AC_AQUEEN:temp=210.0;break;
    case AC_ALIEN:case AC_ACRAWL:temp=196.0;break;
    case AC_FACEHUG:temp=202.0;break;
    case AC_EGG:temp=135.0;break;
    case AC_COCOON:temp=145.0;break;
    default:return 0u;
    }
    if(distance<1.0)distance=1.0;
    /* Short in-game hallways barely attenuate; long Sub-Level corridors and
     * optional extended draw distances gradually cool the apparent signal. */
    transmittance=exp(-0.075*(distance-1.0));
    texture=0.80+0.20*((0.299*r+0.587*g+0.114*b)/255.0);
    reading=18.0+(temp*texture-18.0)*transmittance;
    if(reading>254.0)reading=254.0;
    if(reading<1.0)reading=1.0;
    return (unsigned)(reading+0.5);
}

/* R163: distance affects apparent heat contrast, not the object's physical
 * temperature. R162 rendered nearby bright texels yellow and mid-distance
 * texels RED because red sat in the MIDDLE of the palette. Consequently
 * retreating targets appeared to heat up. For the cinematic presentation,
 * preserve source-texel detail and fade the apparent signal towards the
 * blue/cyan end as the target recedes. Only the blue Thermal enhancement
 * calls this function; Electromagnetic retains its exact R162 behavior. */
unsigned avp_thermal_heat_intensity(const AvpAmp *a,double distance,u32 source_rgb)
{
    double apparent,luma,texture,transmittance,signal;
    const unsigned r=(source_rgb>>16)&255u;
    const unsigned g=(source_rgb>>8)&255u;
    const unsigned b=source_rgb&255u;
    if(!avp_thermal_is_living(a) || !isfinite(distance))return 0u;
    /* Fictional species' temperatures are artistic values, not real
     * temperature readings. Tint depends on the original AAF sprite texel:
     * light areas have more apparent heat, shaded parts can remain cool. */
    switch(a->creature){
    case AC_HUMAN:apparent=247.0;break;
    case AC_PREDATOR:apparent=235.0;break;
    case AC_AQUEEN:apparent=227.0;break;
    case AC_ALIEN:case AC_ACRAWL:apparent=224.0;break;
    case AC_FACEHUG:apparent=230.0;break;
    case AC_EGG:apparent=137.0;break;
    case AC_COCOON:apparent=164.0;break;
    default:return 0u;
    }
    if(distance<1.0)distance=1.0;
    /* Source texture is sampled as decoded CRY RGB. The square root keeps
     * shape detail but allows genuinely dark texels to stay blue/green. */
    luma=(0.299*r+0.587*g+0.114*b)/255.0;
    texture=0.34+0.66*sqrt(luma);
    /* Close actors reach orange/red. At long corridor distance the same
     * texels lose contrast through atmospheric attenuation; a far-away
     * silhouette can no longer sit in the red peak. A short flat near-field
     * prevents arbitrary tint changes during small melee movements. */
    transmittance=exp(-0.21*fmax(0.0,distance-3.5));
    signal=5.0+(apparent*texture-5.0)*transmittance;
    if(signal>254.0)signal=254.0;
    if(signal<1.0)signal=1.0;
    return (unsigned)(signal+0.5);
}
u32 avp_thermal_heat_palette(unsigned intensity)
{
    /* R163 movie-art-inspired Thermal color map. Chromatic progression is
     * monotonic in its warmest region: navy -> blue -> cyan -> green ->
     * yellow -> orange -> RED at the top. R162 had red BEFORE orange/yellow,
     * so lowering the heat intensity with distance made distant actors red.
     * Geometry, weapons, and HUD remain rendered through original filters. */
    static const struct {unsigned i,r,g,b;} stops[]={
        {0,5,17,59}, {30,8,55,148}, {58,11,125,218},
        {83,35,211,234}, {109,26,179,103}, {137,131,207,36},
        {161,237,215,24}, {184,255,144,15}, {208,252,72,12},
        {232,240,24,18}, {255,255,37,26}};
    unsigned k;
    if(intensity>255u)intensity=255u;
    for(k=1;k<sizeof(stops)/sizeof(stops[0]);k++)if(intensity<=stops[k].i)break;
    if(k>=sizeof(stops)/sizeof(stops[0]))return 0x00ff251a;
    {
        const unsigned range=stops[k].i-stops[k-1].i;
        const unsigned t=intensity-stops[k-1].i;
        unsigned r=(stops[k-1].r*(range-t)+stops[k].r*t+range/2u)/range;
        unsigned g=(stops[k-1].g*(range-t)+stops[k].g*t+range/2u)/range;
        unsigned b=(stops[k-1].b*(range-t)+stops[k].b*t+range/2u)/range;
        return (r<<16)|(g<<8)|b;
    }
}
u32 avp_infrared_palette(unsigned intensity)
{
    /* RE39 R171: retune Infrared against the newer reference. Preserve the
     * cooler purple/magenta cloth/shadow separation from R165, but push hotter
     * texels farther into yellow/orange with a slight warm-white tendency at
     * the brightest end. No distance logic or actor detection changes here. */
    static const struct { unsigned i,r,g,b; } stops[]={
        {0,6,6,58}, {36,18,10,116}, {68,52,13,170},
        {96,112,14,170}, {123,171,20,157}, {147,198,26,140},
        {171,221,45,119}, {194,240,88,74}, {219,250,165,56},
        {240,255,221,76}, {255,255,240,128}};
    unsigned k;
    if(intensity>255u)intensity=255u;
    for(k=1;k<sizeof(stops)/sizeof(stops[0]);k++)if(intensity<=stops[k].i)break;
    if(k>=sizeof(stops)/sizeof(stops[0]))return 0x00ffe450;
    {
        const unsigned range=stops[k].i-stops[k-1].i;
        const unsigned t=intensity-stops[k-1].i;
        const unsigned r=(stops[k-1].r*(range-t)+stops[k].r*t+range/2u)/range;
        const unsigned g=(stops[k-1].g*(range-t)+stops[k].g*t+range/2u)/range;
        const unsigned b=(stops[k-1].b*(range-t)+stops[k].b*t+range/2u)/range;
        return (r<<16)|(g<<8)|b;
    }
}
u32 avp_thermal_palette(unsigned intensity)
{
    /* Electromagnetic false color: cool navy -> blue -> cyan -> yellow -> orange ->
     * red -> pale-white hot points. No world geometry is ever recolored. */
    static const struct { unsigned i,r,g,b; } stops[]={
        {0,10,15,38},{45,20,58,151},{96,25,180,208},
        {138,240,221,83},{183,255,108,31},{226,255,29,19},
        {255,255,236,204}};
    unsigned k;
    if(intensity>255u)intensity=255u;
    for(k=1;k<sizeof(stops)/sizeof(stops[0]);++k)if(intensity<=stops[k].i)break;
    if(k>=sizeof(stops)/sizeof(stops[0]))return 0x00ffeccc; /* unreachable */
    {
        const unsigned range=stops[k].i-stops[k-1].i;
        const unsigned t=intensity-stops[k-1].i;
        const unsigned r=(stops[k-1].r*(range-t)+stops[k].r*t+range/2u)/range;
        const unsigned g=(stops[k-1].g*(range-t)+stops[k].g*t+range/2u)/range;
        const unsigned b=(stops[k-1].b*(range-t)+stops[k].b*t+range/2u)/range;
        return (r<<16)|(g<<8)|b;
    }
}
u32 avp_thermal_record(unsigned intensity,u32 original_color)
{
    if(!intensity)return 0u;
    if(intensity>254u)intensity=254u;
    return ((intensity+1u)<<24)|(original_color&0xffffffu);
}
void avp_thermal_remove_covered(u32 *records,const u32 *scene,size_t count)
{
    size_t i;
    if(!records||!scene)return;
    for(i=0;i<count;i++)if(records[i] &&
        (records[i]&0xffffffu)!=(scene[i]&0xffffffu))records[i]=0u;
}
void avp_thermal_composite(const u32 *records,u32 *scene,size_t count)
{
    size_t i;
    if(!records||!scene)return;
    for(i=0;i<count;i++)if(records[i])
        scene[i]=avp_thermal_palette((records[i]>>24)-1u);
}

void avp_thermal_composite_rgb16(const u32 *records,u32 *scene,
                                 unsigned width,unsigned height)
{
    unsigned y,i,x;
    if(!records||!scene||!width||!height)return;
    /* VMODE RGB16 samples one source column per Jaguar line-buffer word.
     * Reuse that mapping exactly; a raw screen-space post effect would
     * misalign at wide or high-resolution host window sizes. */
    for(y=0;y<height;y++){
        const size_t row=(size_t)y*width;
        for(i=0;i<320u;i++){
            const unsigned sx=(unsigned)(((uint64_t)(2u*i+1u)*width)/640u);
            const unsigned start=(unsigned)(((uint64_t)i*width+319u)/320u);
            const unsigned end=(unsigned)(((uint64_t)(i+1u)*width+319u)/320u);
            const u32 heat=records[row+(sx<width?sx:width-1u)];
            if(!heat)continue;
            const u32 color=avp_thermal_palette((heat>>24)-1u);
            for(x=start;x<end;x++)scene[row+x]=color;
        }
    }
}

void avp_thermal_composite_vmode(const u32 *records,u32 *scene,
                                 unsigned width,unsigned height,
                                 unsigned mode,unsigned style)
{
    unsigned words,y,i,x;
    if(!records || !scene || !width || !height || !style)return;
    if(mode!=6u && mode!=2u)return;
    words=mode==6u?320u:160u;
    for(y=0;y<height;y++){
        size_t row=(size_t)y*width;
        for(i=0;i<words;i++){
            const unsigned sx0=(unsigned)(((uint64_t)((mode==6u?2u:4u)*i+1u)*width)/640u);
            const u32 h0=records[row+(sx0<width?sx0:width-1u)];
            unsigned intensity,start,end;
            u32 color;
            if(!h0)continue;
            intensity=(h0>>24)-1u;
            if(mode==2u){
                /* RGB24 combines TWO adjacent source words in one output
                 * pixel. Keep this safe generic compositor for any RGB24
                 * caller; all three current enhancement targets use RGB16. */
                unsigned sx1=(unsigned)(((uint64_t)(4u*i+3u)*width)/640u);
                u32 h1=records[row+(sx1<width?sx1:width-1u)];
                if(!h1)continue;
                intensity=(intensity+((h1>>24)-1u)+1u)/2u;
            }
            color=style==AVP_VISION_ENHANCED_THERMAL?
                  avp_thermal_heat_palette(intensity):
                  style==AVP_VISION_ENHANCED_INFRARED?
                  avp_infrared_palette(intensity):avp_thermal_palette(intensity);
            start=(unsigned)(((uint64_t)i*width+words-1u)/words);
            end=(unsigned)(((uint64_t)(i+1u)*width+words-1u)/words);
            for(x=start;x<end;x++)scene[row+x]=color;
        }
    }
}
