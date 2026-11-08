/* Source-authored AVP door/exit/access metadata reconstructed from
 * MAZE/LEVDOOR.S. This is game metadata, not PC platform state.
 * Offsets are byte offsets into the original 64x64 x 8-byte maze layout.
 */
#include "doors.h"

static const u16 inter1_0_required[]={3223u};
static const u16 inter5_0_required[]={3279u};
static const u16 inter14_0_required[]={10951u};
static const u16 inter15_0_required[]={14007u};

static const AvpDoorAccessEntry access1[]={
    {1175u,AVP_ACC_EXIT},
    {1231u,AVP_ACC_UNUSED},
    {26287u,AVP_ACC_UNUSED},
    {5807u,6},
    {5815u,6},
    {15639u,6},
    {22191u,6},
    {22199u,6},
};
static const AvpDoorAccessEntry access2[]={
    {10415u,9},
    {6262u,9},
    {6398u,9},
    {4271u,9},
    {2735u,9},
    {6238u,3},
    {15454u,3},
    {11927u,7},
    {11983u,7},
    {19663u,7},
    {19607u,7},
    {21167u,7},
    {23806u,7},
    {15646u,4},
    {6430u,4},
    {9510u,5},
    {11086u,5},
    {12622u,7},
    {12582u,9},
    {18198u,AVP_ACC_JAMMED},
};
static const AvpDoorAccessEntry access3[]={
    {26295u,AVP_ACC_UNUSED},
    {13999u,1},
    {14023u,1},
    {4246u,2},
    {4294u,2},
    {21687u,6},
    {20094u,AVP_ACC_JAMMED},
    {21127u,AVP_ACC_JAMMED},
};
static const AvpDoorAccessEntry access4[]={
    {11070u,4},
    {10055u,5},
    {10079u,5},
    {12639u,7},
    {12615u,9},
    {9503u,3},
    {13087u,3},
    {10998u,3},
    {2239u,3},
    {2295u,3},
    {2359u,3},
    {2407u,3},
    {3998u,7},
    {7582u,3},
    {11166u,3},
    {18846u,3},
    {22375u,3},
    {22303u,3},
    {3750u,3},
    {10894u,3},
    {3726u,7},
    {14382u,7},
    {17950u,AVP_ACC_JAMMED},
    {20510u,AVP_ACC_JAMMED},
    {22087u,AVP_ACC_JAMMED},
    {14982u,AVP_ACC_JAMMED},
};
static const AvpDoorAccessEntry access5[]={
    {1175u,AVP_ACC_UNUSED},
    {1231u,AVP_ACC_EXIT},
    {24255u,AVP_ACC_JAMMED},
    {26287u,AVP_ACC_ESCAPE},
    {5807u,6},
    {5815u,6},
    {22191u,6},
    {22199u,6},
};
static const AvpDoorAccessEntry access14[]={
    {12999u,AVP_ACC_EXIT},
};
static const AvpDoorAccessEntry access15[]={
    {16055u,AVP_ACC_EXIT},
};

static const AvpDoorExitEntry exit1[]={
    {2064u,3u,4u,6u},
    {6465u,17u,4u,6u},
    {19266u,17u,20u,6u},
    {23056u,3u,18u,7u},
    {1169u,22u,27u,15u},
};
static const AvpDoorExitEntry exit2[]={
    {2640u,3u,4u,7u},
    {2834u,18u,4u,8u},
    {20257u,16u,17u,8u},
    {23147u,6u,16u,8u},
    {21266u,15u,18u,7u},
};
static const AvpDoorExitEntry exit3[]={
    {1746u,8u,4u,10u},
    {3609u,4u,3u,8u},
    {15506u,9u,5u,9u},
    {17947u,4u,21u,10u},
    {21034u,4u,20u,8u},
    {22640u,9u,21u,9u},
    {23306u,16u,20u,8u},
};
static const AvpDoorExitEntry exit4[]={
    {2576u,5u,7u,11u},
    {4355u,11u,5u,10u},
    {4450u,14u,7u,10u},
    {11040u,14u,14u,10u},
    {13011u,7u,4u,12u},
    {16691u,16u,16u,11u},
    {16977u,5u,18u,10u},
    {17801u,10u,19u,12u},
    {20259u,15u,23u,10u},
    {20619u,6u,17u,12u},
};
static const AvpDoorExitEntry exit5[]={
    {2064u,4u,4u,13u},
    {6465u,15u,7u,13u},
    {19266u,18u,14u,12u},
    {23056u,8u,19u,13u},
    {1225u,24u,21u,14u},
};
static const AvpDoorExitEntry exit6[]={
    {2074u,2u,4u,1u},
    {2184u,40u,12u,1u},
    {10376u,40u,37u,1u},
};
static const AvpDoorExitEntry exit7[]={
    {2074u,10u,5u,2u},
    {9242u,2u,45u,1u},
    {9336u,34u,41u,2u},
};
static const AvpDoorExitEntry exit8[]={
    {1571u,3u,7u,3u},
    {2192u,34u,5u,2u},
    {8241u,13u,45u,2u},
    {8835u,36u,39u,2u},
    {10272u,5u,41u,3u},
    {10368u,33u,45u,3u},
};
static const AvpDoorExitEntry exit9[]={
    {2632u,18u,30u,3u},
    {10826u,14u,44u,3u},
};
static const AvpDoorExitEntry exit10[]={
    {2112u,26u,3u,3u},
    {2649u,32u,8u,4u},
    {3696u,44u,8u,4u},
    {7282u,36u,21u,4u},
    {9259u,10u,33u,4u},
    {10785u,3u,35u,3u},
    {11897u,36u,39u,4u},
};
static const AvpDoorExitEntry exit11[]={
    {3626u,2u,5u,4u},
    {8321u,38u,32u,4u},
};
static const AvpDoorExitEntry exit12[]={
    {2105u,26u,25u,4u},
    {7312u,40u,37u,5u},
    {8753u,17u,40u,4u},
    {9811u,49u,34u,4u},
};
static const AvpDoorExitEntry exit13[]={
    {2082u,2u,4u,5u},
    {3707u,40u,12u,5u},
    {9794u,2u,45u,5u},
};
static const AvpDoorExitEntry exit14[]={
    {12483u,25u,5u,5u},
};
static const AvpDoorExitEntry exit15[]={
    {15539u,18u,5u,1u},
};

static const AvpDoorInterlockEntry inter1[]={
    {1175u,inter1_0_required,sizeof(inter1_0_required)/sizeof(inter1_0_required[0])},
};
static const AvpDoorInterlockEntry inter5[]={
    {1231u,inter5_0_required,sizeof(inter5_0_required)/sizeof(inter5_0_required[0])},
};
static const AvpDoorInterlockEntry inter14[]={
    {12999u,inter14_0_required,sizeof(inter14_0_required)/sizeof(inter14_0_required[0])},
};
static const AvpDoorInterlockEntry inter15[]={
    {16055u,inter15_0_required,sizeof(inter15_0_required)/sizeof(inter15_0_required[0])},
};

static const AvpDoorLevelMeta retail_meta[15]={
    {access1,sizeof(access1)/sizeof(access1[0]),exit1,sizeof(exit1)/sizeof(exit1[0]),inter1,sizeof(inter1)/sizeof(inter1[0])}, /* level 1 */
    {access2,sizeof(access2)/sizeof(access2[0]),exit2,sizeof(exit2)/sizeof(exit2[0]),NULL,0}, /* level 2 */
    {access3,sizeof(access3)/sizeof(access3[0]),exit3,sizeof(exit3)/sizeof(exit3[0]),NULL,0}, /* level 3 */
    {access4,sizeof(access4)/sizeof(access4[0]),exit4,sizeof(exit4)/sizeof(exit4[0]),NULL,0}, /* level 4 */
    {access5,sizeof(access5)/sizeof(access5[0]),exit5,sizeof(exit5)/sizeof(exit5[0]),inter5,sizeof(inter5)/sizeof(inter5[0])}, /* level 5 */
    {NULL,0,exit6,sizeof(exit6)/sizeof(exit6[0]),NULL,0}, /* level 6 */
    {NULL,0,exit7,sizeof(exit7)/sizeof(exit7[0]),NULL,0}, /* level 7 */
    {NULL,0,exit8,sizeof(exit8)/sizeof(exit8[0]),NULL,0}, /* level 8 */
    {NULL,0,exit9,sizeof(exit9)/sizeof(exit9[0]),NULL,0}, /* level 9 */
    {NULL,0,exit10,sizeof(exit10)/sizeof(exit10[0]),NULL,0}, /* level 10 */
    {NULL,0,exit11,sizeof(exit11)/sizeof(exit11[0]),NULL,0}, /* level 11 */
    {NULL,0,exit12,sizeof(exit12)/sizeof(exit12[0]),NULL,0}, /* level 12 */
    {NULL,0,exit13,sizeof(exit13)/sizeof(exit13[0]),NULL,0}, /* level 13 */
    {access14,sizeof(access14)/sizeof(access14[0]),exit14,sizeof(exit14)/sizeof(exit14[0]),inter14,sizeof(inter14)/sizeof(inter14[0])}, /* level 14 */
    {access15,sizeof(access15)/sizeof(access15[0]),exit15,sizeof(exit15)/sizeof(exit15[0]),inter15,sizeof(inter15)/sizeof(inter15[0])}, /* level 15 */
};

void avp_doors_bind_retail_meta(void)
{
    avp_doors_bind_level_meta(retail_meta,sizeof(retail_meta)/sizeof(retail_meta[0]));
}
