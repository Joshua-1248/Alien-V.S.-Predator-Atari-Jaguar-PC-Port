/* RE39 R166: original register identity, independent toggle, scene/foreground
 * isolation under native RGB16 VMODE, and stable mode-specific background. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned r(u32 x){return (x>>16)&255u;}
static unsigned g(u32 x){return (x>>8)&255u;}
static unsigned b(u32 x){return x&255u;}
int main(void)
{
    const u32 world=0x00c7bd80u;
    const unsigned blue=AVP_VISION_ENHANCED_THERMAL;
    const unsigned purple=AVP_VISION_ENHANCED_ELECTROMAGNETIC;
    const unsigned ir=AVP_VISION_ENHANCED_INFRARED;
    u32 frame[640u*2u],mask[640u*2u],before[320u*228u]={0},after[320u*228u]={0};
    /* The shared background toggle must leave original Thermal scenery
     * untouched, while the separate living-actor Thermal toggle still works. */
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x26f8)==AVP_VISION_ORIGINAL);
    assert(avp_predator_vision_style_ex(1,0,0,PT_PREDATOR,2,0x06c7,0x26f8)==(int)blue);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x9558)==(int)purple);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x8880)==(int)ir);
    assert(avp_predator_background_style(0,PT_PREDATOR,2,0x06c7,0x26f8)==0);
    assert(avp_predator_background_style(1,PT_HUMAN,2,0x06c7,0x26f8)==0);
    assert(avp_predator_background_style(1,PT_PREDATOR,1,0x06c7,0x26f8)==0);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x0ec3,0x8880)==0);
    assert(avp_predator_background_color(0,world,0,0)==world);
    assert(avp_predator_background_color(blue,world,22,40)!=world);
    assert(avp_predator_background_color(purple,world,22,40)!=world);
    assert(avp_predator_background_color(ir,world,22,40)!=world);
    assert(b(avp_predator_background_color(blue,world,22,40))>
           r(avp_predator_background_color(blue,world,22,40))*3u);
    assert(b(avp_predator_background_color(ir,world,22,40))>
           g(avp_predator_background_color(ir,world,22,40))*2u);
    assert(r(avp_predator_background_color(purple,world,22,40))>
           r(avp_predator_background_color(blue,world,22,40)));
    { /* R169: only the Thermal SCENERY blue component brightens by 12.
       Exact reference values lock the restrained change and ensure previous
       dark/light scenery detail is not flattened or made cyan/green. */
      static const struct {u32 input,expected;} samples[]={
          {0x00000000u,0x00041853u},
          {0x00224466u,0x00092c81u},
          {0x006080a0u,0x000d3faeu},
          {0x00c7bd80u,0x001254dcu},
          {0x00ffffffu,0x00145ef3u}
      };
      for(unsigned i=0;i<sizeof(samples)/sizeof(samples[0]);i++){
          assert(avp_predator_background_color(blue,samples[i].input,0,0)==samples[i].expected);
          assert(avp_predator_background_color(blue,samples[i].input,41,73)==samples[i].expected);
      }
      /* R174: Infrared scenery now keeps more of the lower/mid range
       * in purple/violet, with pink/red-magenta reserved farther up the
       * brightness curve for a better depth-like separation. */
      assert(avp_predator_background_color(ir,world,22,40)==0x00e8299au);
    }
    assert(avp_predator_background_color(purple,world,1,1)==
           avp_predator_background_color(purple,world,1,1));
    { /* R168: Electromagnetic no longer uses any tiled/mottled position-
       * based pattern. Like Infrared, identical scene input stays spatially
       * stable; distinction now comes from its darker higher-contrast palette. */
      u32 em0=avp_predator_background_color(purple,world,0,0),
          em1=avp_predator_background_color(purple,world,48,48);
      u32 ir0=avp_predator_background_color(ir,world,0,0),
          ir1=avp_predator_background_color(ir,world,48,48);
      assert(em0==em1 && ir0==ir1);
    }
    for(unsigned i=0;i<640u*2u;i++){frame[i]=world;mask[i]=0;}
    /* A sprite and HUD source texel at Jaguar word 20 in row 0 must remain
       unmodified throughout its two-column expanded output span. */
    mask[41]=1;
    frame[40]=frame[41]=0x00111213u;
    frame[680]=0x00885544u; /* second row foreground */
    mask[681]=1;
    avp_predator_background_composite_rgb16(frame,mask,640,2,blue);
    assert(frame[40]==0x00111213u && frame[41]==0x00111213u);
    assert(frame[680]==0x00885544u); /* sample word protected */
    assert(frame[42]!=world && frame[43]!=world);
    assert(frame[0]!=world && frame[1]==frame[0]);
    for(unsigned i=0;i<640u*2u;i++)frame[i]=world;
    avp_predator_background_composite_rgb16(frame,mask,640,2,
        (unsigned)avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x26f8));
    for(unsigned i=0;i<640u*2u;i++)assert(frame[i]==world);
    avp_predator_background_composite_rgb16(frame,mask,640,2,0);
    for(unsigned i=0;i<640u*2u;i++)assert(frame[i]==world);
    /* HUD native tile 5,6 expands to x=10..11, y=12..13 at 2x size. */
    memset(mask,0,sizeof mask);
    after[6u*320u+5u]=0x00123456u;
    avp_predator_background_mark_scaled_overlay(mask,640,640,2,before,after);
    /* Height 2 has a zero-height region for native row 6. */
    for(unsigned i=0;i<640u*2u;i++)assert(mask[i]==0u);
    /* Native 320x228 path, with matching foreground sample. */
    {u32 native_mask[320u*228u]={0};
     avp_predator_background_mark_scaled_overlay(native_mask,320,320,228,before,after);
     assert(native_mask[6u*320u+5u]==1u);
     assert(native_mask[6u*320u+4u]==0u);
    }
    /* Existing sprite heat spectra are unchanged. */
    assert(avp_thermal_heat_palette(255)==0x00ff251au);
    assert(avp_thermal_palette(255)==0x00ffecccu);
    assert(avp_infrared_palette(255)==0x00fff080u);
    puts("R174 Predator background RGB16 mode, foreground mask and palette isolation PASS");
    return 0;
}
