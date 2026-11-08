#ifndef AVP_NEW_MAP_MAKER_H
#define AVP_NEW_MAP_MAKER_H
/* PC-only editable copies of Jaguar's eight-byte-cell maze records.
 * The game, retail ROM, doors, AMP placement and campaign state are NEVER
 * modified by this module. The AVPMAP01 format is a staging format, not ROM.
 */
#include "avp_types.h"
#include "map_maker_scripts.h"
#include <stddef.h>
#define AVP_MM_LEVELS 15u
#define AVP_MM_DIM 64u
#define AVP_MM_CELL 8u
#define AVP_MM_MAX_BYTES (AVP_MM_DIM*AVP_MM_DIM*AVP_MM_CELL)
typedef struct AvpMapMakerSource {
    u8 *original;
    u16 w,h;
} AvpMapMakerSource;
#define AVP_MM_MAX_OBJECTS 64u
typedef struct AvpMmObjectRecord {u16 x8,y8;u8 def,group;} AvpMmObjectRecord;
#define AVP_MM_SELECTION_MAX 128u
typedef struct AvpMmSelectedFace {u16 x,y;u8 field;} AvpMmSelectedFace;
struct AvpMmHistory;
typedef struct AvpMapMaker {
    AvpMapMakerSource source[AVP_MM_LEVELS+1u];
    struct AvpMmHistory *edit_history; /* host-only bounded undo; never serialized */
    /* One private, per-selected-level editable metadata index, not game data. */
    AvpMmScriptRow script_rows[AVP_MM_MAX_SCRIPTS];
    unsigned script_count,script_modified;
    u32 script_source_crc; /* bound original map fingerprint */
    u8 source_header[8];
    u8 cells[AVP_MM_MAX_BYTES];
    u16 w,h;
    unsigned level,x,y,field;
    int dirty,rom_loaded,editor_started,custom_map;
    AvpMmObjectRecord objects[AVP_MM_MAX_OBJECTS];
    unsigned object_count,object_def,object_group;
    int palette_open; unsigned palette_id; /* property / texture ID selector */
    /* PC-only one-key wall brush. Never written to a retail map or AVPM draft. */
    int wall_tool; /* compatibility flag: true only in 2D Walls mode */
    unsigned edit_mode_2d; /* 0 cells, 1 walls, 2 objects, 3 textures; host-only */
    unsigned texture_target_2d; /* 0 floor, 1 ceiling, 2 wall */
    u8 texture_brush[3]; /* separate raw Jaguar IDs per surface type */
    int selection_2d_valid; /* UI selection only, never serialized */
    int selection_2d_range; /* Shift+arrow rectangular selection, UI only */
    unsigned select_anchor_x,select_anchor_y;
    int playtest_no_enemies; /* launch-only; not written to .avpm */
    u8 wall_brush;
    int wall_drag_lock; /* keep zoomed 2D overview fixed while mouse paints */
    unsigned wall_drag_x,wall_drag_y;
    /* One predictable face per drag stroke; mouse motion is interpolated
     * over any skipped cells, but never changes the drag direction. */
    unsigned wall_drag_side,wall_last_x,wall_last_y;
    int wall_last_valid;
    /* Pointer-only feedback: previewed edge is not placed or serialized. */
    unsigned wall_hover_x,wall_hover_y,wall_hover_side;
    int wall_hover_valid;
    int mode_3d; /* 0: editable overview, 1: PC Jaguar-rendered walk preview */
    int overhead_ceiling; /* textured overhead: 0=floor, 1=ceiling */
    unsigned overhead_zoom; /* 1..28 wheel notches, PC-only */
    double overhead_zoom_visual; /* eased current notch; independent of target */
    double overhead_anchor_cell_x,overhead_anchor_cell_y; /* floating tile anchor */
    int overhead_anchor_px,overhead_anchor_py; /* same position in preview pixels */
    int overhead_anchor_valid; /* cursor-anchored zoom, independent of selection */
    unsigned char copied_texture,has_copied_texture;
    unsigned copied_field,copied_level; /* translate between wall and flat sets */
    double camera_pitch; /* host-only editor look pitch in radians */
    int context_open, browser_open, name_edit_open;
    unsigned browser_count,browser_cursor;
    char browser_files[128][96];
    char map_stem[96], edit_stem[96];
    int ghost_walls; /* allow selecting zero-texture wall slots in 3D */
    int highlight_3d; /* PC-only selected/hovered surface outline */
    double camera_x,camera_y,camera_heading; /* radians; never passed to gameplay */
    unsigned target_x,target_y,target_field;
    int target_valid;
    /* Persistent 3D selection; hover target above is independent of clicks. */
    AvpMmSelectedFace selected[AVP_MM_SELECTION_MAX];
    unsigned selected_count;
    /* Host-only selected staged objects, indexed by the current .avpo array.
     * Selection is never written to the map or retail game records. */
    u8 selected_objects[AVP_MM_MAX_OBJECTS];
    double view_ray_scale; /* horizontal FOV matches active native renderer */
    double cursor_u,cursor_v; /* normalized pointer on preview (0..1) */
    char message[120];
} AvpMapMaker;
/* One Ctrl+Z step per logical edit; UI brackets continuous drags as a unit. */
void avp_mm_history_begin(AvpMapMaker *m);
void avp_mm_history_commit(AvpMapMaker *m);
void avp_mm_history_reset(AvpMapMaker *m);
int avp_mm_undo(AvpMapMaker *m);
int avp_mm_redo(AvpMapMaker *m);
void avp_mm_init(AvpMapMaker *m);
void avp_mm_destroy(AvpMapMaker *m);
int avp_mm_bind(AvpMapMaker *m,unsigned level,const u8 *decoded,size_t len);
int avp_mm_select(AvpMapMaker *m,unsigned level);
int avp_mm_blank(AvpMapMaker *m);
void avp_mm_move(AvpMapMaker *m,int dx,int dy);
void avp_mm_extend_2d_selection(AvpMapMaker *m,int dx,int dy);
/* Shift + left mouse drag: the clicked cell becomes the fixed anchor;
 * moving the pointer updates the other corner without editing map bytes. */
int avp_mm_begin_2d_drag_selection(AvpMapMaker *m,unsigned x,unsigned y);
int avp_mm_update_2d_drag_selection(AvpMapMaker *m,unsigned x,unsigned y);
int avp_mm_in_2d_selection(const AvpMapMaker *m,unsigned x,unsigned y);
int avp_mm_paint_2d_cell(AvpMapMaker *m,unsigned x,unsigned y,unsigned side);
/* Texture mode queries never modify geometry. Mix is true when any
 * selected cell has a differing raw ID in the active surface slot. */
unsigned avp_mm_texture_field(const AvpMapMaker *m);
const char *avp_mm_texture_target_name(const AvpMapMaker *m);
void avp_mm_cycle_texture_target(AvpMapMaker *m);
void avp_mm_texture_brush_select(AvpMapMaker *m,u8 id);
int avp_mm_texture_selection(const AvpMapMaker *m,u8 *id,int *mixed);
int avp_mm_texture_paint_selection(AvpMapMaker *m);
unsigned avp_mm_delete_2d_selection(AvpMapMaker *m);
int avp_mm_adjust(AvpMapMaker *m,int delta);
int avp_mm_assign(AvpMapMaker *m,u8 value);
u8 avp_mm_value(const AvpMapMaker *m);
const char *avp_mm_name(unsigned level);
/* Native DOORS.S InitDoors / MAZE2 wall-byte classification.  Bits 6-3
 * choose one of 16 wall handlers, bits 2-0 choose the panel variant, and
 * bit 7 mirrors the wall artwork.  ID 0 and 128 are empty surfaces. */
const char *avp_mm_panel_kind(u8 panel);
unsigned avp_mm_panel_type(u8 panel);
/* T-palette quick jumps for wall fields only; assigns nothing. */
int avp_mm_palette_category_start(unsigned field,unsigned key,unsigned *id);
int avp_mm_save(AvpMapMaker *m,const char *path);
int avp_mm_load(AvpMapMaker *m,const char *path);
void avp_mm_toggle_3d(AvpMapMaker *m);
void avp_mm_camera_motion(AvpMapMaker *m,double forward,double strafe,double turn);
/* Left-click replaces; either Shift+click toggles a surface in the set. */
int avp_mm_select_target(AvpMapMaker *m,int append);
/* Drag additions are idempotent, unlike Shift+click's deliberate toggle. */
int avp_mm_select_target_drag(AvpMapMaker *m);
int avp_mm_select_object(AvpMapMaker *m,unsigned index,int append,int toggle);
unsigned avp_mm_selected_object_count(const AvpMapMaker *m);
unsigned avp_mm_delete_selected_objects(AvpMapMaker *m);
void avp_mm_clear_selection(AvpMapMaker *m);
int avp_mm_mirror_target(AvpMapMaker *m);
void avp_mm_toggle_ghost_walls(AvpMapMaker *m);
void avp_mm_toggle_highlight(AvpMapMaker *m);
void avp_mm_toggle_overhead_surface(AvpMapMaker *m);
int avp_mm_pick(AvpMapMaker *m,double u,double v);
int avp_mm_assign_target(AvpMapMaker *m,u8 value);
int avp_mm_adjust_target(AvpMapMaker *m,int delta);
/* Inspect the retail-derived working metadata overlay. */
unsigned avp_mm_script_flags(const AvpMapMaker *m,unsigned x,unsigned y,unsigned side);
int avp_mm_inspect_script(AvpMapMaker *m);
/* Spawn must be inside an authored floor; final native SafePos is checked by game. */
int avp_mm_test_start_valid(const AvpMapMaker *m,double x,double y);
int avp_mm_place_floor(AvpMapMaker *m);
int avp_mm_delete_cell(AvpMapMaker *m);
int avp_mm_delete_flat(AvpMapMaker *m);
/* Rotate the currently selected cell wall/panel 90 degrees.  Positive is
 * clockwise in the editor overhead (top -> right -> bottom -> left).
 * Door opening bytes follow the canonical left/top edge owner. */
int avp_mm_rotate_wall(AvpMapMaker *m,int clockwise);
/* Explicit, guarded editor-only rotation with offset-only metadata move.
 * Must not be mistaken for playable runtime script relocation. */
int avp_mm_relocate_scripted_wall(AvpMapMaker *m,int clockwise);
/* Simple 2D brush: W toggles, left click/drag places on empty cell edges,
 * right click/drag erases, comma/period rotates selected wall or empty brush
 * orientation. Scripted edges and adjacent owner conflicts are protected. */
void avp_mm_toggle_wall_tool(AvpMapMaker *m);
void avp_mm_cycle_2d_mode(AvpMapMaker *m);
const char *avp_mm_2d_mode_name(const AvpMapMaker *m);
int avp_mm_deselect_target(AvpMapMaker *m);
void avp_mm_wall_brush_select(AvpMapMaker *m,u8 panel);
int avp_mm_wall_place(AvpMapMaker *m,unsigned x,unsigned y,unsigned side);
int avp_mm_wall_erase(AvpMapMaker *m,unsigned x,unsigned y,unsigned side);
int avp_mm_wall_turn(AvpMapMaker *m,int clockwise);
/* Call once per pointer event while dragging, then end on mouse release.
 * Returns 0 if any requested edge could not safely be modified. */
int avp_mm_wall_stroke(AvpMapMaker *m,unsigned x,unsigned y,unsigned side,int erase);
/* Read-only brush preview: no geometry, state or script bytes are edited. */
int avp_mm_wall_placeable(const AvpMapMaker *m,unsigned x,unsigned y,unsigned side);
void avp_mm_wall_drag_end(AvpMapMaker *m);
int avp_mm_copy_surface(AvpMapMaker *m,unsigned x,unsigned y,unsigned field);
int avp_mm_paste_surface(AvpMapMaker *m,unsigned x,unsigned y,unsigned field);
int avp_mm_copy_target(AvpMapMaker *m);
int avp_mm_paste_target(AvpMapMaker *m);
/* 3D Enter-brush: paste to the currently aimed face once per stroke,
 * without modifying the user's explicit multi-face selection. The callback
 * is the PC host's original guarded, cross-texture-table paste path. */
#define AVP_MM_3D_PAINT_VISITED_BYTES ((AVP_MM_DIM*AVP_MM_DIM*6u+7u)/8u)
typedef int (*AvpMmPaintFace)(AvpMapMaker *m,unsigned x,unsigned y,
                              unsigned field,void *context);
int avp_mm_paint_3d_target_once(AvpMapMaker *m,u8 *visited,size_t visited_bytes,
                                AvpMmPaintFace paste,void *context);

void avp_mm_zoom(AvpMapMaker *m,int direction);
void avp_mm_camera_pitch(AvpMapMaker *m,double delta);
#endif
