#version 450
#extension GL_GOOGLE_include_directive : require
/* RE46 R336. Opaque *plain* wall source-texture diagnostic; one source image
 * bound to descriptor zero. This is NOT a complete gameplay shader.
 * No CPU framebuffer pixels or pre-shaded wall columns are uploaded.
 * Host builds a deterministic projected-wall stream from real R334 rays.
 * Unsupported doors, windows, portals, source alpha, fog/AF parity and
 * multiple simultaneous source textures are NOT certified here. */
layout(set=0,binding=0) uniform usampler2D avp_source;
layout(set=0,binding=1) uniform usampler2D avp_lut;
layout(std430,set=0,binding=2) readonly buffer AvpWallProjection {
    uint words[];
} avp_projection;
layout(push_constant) uniform AvpSampling {
    uint encoding;       /* R32_UINT CRY16/alpha8 or RGB8/alpha8 */
    uint wx;             /* expected image slot for this diagnostic draw */
    uint wy;             /* must be zero: nearest-point-only diagnostic */
    int shade;           /* explicitly zero until fog/CLUT is verified */
} avp_params;
layout(location=0) out vec4 fragColor;
#include "avp_world_opaque_integer.glsl"
void avp_invalid() { fragColor=vec4(1.0,0.0,1.0,1.0); }
void main()
{
    uint x=uint(gl_FragCoord.x),y=uint(gl_FragCoord.y);
    uint n=uint(avp_projection.words.length());
    if(n<8u){avp_invalid();return;}
    uint version=avp_projection.words[1];
    uint widthWords=avp_projection.words[2];
    uint strideWords=version==3u?13u:(version==2u?12u:11u);
    uint firstMapWord=8u+13u*widthWords;
    if(n<8u || avp_projection.words[0]!=0x4c575641u ||
       (version!=1u && version!=2u && version!=3u) ||
       avp_projection.words[2]==0u || avp_projection.words[2]>8192u ||
       avp_projection.words[3]==0u || avp_projection.words[3]>8192u ||
       avp_projection.words[4]!=(version==3u?52u:(version==2u?48u:44u)) ||
       avp_projection.words[5]!=32u ||
       (version==3u ?
          (avp_projection.words[7]!=firstMapWord ||
           n<firstMapWord || n>8388608u) :
          (avp_projection.words[7]!=0u || n!=8u+strideWords*widthWords)) ||
       x>=avp_projection.words[2] || y>=avp_projection.words[3] ||
       avp_params.wy!=0u || avp_params.shade!=0) {
        avp_invalid();return;
    }
    uint base=8u+strideWords*x;
    uint wallState=avp_projection.words[base];
    if(wallState==0u){discard;}
    if(wallState!=1u){
        avp_invalid();return;
    }
    /* R346: a different texture's column is not an invalid fragment.
     * Each slot is drawn in its own pass, including mixed CRY/RGB walls. */
    if(avp_projection.words[base+1u]!=avp_params.wx)discard;
    if(avp_projection.words[base+10u]!=avp_params.encoding ||
       avp_projection.words[base+5u]>65535u){
        avp_invalid();return;
    }
    uint top=avp_projection.words[base+3u];
    uint bottom=avp_projection.words[base+4u];
    if(top>=bottom || bottom>avp_projection.words[3]){
        avp_invalid();return;
    }
    if(y<top || y>=bottom)discard;
    int originalTop=int(avp_projection.words[base+5u])-32768;
    int projectedHeight=int(avp_projection.words[base+6u]);
    int width=int(avp_projection.words[base+8u]);
    int height=int(avp_projection.words[base+9u]);
    int sourceX=int(avp_projection.words[base+7u]);
    if(projectedHeight<=0 || projectedHeight>32768 ||
       width<=0 || height<=0 || width>8192 || height>8192 ||
       sourceX<0 || sourceX>=width ||
       textureSize(avp_source,0)!=ivec2(width,height)){
        avp_invalid();return;
    }
    /* Integer nearest-point source coordinate.  The CPU restricts the
     * projected height so these intermediates are safe in GLSL 32-bit int.
     * Original host source rows use truncation toward zero. */
    int relative=int(y)-originalTop;
    int numerator=relative*2+1;
    int sourceY;
    if(height>128) sourceY=(numerator*height)/(2*projectedHeight);
    else {
        int nativeRow=clamp((numerator*128)/(2*projectedHeight),0,127);
        sourceY=nativeRow*height/128;
    }
    sourceY=clamp(sourceY,0,height-1);
    /* R345: the optional v3 tail contains the exact Software double-DDA
     * texel row for each visible hi-res fragment. This remains in the same
     * std430 geometry buffer; no float64 shader feature or new descriptor. */
    if(version==3u){
        uint rowMapWord=avp_projection.words[base+12u];
        if(rowMapWord!=0u){
            uint visible=bottom-top;
            uint count=(visible+1u)/2u;
            if(height<=128 || rowMapWord<firstMapWord ||
               rowMapWord>n || count>n-rowMapWord){
                avp_invalid();return;
            }
            uint index=y-top;
            uint packed=avp_projection.words[rowMapWord+index/2u];
            sourceY=int((packed>>((index&1u)*16u))&65535u);
            if(sourceY>=height){avp_invalid();return;}
        }
    }
    int columnShade=version>=2u ?
        int(avp_projection.words[base+11u]) : 0;
    if(columnShade < -255 || columnShade > 255 ||
       (avp_params.encoding==2u && columnShade!=0)){
        avp_invalid();return;
    }
    uint raw=texelFetch(avp_source,ivec2(sourceX,sourceY),0).r;
    uint alpha=avp_params.encoding==1u ? ((raw>>16u)&255u) :
                                          ((raw>>24u)&255u);
    if(alpha!=255u)discard; /* transparency/partial coverage not certified */
    uint rgb=avp_params.encoding==1u ? avp_cry_rgb(raw,columnShade,avp_lut) :
                                         avp_rgb_shaded(raw,0);
    fragColor=vec4(float((rgb>>16u)&255u),float((rgb>>8u)&255u),
                   float(rgb&255u),255.0)/255.0;
}
