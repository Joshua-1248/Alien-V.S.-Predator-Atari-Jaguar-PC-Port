/* RE41 R239: Jaguar GPU/DSP single-step control and SINGLE_STOP readback.
 * Atari TRM Rev.8 GPU pp.40,60 / DSP pp.110-111: after the first
 * instruction with GO|SINGLE_STEP, halt until a SINGLE_GO write; the
 * read-side bit 3 is SINGLE_STOP, not the requested mode bit. */
#include "jaguar/jrisc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define INSN(op,r1,r2) ((uint16_t)(((op)<<10)|((r1)<<5)|(r2)))

static uint32_t status(JRisc *j)
{
    uint32_t value=0u;
    assert(jrisc_local_read(j,j->base+0x114u,&value));
    return value;
}

static void check(int kind)
{
    JRisc j;
    JRiscBus bus={0};
    const uint32_t ctl=JRISC_CTRL_GO|JRISC_CTRL_SINGLE_STEP;
    const uint16_t a=INSN(35u,3u,0u); /* MOVEQ 3,R0 */
    const uint16_t b=INSN(35u,4u,1u); /* MOVEQ 4,R1 */
    const uint16_t c=INSN(0u,0u,1u);  /* ADD R0,R1 */
    const uint16_t d=INSN(57u,0u,0u); /* NOP */
    jrisc_init(&j,kind,bus);
    /* Synthetic program: high-word-first instruction fetch. */
    assert(jrisc_local_write(&j,j.base+0x10Cu,4u));
    assert(jrisc_local_write(&j,j.ram_base,((uint32_t)a<<16)|b));
    assert(jrisc_local_write(&j,j.ram_base+4u,((uint32_t)c<<16)|d));
    assert(jrisc_local_write(&j,j.base+0x110u,j.ram_base));

    /* A new step run executes its first instruction without SINGLE_GO. */
    assert(jrisc_local_write(&j,j.base+0x114u,ctl|JRISC_CTRL_SINGLE_GO));
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)==0u);
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][0]==3u && j.pc==j.ram_base+2u);
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)!=0u);
    assert((status(&j)&JRISC_CTRL_SINGLE_GO)==0u);

    /* A paused processor consumes no cycles, no instructions, and no PC. */
    uint64_t cycles=j.cycles, instructions=j.instructions;
    assert(jrisc_step(&j)==0u);
    assert(jrisc_run(&j,100u)==0u);
    assert(j.cycles==cycles && j.instructions==instructions);
    assert(j.pc==j.ram_base+2u && j.reg[0][1]==0u);

    /* Writing GO|STEP again alone does not advance a halted processor. */
    assert(jrisc_local_write(&j,j.base+0x114u,ctl));
    assert(jrisc_step(&j)==0u);
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)!=0u);

    /* One SINGLE_GO command permits exactly one more instruction. */
    assert(jrisc_local_write(&j,j.base+0x114u,ctl|JRISC_CTRL_SINGLE_GO));
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)==0u);
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][1]==4u && j.pc==j.ram_base+4u);
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)!=0u);
    assert(jrisc_step(&j)==0u);

    /* Leaving step mode while GO remains set resumes normal execution. */
    assert(jrisc_local_write(&j,j.base+0x114u,JRISC_CTRL_GO));
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)==0u);
    assert(jrisc_run(&j,2u)>=2u);
    assert(j.reg[0][1]==7u && j.pc==j.ram_base+8u);

    /* Stopped state is not falsely reported as SINGLE_STOP. */
    assert(jrisc_local_write(&j,j.base+0x114u,0u));
    assert((status(&j)&(JRISC_CTRL_GO|JRISC_CTRL_SINGLE_STEP))==0u);
    assert(jrisc_step(&j)==0u);
}

static void check_jump_slot(int kind)
{
    JRisc j;
    JRiscBus bus={0};
    const uint32_t ctl=JRISC_CTRL_GO|JRISC_CTRL_SINGLE_STEP;
    const uint16_t jump=INSN(52u,2u,0u); /* JUMP (R2), condition always */
    const uint16_t slot=INSN(35u,5u,0u); /* MOVEQ 5,R0 (delay slot) */
    const uint16_t skip=INSN(35u,6u,1u); /* skipped */
    const uint16_t dest=INSN(35u,7u,3u); /* target */
    jrisc_init(&j,kind,bus);
    assert(jrisc_local_write(&j,j.base+0x10Cu,4u));
    assert(jrisc_local_write(&j,j.ram_base,((uint32_t)jump<<16)|slot));
    assert(jrisc_local_write(&j,j.ram_base+4u,((uint32_t)skip<<16)|dest));
    j.reg[0][2]=j.ram_base+6u;
    assert(jrisc_local_write(&j,j.base+0x114u,ctl));

    /* JUMP itself is one instruction.  Its delay-slot remains pending. */
    assert(jrisc_step(&j)>0u);
    assert(j.pc==j.ram_base+2u && j.delay_pending==1);
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)!=0u);
    assert(jrisc_step(&j)==0u && j.reg[0][0]==0u);

    /* The second step executes only the mandatory delay-slot instruction. */
    assert(jrisc_local_write(&j,j.base+0x114u,ctl|JRISC_CTRL_SINGLE_GO));
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][0]==5u);
    assert(j.delay_pending==0 && j.pc==j.ram_base+6u);
    assert((status(&j)&JRISC_CTRL_SINGLE_STEP)!=0u);
    assert(jrisc_step(&j)==0u && j.reg[0][3]==0u);

    assert(jrisc_local_write(&j,j.base+0x114u,ctl|JRISC_CTRL_SINGLE_GO));
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][1]==0u && j.reg[0][3]==7u);
    assert(j.pc==j.ram_base+8u);
}

int main(void)
{
    check(JRISC_GPU);
    check(JRISC_DSP);
    check_jump_slot(JRISC_GPU);
    check_jump_slot(JRISC_DSP);
    puts("R239 GPU/DSP SINGLE_STEP/SINGLE_GO/SINGLE_STOP regression PASS");
    return 0;
}
