/* R231: F12 Back/Delete isolation fixture.  Every Map Maker call below is
 * unreachable in this keyboard/menu navigation test. Aborting makes a
 * mistaken cross-subsystem transition a test failure, not a silent mock.
 * Live Map Maker behavior is exercised by its own production-linked tests. */
#include <stdlib.h>
#include "map_maker.h"
#include "map_maker_highlight.h"
#include "map_maker_objects.h"
#include "map_maker_overhead.h"
#include "map_maker_playtest.h"
#include "map_maker_scripts.h"
#include "map_maker_texture_browser.h"
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

int avp_mm_adjust(AvpMapMaker *m,int delta)
{ abort(); return 0; }

int avp_mm_adjust_target(AvpMapMaker *m,int delta)
{ abort(); return 0; }

int avp_mm_assign(AvpMapMaker *m,u8 value)
{ abort(); return 0; }

int avp_mm_assign_target(AvpMapMaker *m,u8 value)
{ abort(); return 0; }

void avp_mm_camera_pitch(AvpMapMaker *m,double delta)
{ abort(); }

void avp_mm_clear_selection(AvpMapMaker *m)
{ abort(); }

int avp_mm_copy_surface(AvpMapMaker *m,unsigned x,unsigned y,unsigned field)
{ abort(); return 0; }

int avp_mm_copy_target(AvpMapMaker *m)
{ abort(); return 0; }

void avp_mm_cycle_2d_mode(AvpMapMaker *m)
{ abort(); }

void avp_mm_cycle_texture_target(AvpMapMaker *m)
{ abort(); }

unsigned avp_mm_delete_2d_selection(AvpMapMaker *m)
{ abort(); return 0; }

unsigned avp_mm_delete_selected_objects(AvpMapMaker *m)
{ abort(); return 0; }

void avp_mm_history_begin(AvpMapMaker *m)
{ abort(); }

int avp_mm_inspect_script(AvpMapMaker *m)
{ abort(); return 0; }

int avp_mm_mirror_target(AvpMapMaker *m)
{ abort(); return 0; }

void avp_mm_move(AvpMapMaker *m,int dx,int dy)
{ abort(); }

unsigned avp_mm_object_catalog_count(void)
{ abort(); return 0; }

unsigned avp_mm_object_catalog_def(unsigned id)
{ abort(); return 0; }

unsigned avp_mm_object_catalog_id(unsigned amp_def)
{ abort(); return 0; }

int avp_mm_object_place(AvpMapMaker *m,double x,double y)
{ abort(); return 0; }

int avp_mm_object_remove(AvpMapMaker *m,unsigned x,unsigned y)
{ abort(); return 0; }

int avp_mm_objects_load(AvpMapMaker *m,const char *path)
{ abort(); return 0; }

int avp_mm_objects_save(const AvpMapMaker *m,const char *path)
{ abort(); return 0; }

int avp_mm_palette_category_start(unsigned field,unsigned key,unsigned *id)
{ abort(); return 0; }

int avp_mm_paste_surface(AvpMapMaker *m,unsigned x,unsigned y,unsigned field)
{ abort(); return 0; }

int avp_mm_pick(AvpMapMaker *m,double u,double v)
{ abort(); return 0; }

int avp_mm_place_floor(AvpMapMaker *m)
{ abort(); return 0; }

int avp_mm_relocate_scripted_wall(AvpMapMaker *m,int clockwise)
{ abort(); return 0; }

int avp_mm_rotate_wall(AvpMapMaker *m,int clockwise)
{ abort(); return 0; }

int avp_mm_save(AvpMapMaker *m,const char *path)
{ abort(); return 0; }

unsigned avp_mm_selected_object_count(const AvpMapMaker *m)
{ abort(); return 0; }

int avp_mm_test_start_valid(const AvpMapMaker *m,double x,double y)
{ abort(); return 0; }

void avp_mm_texture_brush_select(AvpMapMaker *m,u8 id)
{ abort(); }

unsigned avp_mm_texture_field(const AvpMapMaker *m)
{ abort(); return 0; }

int avp_mm_texture_paint_selection(AvpMapMaker *m)
{ abort(); return 0; }

void avp_mm_toggle_3d(AvpMapMaker *m)
{ abort(); }

void avp_mm_toggle_ghost_walls(AvpMapMaker *m)
{ abort(); }

void avp_mm_toggle_highlight(AvpMapMaker *m)
{ abort(); }

void avp_mm_toggle_overhead_surface(AvpMapMaker *m)
{ abort(); }

void avp_mm_toggle_wall_tool(AvpMapMaker *m)
{ abort(); }

u8 avp_mm_value(const AvpMapMaker *m)
{ abort(); return 0; }

void avp_mm_wall_brush_select(AvpMapMaker *m,u8 panel)
{ abort(); }

int avp_mm_wall_place(AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{ abort(); return 0; }

int avp_mm_wall_turn(AvpMapMaker *m,int clockwise)
{ abort(); return 0; }

int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file_id)
{(void)level;(void)wall;(void)file_id;abort();}
int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file_id)
{(void)level;(void)flat;(void)file_id;abort();}
