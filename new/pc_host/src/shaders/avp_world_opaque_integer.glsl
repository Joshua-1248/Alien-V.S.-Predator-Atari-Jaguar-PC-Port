/* RE46 R324 — shader-side integer texture sampling source (GLSL 450).
 * Standalone function library: not compiled to SPIR-V, linked to a pipeline,
 * or invoked by the R324 game. No proprietary shader code included.
 * Full game use requires asset descriptor upload, shader build, validated
 * portals/depth/fog, and a pixel-parity check on an actual Vulkan driver.
 * Texture and CRY LUT are VK_FORMAT_R32_UINT, never normalized/sRGB. */
#ifndef AVP_WORLD_OPAQUE_INTEGER_GLSL
#define AVP_WORLD_OPAQUE_INTEGER_GLSL
uint avp_cry_rgb(uint word, int shade, usampler2D cryLut)
{
    uint cry=word&65535u;
    int y=int(cry&255u);
    if(shade>=255)y=255;
    else if(shade<=-255)y=0;
    else y=clamp(y+shade,0,255);
    return texelFetch(cryLut,ivec2(y,int((cry>>8u)&255u)),0).r;
}
uint avp_rgb_shaded(uint word,int shade)
{
    uint r=(word>>16u)&255u,g=(word>>8u)&255u,b=word&255u;
    if(shade<0){
        uint k=shade<=-255?0u:uint(255+shade);
        r=r*k/255u;g=g*k/255u;b=b*k/255u;
    }
    return (r<<16u)|(g<<8u)|b;
}
/* Positions and 10-bit weights are already resolved from the exact CPU
 * raycast, avoiding divergent GPU float rounding on the first parity gate.
 * The next stage can move coordinate resolution to the shader only after
 * comparing projected coordinates against the software path. */
bool avp_opaque_bilinear(usampler2D source,usampler2D cryLut,
    uint encoding,ivec2 p00,ivec2 p10,ivec2 p01,ivec2 p11,
    uint wx,uint wy,int shade,out uint rgb)
{
    uint texels[4]=uint[4](texelFetch(source,p00,0).r,
                            texelFetch(source,p10,0).r,
                            texelFetch(source,p01,0).r,
                            texelFetch(source,p11,0).r);
    uint c[4];
    if(wx>1024u||wy>1024u|| (encoding!=1u && encoding!=2u))return false;
    for(int i=0;i<4;i++){
        uint a=encoding==1u?((texels[i]>>16u)&255u):((texels[i]>>24u)&255u);
        if(a!=255u)return false; /* transparent route not yet certified */
        c[i]=encoding==1u?avp_cry_rgb(texels[i],shade,cryLut):
                           avp_rgb_shaded(texels[i],shade);
    }
    uint w[4]=uint[4]((1024u-wx)*(1024u-wy),wx*(1024u-wy),
                       (1024u-wx)*wy,wx*wy);
    uint r=0u,g=0u,b=0u;
    for(int i=0;i<4;i++){
        r+=((c[i]>>16u)&255u)*w[i];
        g+=((c[i]>>8u)&255u)*w[i];
        b+=(c[i]&255u)*w[i];
    }
    rgb=(((r+524288u)>>20u)<<16u)|(((g+524288u)>>20u)<<8u)|
        ((b+524288u)>>20u);
    return true;
}
#endif
