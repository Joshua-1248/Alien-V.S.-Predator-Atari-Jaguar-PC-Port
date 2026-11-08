/* RE24-S10 frontend parity probe: renders the native port's title screen
 * (and its menu state) headless with the live frontend renderer, writing the
 * 240 displayed lines (frontend rows 14..253) as PPM for comparison with the
 * original ROM executed by jagref.  Built by tools/jagref/frontend_parity.sh. */
#include "avp_rom.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int avp_gpu_assets_frontend_load(AvpAvpGpuState*,const AvpRomImage*,unsigned,unsigned,unsigned);
static void ppm(const char *n,AvpJaguarFramebuffer *fb){ /* crop displayed lines 14..253 */
    FILE *f=fopen(n,"wb");fprintf(f,"P6 320 240 255\n");
    for(unsigned y=14;y<254;y++)for(unsigned x=0;x<320;x++){u32 c=fb->pixels[y*320+x];fputc(c>>16,f);fputc(c>>8,f);fputc(c,f);}fclose(f);}
int main(int argc,char**argv){if(argc<2)return 2;
    AvpRomImage rom={0};FILE *f=fopen(argv[1],"rb");fseek(f,0,SEEK_END);rom.size=ftell(f);rewind(f);
    rom.data=malloc(rom.size);if(!rom.data||fread(rom.data,1,rom.size,f)!=rom.size)return 1;fclose(f);rom.exact_retail_sha256=1;
    AvpAvpGpuState *st=calloc(1,sizeof *st);AvpJaguarFramebuffer fb={0};
    avp_jaguar_framebuffer_init(&fb,320,254);
    static const unsigned t[][2]={{208,0},{237,2},{236,3},{233,4},{232,6},{205,5},{234,7}};
    for(unsigned i=0;i<7;i++)avp_gpu_assets_frontend_load(st,&rom,AVP_MJP_TITLE,t[i][0],t[i][1]);
    st->frontend_mode=AVP_MJP_TITLE;st->frontend_title_splash=0;st->frontend_title_menu_pending=0;
    st->frontend_title_menu_dim_phase=6;st->frontend_title_reveal_tick=96;
    avp_gpu_frontend_present(st,&fb,AVP_MJP_TITLE,0);ppm("m_menu.ppm",&fb);
    st->frontend_title_splash=1;
    avp_gpu_frontend_present(st,&fb,AVP_MJP_TITLE,0);ppm("m_title.ppm",&fb);
    return 0;}
