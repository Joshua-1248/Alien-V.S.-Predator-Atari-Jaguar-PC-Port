#version 450
#extension GL_GOOGLE_include_directive : require
/* R335: synthetic full-width GPU ray-buffer indexing + texture sampling.
 * This does NOT implement game walls, doors, lighting, fog or animation. */
layout(set=0,binding=0) uniform usampler2D avp_source;
layout(set=0,binding=1) uniform usampler2D avp_lut;
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
    uint x=uint(gl_FragCoord.x),y=uint(gl_FragCoord.y);
    uint n=uint(avp_ray.words.length());
    /* All indexing is checked before dereferencing. The CPU validates the
     * full wire stream before upload; these checks protect the shader too. */
    if(n<8u || avp_ray.words[0]!=0x57525641u || avp_ray.words[1]!=1u ||
       avp_ray.words[2]==0u || avp_ray.words[2]>8192u ||
       avp_ray.words[3]==0u || avp_ray.words[3]>8192u ||
       x>=avp_ray.words[2] || y>=avp_ray.words[3] ||
       avp_ray.words[4]!=avp_ray.words[2] || avp_ray.words[5]!=32u ||
       avp_ray.words[6]!=32u+24u*avp_ray.words[2] ||
       n!=8u+16u*avp_ray.words[2]) {
        fragColor=vec4(1.0,0.0,1.0,1.0);
        return;
    }
    uint column=8u+6u*x;
    uint first=avp_ray.words[column];
    if(avp_ray.words[column+1u]!=1u || first!=x ||
       avp_ray.words[column+4u]!=1u || avp_ray.words[column+5u]!=0u){
        fragColor=vec4(1.0,0.0,1.0,1.0);
        return;
    }
    uint hit=8u+6u*avp_ray.words[2]+10u*first;
    uint wall=avp_ray.words[hit+4u];
    if(wall<1u || wall>2u ||
       avp_ray.words[hit+7u]!=1u ||
       avp_ray.words[hit+8u]!=0u ||
       avp_ray.words[hit+9u]!=avp_ray.words[3]) {
        fragColor=vec4(1.0,0.0,1.0,1.0);
        return;
    }
    ivec2 dims=textureSize(avp_source,0);
    if(dims.x==0 || dims.y==0){fragColor=vec4(1.0,0.0,1.0,1.0);return;}
    uint sx=(x+(wall-1u))%uint(dims.x);
    uint sy=y%uint(dims.y);
    ivec2 p00=ivec2(int(sx),int(sy));
    ivec2 p10=ivec2(int((sx+1u)%uint(dims.x)),int(sy));
    ivec2 p01=ivec2(int(sx),int((sy+1u)%uint(dims.y)));
    ivec2 p11=ivec2(p10.x,p01.y);
    uint rgb;
    if(!avp_opaque_bilinear(avp_source,avp_lut,avp_params.encoding,
                          p00,p10,p01,p11,avp_params.wx,avp_params.wy,
                          avp_params.shade,rgb)){
        fragColor=vec4(1.0,0.0,1.0,1.0);
        return;
    }
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
