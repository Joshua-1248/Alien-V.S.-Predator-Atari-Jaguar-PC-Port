#version 450
#extension GL_GOOGLE_include_directive : require
/* R361 experimental per-frame SOURCE geometry: AVFL texel references are
 * produced from original ray/flat data, NOT from the Software RGB world. */
layout(set=0,binding=0) uniform usampler2D avp_source;
layout(set=0,binding=1) uniform usampler2D avp_lut;
layout(std430,set=0,binding=2) readonly buffer AvpFlatProjection {
    uint words[];
} avp_flat;
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
#include "avp_world_opaque_integer.glsl"
void main()
{
    uint n=uint(avp_flat.words.length());
    if(n<8u || avp_flat.words[0]!=0x4c465641u ||
       avp_flat.words[1]!=1u || avp_flat.words[4]!=16u ||
       avp_flat.words[5]!=32u || avp_flat.words[7]!=0u ||
       avp_params.region_width==0u || avp_params.region_height==0u){
        fragColor=vec4(1,0,1,1);return;
    }
    uint w=avp_flat.words[2],h=avp_flat.words[3];
    if(w==0u||h==0u||w>8192u||h>8192u||
       w!=avp_params.source_width||h!=avp_params.source_height||
       n!=8u+4u*w*h){
        fragColor=vec4(1,0,1,1);return;
    }
    uint px=uint(gl_FragCoord.x),py=uint(gl_FragCoord.y);
    if(px<avp_params.region_x||py<avp_params.region_y||
       px-avp_params.region_x>=avp_params.region_width||
       py-avp_params.region_y>=avp_params.region_height)discard;
    /* Exactly the CPU's nearest32 scaling: uint division, no float UV
     * rounding at odd aspect ratios or unusual presentation resolutions. */
    uint x=((px-avp_params.region_x)*w)/avp_params.region_width;
    uint y=((py-avp_params.region_y)*h)/avp_params.region_height;
    uint base=8u+4u*(y*w+x);
    uint slot=avp_flat.words[base];
    if(slot==0xffffffffu || slot!=avp_params.slot)discard;
    uint sx=avp_flat.words[base+1u],sy=avp_flat.words[base+2u];
    int shade=int(avp_flat.words[base+3u]);
    ivec2 extent=textureSize(avp_source,0);
    if(extent.x<=0||extent.y<=0||sx>=uint(extent.x)||sy>=uint(extent.y)||
       (avp_params.encoding!=1u&&avp_params.encoding!=2u)||
       shade< -255||shade>255){fragColor=vec4(1,0,1,1);return;}
    uint raw=texelFetch(avp_source,ivec2(sx,sy),0).r;
    /* The R358 CPU layering pass rejects partial-alpha material and has
     * resolved fully transparent texels before this pass. */
    uint rgb=avp_params.encoding==1u ? avp_cry_rgb(raw,shade,avp_lut) :
                                           (raw&0xffffffu);
    rgb=avp_scan_rgb(rgb,px-avp_params.region_x,py-avp_params.region_y);
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
