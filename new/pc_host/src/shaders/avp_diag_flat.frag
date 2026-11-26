#version 450
#extension GL_GOOGLE_include_directive : require
/* RE47/R355: proposed nearest-texel Vulkan floor/ceiling fragment pass.
 * A source-backed AVFL stream contains exact original host row projection,
 * ORIGINAL Jaguar CRY intensity deltas and nearest texture coordinates.
 * It is deliberately NOT connected to the game's active renderer yet.
 * The CPU sends SOURCE TEXEL REFERENCES, not composed framebuffer pixels. */
layout(set=0,binding=0) uniform usampler2D avp_source;
layout(set=0,binding=1) uniform usampler2D avp_lut;
layout(std430,set=0,binding=2) readonly buffer AvpFlatProjection {
    uint words[];
} avp_flat;
layout(push_constant) uniform AvpSampling {
    uint encoding;
    uint wx;  /* source image slot bound by this draw */
    uint wy;  /* nearest-point diagnostic only; must be zero */
    int shade;/* unused: exact Jaguar flat shade comes from the stream */
} avp_params;
layout(location=0) out vec4 fragColor;
#include "avp_world_opaque_integer.glsl"
void avp_invalid_flat() { fragColor=vec4(1.0,0.0,1.0,1.0); }
void main()
{
    uint x=uint(gl_FragCoord.x), y=uint(gl_FragCoord.y);
    uint n=uint(avp_flat.words.length());
    if(n<8u){avp_invalid_flat();return;}
    uint w=avp_flat.words[2], h=avp_flat.words[3];
    if(avp_flat.words[0]!=0x4c465641u || avp_flat.words[1]!=1u ||
       !((w>0u)&&(w<=8192u)&&(h>0u)&&(h<=8192u)) ||
       avp_flat.words[4]!=16u || avp_flat.words[5]!=32u ||
       avp_flat.words[7]!=0u ||
       n!=8u+4u*w*h ||
       x>=w||y>=h || avp_params.wy!=0u||avp_params.shade!=0){
        avp_invalid_flat();return;
    }
    uint idx=8u+4u*(y*w+x);
    uint slot=avp_flat.words[idx];
    if(slot==0xffffffffu)discard; /* no flat at this pixel */
    if(slot!=avp_params.wx)discard; /* drawn in another texture pass */
    uint sx=avp_flat.words[idx+1u],sy=avp_flat.words[idx+2u];
    int flatShade=int(avp_flat.words[idx+3u]);
    ivec2 size=textureSize(avp_source,0);
    if(size.x<=0||size.y<=0||sx>=uint(size.x)||sy>=uint(size.y)||
       (avp_params.encoding!=1u && avp_params.encoding!=2u) ||
       flatShade < -255 || flatShade > 255){
        avp_invalid_flat();return;
    }
    uint raw=texelFetch(avp_source,ivec2(sx,sy),0).r;
    uint alpha=avp_params.encoding==1u ? ((raw>>16u)&255u) :
                                          ((raw>>24u)&255u);
    /* Transparent/partial sources require a future blending parity pass. */
    if(alpha!=255u)discard;
    uint rgb=avp_params.encoding==1u ? avp_cry_rgb(raw,flatShade,avp_lut) :
                                         (raw&0xffffffu);
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
