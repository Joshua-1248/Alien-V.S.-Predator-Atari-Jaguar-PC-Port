#version 450
/* RE47/R360: the original Jaguar actor/weapon/HUD output above a source-backed
 * Vulkan world. The CPU supplies only the pixels that CHANGED after the world
 * pass. It does not supply an opaque completed Software frame. A changed pixel
 * already contains the correct result of the Jaguar's alpha compositing,
 * so the hybrid GPU draw can overwrite it exactly without approximating the
 * original semi-transparent sprite blends. */
layout(set=0,binding=0) uniform usampler2D avp_foreground;
/* Descriptor binding 1 is reserved for the shared integer-image layout.
 * No CRY LUT is needed because these foreground pixels are final XRGB. */
layout(set=0,binding=1) uniform usampler2D avp_reserved_lut;
layout(push_constant) uniform AvpHybridSampling {
    uint encoding;
    uint screen_width;
    uint screen_height;
    int reserved;
} avp_params;
layout(location=0) out vec4 fragColor;
void main()
{
    uint x=uint(gl_FragCoord.x),y=uint(gl_FragCoord.y);
    ivec2 extent=textureSize(avp_foreground,0);
    if(extent.x<=0||extent.y<=0||avp_params.encoding!=2u ||
       avp_params.screen_width!=uint(extent.x) ||
       avp_params.screen_height!=uint(extent.y) ||
       x>=uint(extent.x)||y>=uint(extent.y) || avp_params.reserved!=0){
        fragColor=vec4(1.0,0.0,1.0,1.0);
        return;
    }
    uint pixel=texelFetch(avp_foreground,ivec2(x,y),0).r;
    uint alpha=pixel>>24u;
    if(alpha==0u)discard;
    if(alpha!=255u){fragColor=vec4(1.0,0.0,1.0,1.0);return;}
    uint rgb=pixel&0x00ffffffu;
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
