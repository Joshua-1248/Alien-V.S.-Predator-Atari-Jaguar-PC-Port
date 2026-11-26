/* RE40 R214: TOM reference-machine frame capture width must belong to the
 * just-completed physical video field, not the widest previous field.
 * Uses real TOM MMIO, a harmless 68000 loop and an ordinary STOP object.
 * Not a claim of beam-cycle-accurate horizontal scheduling. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine machine;
static uint8_t cartridge[0x3000];

static void init(void)
{
    memset(cartridge,0,sizeof cartridge);
    cartridge[0x404]=0x00;cartridge[0x405]=0x80;
    cartridge[0x406]=0x20;cartridge[0x407]=0x00;
    cartridge[0x2000]=0x60;cartridge[0x2001]=0xfe;
    assert(jagm_init(&machine,cartridge,sizeof cartridge)==0);
    jagm_write32(&machine,0x1000u,0u);
    jagm_write32(&machine,0x1004u,4u); /* no-IRQ STOP */
    jagm_write16(&machine,0xf00020u,0x1000u);
    jagm_write16(&machine,0xf00022u,0u);
    jagm_write16(&machine,0xf0002eu,31u); /* HP */
    jagm_write16(&machine,0xf0003eu,31u); /* VP */
    jagm_write16(&machine,0xf00046u,28u); /* VDB */
    jagm_write16(&machine,0xf00048u,30u); /* VDE */
    jagm_write16(&machine,0xf00038u,4u);  /* HDB1 */
    jagm_write16(&machine,0xf0003au,4u);  /* HDB2 unused */
    jagm_write16(&machine,0xf0003cu,24u);/* HDE */
    jagm_write16(&machine,0xf00028u,1u); /* VMODE enabled CRY16 PW=1 */
}
static void field(unsigned width,unsigned hde)
{
    jagm_write16(&machine,0xf0003cu,(uint16_t)hde);
    jagm_run_frames(&machine,1u);
    assert(machine.frame_w==width);
    assert(machine.frame_h==1u);
    assert(machine.op_unsupported_mask==0u);
}
static void check_normal(void)
{
    init();
    field(20u,24u); /* first field wide */
    field(8u,12u);  /* second field narrower: fails untouched R213 */
    field(24u,28u); /* expand again */
    field(2u,6u);   /* very narrow */
    field(20u,24u); /* no stale width on subsequent fields */
}
static void check_no_video(void)
{
    init();
    field(20u,24u);
    jagm_write16(&machine,0xf00028u,0u); /* object processing disabled */
    jagm_run_frames(&machine,1u);
    assert(machine.frame_w==0u);
    jagm_write16(&machine,0xf00028u,1u);
    field(8u,12u);
}
static void check_hdb2(void)
{
    init();
    /* HDB2 lies in the following horizontal half; output two segments. */
    jagm_write16(&machine,0xf0003au,0x0406u);
    field(48u,0x0414u); /* (HP+1+20-4) = 48 */
    field(40u,0x040cu);
    field(48u,0x0414u);
}
static void check_pixel_clock_and_rgb24(void)
{
    init();
    /* VMODE PW=2: physical pixel width changes within successive fields. */
    jagm_write16(&machine,0xf00028u,0x0201u);
    field(10u,24u);
    field(4u,12u);
    field(12u,28u);
    /* RGB24 output uses two 16-bit line-buffer words per physical pixel;
     * its capture width must also contract across register changes. */
    init();
    jagm_write16(&machine,0xf00028u,3u);
    field(20u,24u);
    field(8u,12u);
    field(12u,16u);
}
int main(void)
{
    check_normal();check_no_video();check_hdb2();
    check_pixel_clock_and_rgb24();
    puts("R214 TOM per-field width, HDB2, pixel clock and RGB24 PASS");
    return 0;
}
