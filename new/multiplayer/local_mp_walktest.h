#ifndef AVP_LOCAL_MP_WALKTEST_H
#define AVP_LOCAL_MP_WALKTEST_H
#include "pc/pc_video.h"
#include "pc/pc_resources.h"
/* Original one-window native co-op runtime. The campaign and each player's
 * species are independent. Additional spawn positions use original SafePos. */
/* Return codes: 0 = user exited, 1 = fault, 2 = campaign victory faded out. */
enum { AVP_MP_RUN_EXIT=0, AVP_MP_RUN_ERROR=1, AVP_MP_RUN_COMPLETED=2, AVP_MP_RUN_TITLE=3, AVP_MP_RUN_RESTART=4 };
int avp_mp_walktest_run(AvpPcVideo *video, unsigned level,
                        const AvpMazeCandidate *retail_level,
                        const AvpMazeCandidate *all_mazes,unsigned maze_count,
                        const AvpRomImage *rom,
                        int combat_test, unsigned players,
                        const AvpLocalMpSetup *requested);
#endif
