/* RE39 R194: verify the actual single-player presentation entry is wired to
 * the R193 crosshair compositor. R193's primitive-only test passed despite
 * the ordinary main-loop renderer never enabling its gameplay frame flag.
 * Source integration check is intentional: the screen presenter needs X11
 * and real ROM rendering; this check locks the missing production call site.
 */
#include "pc/pc_crosshair.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text(const char *path)
{
    FILE *f=fopen(path,"rb");
    long n;
    char *s;
    assert(f);
    assert(fseek(f,0,SEEK_END)==0);
    n=ftell(f);
    assert(n>0);
    assert(fseek(f,0,SEEK_SET)==0);
    s=(char*)malloc((size_t)n+1u);
    assert(s);
    assert(fread(s,1,(size_t)n,f)==(size_t)n);
    s[n]='\0';
    fclose(f);
    return s;
}

static void require_gameplay_gate(const char *begin,const char *end)
{
    const char *open=strstr(begin,"v->gameplay_crosshair=1;");
    const char *present;
    const char *close;
    assert(open && open<end);
    present=strstr(open,"present_framebuffer(v,");
    assert(present && present<end);
    close=strstr(present,"v->gameplay_crosshair=0;");
    assert(close && close<end);
}
int main(int argc,char **argv)
{
    uint32_t pixels[320u*228u]={0};
    char *src;
    const char *maze,*frontend,*gameplay,*automap;
    assert(argc==2);
    src=read_text(argv[1]);
    maze=strstr(src,"void avp_pc_video_render_maze(");
    assert(maze);
    frontend=strstr(maze,"/* -------------------------------------------------------------------------\n * Retail MJP front-end bridge.");
    assert(frontend);
    /* This is the route used by the ordinary single-player main loop. */
    require_gameplay_gate(maze,frontend);
    gameplay=strstr(src,"void avp_pc_video_present_gameplay(");
    assert(gameplay);
    automap=strstr(gameplay,"void avp_pc_video_automap_save(");
    assert(automap);
    /* Preserve the existing event-driven presentation as well. */
    require_gameplay_gate(gameplay,automap);
    /* Demonstrate the real enabled-P1 pixel primitive still draws at center. */
    avp_pc_crosshair_draw(pixels,320u,320u,228u,0,0,320u,228u,0,0);
    assert(pixels[114u*320u+160u]==0x00f0f0f0u);
    free(src);
    puts("R194 single-player and event-frame crosshair presentation PASS");
    return 0;
}
