#version 450
#extension GL_GOOGLE_include_directive : require
/* R327 diagnostic ONLY. Original integer Jaguar source payload is sampled
 * in fragment shading, NEVER by uploading a finished Software framebuffer.
 * The synthetic input is a 16x16 opaque checkerboard; no ROM asset required.
 * Actual wall coordinates, portals, fog, anisotropic taps and parity testing
 * remain prerequisites for Hardware gameplay. */
layout(set=0,binding=0) uniform usampler2D avp_source;
layout(set=0,binding=1) uniform usampler2D avp_lut;
layout(push_constant) uniform AvpSampling {
    uint encoding;
    uint wx;
    uint wy;
    int shade;
} avp_params;
layout(location=0) out vec4 fragColor;
#include "avp_world_opaque_integer.glsl"
void main()
{
    ivec2 dims=textureSize(avp_source,0);
    ivec2 xy=ivec2(gl_FragCoord.xy);
    ivec2 p00=ivec2(xy.x%dims.x,xy.y%dims.y);
    ivec2 p10=ivec2((p00.x+1)%dims.x,p00.y);
    ivec2 p01=ivec2(p00.x,(p00.y+1)%dims.y);
    ivec2 p11=ivec2(p10.x,p01.y);
    uint rgb;
    bool opaque=avp_opaque_bilinear(avp_source,avp_lut,avp_params.encoding,
        p00,p10,p01,p11,avp_params.wx,avp_params.wy,avp_params.shade,rgb);
    if (!opaque) discard;
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
