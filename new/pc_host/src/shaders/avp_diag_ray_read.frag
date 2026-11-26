#version 450
#extension GL_GOOGLE_include_directive : require
/* R333 synthetic ray-read diagnostic ONLY. Integer Jaguar source is sampled
 * in fragment shading, NEVER by uploading a finished Software framebuffer.
 * The synthetic input is a 16x16 opaque checkerboard; no ROM asset required.
 * Actual wall coordinates, portals, fog, anisotropic taps and parity testing
 * remain prerequisites for Hardware gameplay. */
layout(set=0,binding=0) uniform usampler2D avp_source;
layout(set=0,binding=1) uniform usampler2D avp_lut;
/* R331 little-endian wire format: 32-byte header, 24-byte column,
 * 40-byte hit. R333 reads the stored ray ID directly on the GPU, instead
 * of trusting a CPU readback or displaying an uploaded software frame. */
layout(std430,set=0,binding=2) readonly buffer AvpRayWire {
    uint words[];
} avp_ray;
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
    /* Synthetic test packet: width=1, height=1, one wall with wall ID=1.
     * A malformed/absent GPU buffer deliberately draws magenta, which the
     * existing exact readback parity test then rejects. */
    if (avp_ray.words.length()<24 ||
        avp_ray.words[0]!=0x57525641u || avp_ray.words[1]!=1u ||
        avp_ray.words[2]!=1u || avp_ray.words[3]!=1u ||
        avp_ray.words[4]!=1u || avp_ray.words[5]!=32u ||
        avp_ray.words[6]!=56u || avp_ray.words[9]!=1u ||
        avp_ray.words[12]!=1u || avp_ray.words[18]!=1u) {
        fragColor=vec4(1.0,0.0,1.0,1.0);
        return;
    }
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
