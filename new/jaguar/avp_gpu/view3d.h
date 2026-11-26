/* RE24 Session 2 (Claude): native C port of the original AvP 3D-view renderer.
 *
 * This is hand-written, human-readable C ported from the retail game's Jaguar
 * GPU view program (the resident sequencer at $F03000 and its overlays:
 * stage 0 = view setup, object transform and floor/ceiling spans; stage 1 =
 * per-column walls; stage 2 = depth-sorted objects).  It is NOT generated
 * from the ROM and contains no ROM bytes.  Like the original, it computes
 * Blitter commands and drives the Blitter, reading the 68000's Jaguar-layout
 * scene memory ($7F80-$7FBC parameter block, maze, object lists, textures).
 *
 * The register-faithful style (named after the original JRISC registers where
 * a value lives in a register across stages or frames) is deliberate: the
 * original program reads some registers it did not set in the same view
 * (values left from the previous view or by the GPU start-up code), and
 * 1:1 output requires carrying exactly those values.  They are explicit in
 * AvpView3DState rather than hidden.
 *
 * Verification: tools/jagref/view3d_port_check.c runs the original GPU code
 * on the TRM JRISC core and this port from the same captured state and
 * requires identical Blitter command streams and identical DRAM.
 */
#ifndef AVP_VIEW3D_H
#define AVP_VIEW3D_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Big-endian Jaguar bus as seen by the GPU: DRAM, cartridge ROM ($800000)
 * and Blitter registers ($F02200-$F022FF, 32-bit writes start blits on
 * B_CMD).  width is 1, 2 or 4. */
typedef struct AvpView3DBus {
    void *ctx;
    uint32_t (*rd)(void *ctx,uint32_t addr,int width);
    void (*wr)(void *ctx,uint32_t addr,uint32_t value,int width);
} AvpView3DBus;

/* GPU register state that the view program carries between stages and
 * between views.  alt[] is the GPU's secondary register bank (bank 1, set up
 * once by the GPU start-up code and then used for cross-stage values);
 * r[] holds the primary-bank registers whose previous contents the original
 * code can observe. */
typedef struct AvpView3DState {
    uint32_t alt[32];
    uint32_t r[32];
    uint32_t remain;   /* DIV remainder register */
    int divoff;        /* G_DIVCTRL bit 0 (16.16 offset division) */
    /* Head of stage 2's depth-sorted object list.  The original keeps it in
     * a GPU local-RAM longword ($F03594); its address is also stored in the
     * DRAM records as the "previous" link of the first object. */
    uint32_t sort_head;
    /* nonzero if the port reached a state the original cannot (diagnostic) */
    uint32_t fault;
    /* coverage counters (diagnostic only, never read by the renderer) */
    uint32_t wall_type_hits[16];
    uint32_t objects_drawn,object_columns;
} AvpView3DState;

/* GPU local-RAM address of the stage-2 list head, as stored in DRAM links */
#define AVP_VIEW3D_SORT_HEAD_ADDR  0xF03594u

/* Original 68000 variables that the game patches into the stage MOVEIs
 * before each view ($ED04/$ED22/$ED52).  The port reads them from the same
 * DRAM addresses. */
#define AVP_VIEW3D_VAR_FLOOR_TEX   0x0309F2u   /* stage 0 $F03270 */
#define AVP_VIEW3D_VAR_SPRITE_PARM 0x023F2Cu   /* stage 2 $F03220 */
/* stage 1: texture bases per wall handler and per-column height tables */
#define AVP_VIEW3D_VAR_WALL_TEX    0x0309EEu   /* $F0359A plain walls */
#define AVP_VIEW3D_VAR_SHAPED_TEX  0x0309F6u   /* $F0364C */
#define AVP_VIEW3D_VAR_DOOR4_TEX   0x0309FAu   /* $F0373C */
#define AVP_VIEW3D_VAR_SLIDE5_TEX  0x0309FEu   /* $F038D2 */
#define AVP_VIEW3D_VAR_SLIDE6_TEX  0x030A02u   /* $F03878 */
#define AVP_VIEW3D_VAR_RISE7_TEX   0x030A06u   /* $F03926 */
#define AVP_VIEW3D_VAR_SPLIT8_TEX  0x030A0Au   /* $F039C0 */
#define AVP_VIEW3D_VAR_DOOR9_TEX   0x030A0Eu   /* $F03A8E */
#define AVP_VIEW3D_VAR_DOOR11_TEX  0x030A12u   /* $F03B14 */
#define AVP_VIEW3D_VAR_SHAPED_HT   0x030A16u   /* $F03646 */
#define AVP_VIEW3D_VAR_DOOR4_HT    0x030A1Au   /* $F03736 */
#define AVP_VIEW3D_VAR_SLIDE6_HT   0x030A1Eu   /* $F03872 */
#define AVP_VIEW3D_VAR_DOOR9_HT    0x030A22u   /* $F03A88 */
#define AVP_VIEW3D_VAR_DOOR11_HT   0x030A26u   /* $F03B0E */
/* cartridge tables read by stage 1 */
#define AVP_VIEW3D_ROM_COLUMN_ANGLE 0x815604u
#define AVP_VIEW3D_ROM_COLUMN_COS   0x815000u
#define AVP_VIEW3D_ROM_STEP_A       0x815A06u
#define AVP_VIEW3D_ROM_STEP_B       0x815402u
/* per-column wall/clip records written by stage 1, read by stage 2 */
#define AVP_VIEW3D_COLUMN_TABLE     0x4000u
#define AVP_VIEW3D_COLUMN_POOL      0x4A00u

/* The GPU divide unit as used by the view program (exposed for tests). */
uint32_t avp_view3d_div(uint32_t dividend,uint32_t divisor,int offset_mode,uint32_t *remainder);

/* Stage-0 view setup ($F03220 in the original overlay). */
void avp_view3d_setup(AvpView3DState *s,const AvpView3DBus *bus);
/* Stage-0 object transform ($F0367C). */
void avp_view3d_transform_objects(AvpView3DState *s,const AvpView3DBus *bus);
/* Stage-0 floor and ceiling spans ($F03266). */
void avp_view3d_floor(AvpView3DState *s,const AvpView3DBus *bus);
/* Stage 0 as the original sequencer runs it ($F03000-$F0305E). */
void avp_view3d_stage0(AvpView3DState *s,const AvpView3DBus *bus);
/* Stage 1: walls, one ray per screen column. */
void avp_view3d_stage1(AvpView3DState *s,const AvpView3DBus *bus);
/* Stage 2: depth-sort the transformed objects and draw them ($F03220 of the
 * third overlay), preceded by the sequencer's overlay load and A2_BASE. */
void avp_view3d_stage2(AvpView3DState *s,const AvpView3DBus *bus);

#ifdef __cplusplus
}
#endif
#endif
