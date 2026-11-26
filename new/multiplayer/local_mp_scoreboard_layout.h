#ifndef AVP_LOCAL_MP_SCOREBOARD_LAYOUT_H
#define AVP_LOCAL_MP_SCOREBOARD_LAYOUT_H
#include <stdint.h>
#include "local_mp.h"

typedef struct AvpMpDmScoreLayout {
    unsigned columns,rows,cell_width,panel_height;
    unsigned first,last,page,pages;
} AvpMpDmScoreLayout;

/* Host-only Deathmatch score overlay, independent of Jaguar simulation.
 * A 320x228 window still fits 36 ordinary P# 0/0 entries, in 3 columns.
 * If scores become too wide or the window too small, rotate through pages
 * rather than silently truncating entries above player 16. */
static inline AvpMpDmScoreLayout avp_mp_dm_score_layout(
    unsigned width,unsigned height,unsigned cell_width,unsigned players,
    uint64_t elapsed_ticks)
{
    AvpMpDmScoreLayout result={0};
    unsigned rows_fit,capacity,total_on_page;
    if(!cell_width)cell_width=92u;
    if(players>AVP_LOCAL_MP_MAX_ACTORS)players=AVP_LOCAL_MP_MAX_ACTORS;
    result.cell_width=cell_width;
    result.columns=width>8u?(width-8u)/cell_width:1u;
    if(!result.columns)result.columns=1u;
    if(result.columns>AVP_LOCAL_MP_MAX_ACTORS)
        result.columns=AVP_LOCAL_MP_MAX_ACTORS;
    rows_fit=height>42u?1u+(height-43u)/16u:1u;
    if(!rows_fit)rows_fit=1u;
    capacity=result.columns*rows_fit;
    if(!capacity)capacity=1u;
    result.pages=players?(players+capacity-1u)/capacity:1u;
    result.page=(unsigned)((elapsed_ticks/50u)%result.pages);
    result.first=result.page*capacity;
    result.last=result.first+capacity;
    if(result.last>players)result.last=players;
    total_on_page=result.last-result.first;
    result.rows=total_on_page?(total_on_page+result.columns-1u)/result.columns:0u;
    result.panel_height=24u+result.rows*16u;
    if(result.panel_height>height)result.panel_height=height;
    return result;
}
#endif
