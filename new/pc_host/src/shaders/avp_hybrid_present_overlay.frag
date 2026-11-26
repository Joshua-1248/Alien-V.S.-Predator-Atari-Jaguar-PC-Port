#version 450
#extension GL_GOOGLE_include_directive : require
/* R361: foreground pixels are ONLY the original actors, weapons, HUD and
 * effects changed after the Software world pass. No full Software framebuffer
 * is used to cover Vulkan source-texture world pixels. */
layout(set=0,binding=0) uniform usampler2D avp_foreground;
layout(set=0,binding=1) uniform usampler2D avp_reserved_lut;
layout(push_constant) uniform AvpSampling {
    uint encoding;
    uint slot;
    uint source_width;
    uint source_height;
    uint region_x;
    uint region_y;
    uint region_width;
    uint region_height;
    uint scan_enabled,scan_style,scan_thickness,scan_spacing,scan_strength,scan_phase;
} avp_params;
layout(location=0) out vec4 fragColor;
#include "avp_scanlines.glsl"
void main()
{
    ivec2 sz=textureSize(avp_foreground,0);
    if(sz.x<=0||sz.y<=0||avp_params.encoding!=2u||
       avp_params.slot!=0u||
       avp_params.source_width!=uint(sz.x)||
       avp_params.source_height!=uint(sz.y)||
       avp_params.region_width==0u||avp_params.region_height==0u){fragColor=vec4(1,0,1,1);return;}
    uint px=uint(gl_FragCoord.x),py=uint(gl_FragCoord.y);
    if(px<avp_params.region_x||py<avp_params.region_y||
       px-avp_params.region_x>=avp_params.region_width||
       py-avp_params.region_y>=avp_params.region_height)discard;
    uint x=((px-avp_params.region_x)*uint(sz.x))/avp_params.region_width;
    uint y=((py-avp_params.region_y)*uint(sz.y))/avp_params.region_height;
    uint pixel=texelFetch(avp_foreground,ivec2(x,y),0).r;
    uint a=pixel>>24u;
    if(a==0u)discard;
    if(a!=255u){fragColor=vec4(1,0,1,1);return;}
    uint rgb=pixel&0xffffffu;
    rgb=avp_scan_rgb(rgb,px-avp_params.region_x,py-avp_params.region_y);
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
