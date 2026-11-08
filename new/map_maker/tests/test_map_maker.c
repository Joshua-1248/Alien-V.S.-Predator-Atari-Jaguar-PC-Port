#include "map_maker.h"
#include "map_maker_scripts.h"
#include "map_maker_overhead.h"
#include "map_maker_highlight.h"
#include "map_maker_texture_browser.h"
#include "map_maker_objects.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int checks=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int fake_flat(void *ignored,unsigned level,unsigned flat,AvpMmOverheadTexture *t)
{
    static const u8 four[12]={255,0,0, 0,255,0, 0,0,255, 255,255,255};
    static const u8 one[3]={97,101,107};
    (void)ignored;(void)level;
    t->rgb=flat==3u?one:four;
    t->width=t->height=flat==3u?1u:2u;
    t->rotation=flat==4u?0u:1u;
    return 1;
}
static int fake_browser(void *context,unsigned level,unsigned id,unsigned field,AvpMmOverheadTexture *t)
{(void)field;return fake_flat(context,level,id,t);}
static unsigned staged_sprite_calls=0;
static int fake_stage_sprite(void *context,unsigned def,AvpMmOverheadSprite *sp)
{
    const uint32_t *rgba=(const uint32_t *)context;
    staged_sprite_calls++;
    if(def!=21u)return 0; /* unmapped object: keep its marker */
    sp->rgba=rgba;
    sp->width=8u;sp->height=8u;
    sp->x0=0u;sp->y0=0u;sp->x1=8u;sp->y1=8u;
    return 1;
}
static int fake_3d_stroke_paste(AvpMapMaker *m,unsigned x,unsigned y,
                                 unsigned field,void *context)
{
    unsigned *calls=(unsigned *)context;
    (*calls)++;
    if(x==11u)return 0; /* e.g. a script-protected destination */
    return avp_mm_paste_surface(m,x,y,field);
}
int main(void)
{
    AvpMapMaker m={0},n={0};
    u8 source[8u+8u*8u*8u]={0};
    const char *path="test_map_maker_roundtrip.avpm";
    remove(path);remove("test_map_maker_custom.avpm");
    avp_mm_init(&m);avp_mm_init(&n);
    /* R48: original sprite thumbnails can be drawn on the zoomed 2D map
     * without replacing placement markers at normal zoom or changing map
     * bytes. Every preview is scissored and the selection border stays on top. */
    {
        AvpMapMaker q={0};AvpMmOverheadLayout l;
        const unsigned width=1280u,height=720u;
        uint32_t *preview=calloc((size_t)width*height,sizeof *preview);
        uint32_t sprite[64];unsigned char before[AVP_MM_MAX_BYTES];
        CHECK(preview!=NULL);
        for(unsigned i=0;i<64u;i++)sprite[i]=0xff1379bdu;
        avp_mm_init(&q);CHECK(avp_mm_blank(&q));
        q.x=20u;q.y=20u;
        q.object_count=2u;
        q.objects[0]=(AvpMmObjectRecord){(u16)(20u*256u+128u),
                (u16)(20u*256u+128u),21u,0u};
        q.objects[1]=(AvpMmObjectRecord){(u16)(21u*256u+128u),
                (u16)(20u*256u+128u),59u,1u};
        memcpy(before,q.cells,sizeof before);
        staged_sprite_calls=0;
        avp_mm_render_overhead_with_sprites(&q,preview,width,height,
                                      fake_flat,sprite,fake_stage_sprite);
        CHECK(staged_sprite_calls==0u); /* overview: crosses, no sprite fetch */
        q.overhead_zoom_visual=6.0;
        CHECK(avp_mm_overhead_layout(&q,width,height,&l));
        CHECK(l.cell_px>=14u);
        staged_sprite_calls=0;
        avp_mm_render_overhead_with_sprites(&q,preview,width,height,
                                      fake_flat,sprite,fake_stage_sprite);
        CHECK(staged_sprite_calls==2u);
        {
            int cx=l.x0+(int)(((uint64_t)q.objects[0].x8*l.cell_px)/256u);
            int cy=l.y0+(int)(((uint64_t)q.objects[0].y8*l.cell_px)/256u);
            int ux=l.x0+(int)(((uint64_t)q.objects[1].x8*l.cell_px)/256u);
            int uy=l.y0+(int)(((uint64_t)q.objects[1].y8*l.cell_px)/256u);
            CHECK(cx-2>=l.clip_x0&&cy-2>=l.clip_y0);
            CHECK(preview[(size_t)(cy-2)*width+(unsigned)(cx-2)]==0x1379bdu);
            CHECK(preview[(size_t)uy*width+(unsigned)ux]==0xe869e5u);
            CHECK(q.dirty==0&&memcmp(before,q.cells,sizeof before)==0);
            q.selection_2d_valid=1;q.selection_2d_range=1;
            q.select_anchor_x=20u;q.select_anchor_y=20u;
            avp_mm_render_overhead_with_sprites(&q,preview,width,height,
                                      fake_flat,sprite,fake_stage_sprite);
            CHECK(preview[(size_t)(l.y0+20u*l.cell_px)*width+
                  (unsigned)(l.x0+20u*l.cell_px+1)]==0x46dcecu ||
                  preview[(size_t)(l.y0+20u*l.cell_px)*width+
                  (unsigned)(l.x0+20u*l.cell_px+1)]==0xffdf4au);
            CHECK(preview[(size_t)(cy-2)*width+(unsigned)(cx-2)]==0x1379bdu);
        }
        avp_mm_destroy(&q);free(preview);
    }
    /* R38: independent Floor/Ceiling/Wall texture brushes, selection Mix,
     * non-destructive mode switching, and atomic multi-cell painting. */
    {
        AvpMapMaker p={0};u8 id=0;int mixed=0;
        avp_mm_init(&p);
        CHECK(avp_mm_blank(&p));
        CHECK(p.edit_mode_2d==0u);
        avp_mm_cycle_2d_mode(&p);CHECK(p.edit_mode_2d==1u);
        avp_mm_cycle_2d_mode(&p);CHECK(p.edit_mode_2d==2u);
        CHECK(avp_mm_begin_2d_drag_selection(&p,4u,4u));
        CHECK(avp_mm_update_2d_drag_selection(&p,5u,4u));
        avp_mm_cycle_2d_mode(&p);CHECK(p.edit_mode_2d==3u);
        CHECK(p.selection_2d_range&&p.select_anchor_x==4u);
        CHECK(avp_mm_texture_field(&p)==5u);
        avp_mm_texture_brush_select(&p,13u);
        CHECK(avp_mm_texture_paint_selection(&p));
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+5u]==13u);
        CHECK(p.cells[((size_t)4u*64u+5u)*8u+5u]==13u);
        CHECK(avp_mm_texture_selection(&p,&id,&mixed)&&id==13u&&!mixed);
        p.cells[((size_t)4u*64u+5u)*8u+5u]=14u;
        CHECK(avp_mm_texture_selection(&p,&id,&mixed)&&mixed);
        p.cells[((size_t)4u*64u+5u)*8u+5u]=13u;
        avp_mm_cycle_texture_target(&p);
        CHECK(avp_mm_texture_field(&p)==4u&&p.overhead_ceiling);
        avp_mm_texture_brush_select(&p,29u);
        CHECK(avp_mm_texture_paint_selection(&p));
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+4u]==29u);
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+5u]==13u);
        avp_mm_cycle_texture_target(&p);
        CHECK(avp_mm_texture_field(&p)==1u);
        p.field=2u;
        CHECK(avp_mm_texture_field(&p)==2u);
        avp_mm_texture_brush_select(&p,43u);
        CHECK(avp_mm_texture_paint_selection(&p));
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+2u]==43u);
        CHECK(p.cells[((size_t)4u*64u+5u)*8u+2u]==43u);
        CHECK(p.cells[((size_t)4u*64u+5u)*8u+4u]==29u);
        CHECK(p.texture_brush[0]==13u&&p.texture_brush[1]==29u&&p.texture_brush[2]==43u);
        CHECK(avp_mm_begin_2d_drag_selection(&p,4u,4u));
        CHECK(avp_mm_update_2d_drag_selection(&p,5u,4u));
        avp_mm_history_begin(&p);
        avp_mm_texture_brush_select(&p,44u);
        CHECK(avp_mm_texture_paint_selection(&p));
        avp_mm_history_commit(&p);
        CHECK(avp_mm_undo(&p));
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+2u]==43u);
        CHECK(avp_mm_redo(&p));
        CHECK(p.cells[((size_t)4u*64u+5u)*8u+2u]==44u);
        CHECK(p.selection_2d_range);
        avp_mm_history_begin(&p);
        CHECK(avp_mm_delete_2d_selection(&p)==2u);
        avp_mm_history_commit(&p);
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+2u]==0u);
        CHECK(p.cells[((size_t)4u*64u+5u)*8u+2u]==0u);
        CHECK(p.cells[((size_t)4u*64u+4u)*8u+5u]==13u);
        CHECK(avp_mm_undo(&p));
        CHECK(p.cells[((size_t)4u*64u+5u)*8u+2u]==44u);
        avp_mm_cycle_2d_mode(&p);CHECK(p.edit_mode_2d==0u&&p.selection_2d_range);
        avp_mm_destroy(&p);
    }
    /* R34: held 3D Enter follows the aimed face, not the saved selection,
     * and each distinct face is only pasted once per continuous stroke. */
    {
        AvpMapMaker p={0};
        u8 visited[AVP_MM_3D_PAINT_VISITED_BYTES]={0};
        unsigned calls=0;
        avp_mm_init(&p);
        CHECK(avp_mm_blank(&p));
        p.mode_3d=1;p.has_copied_texture=1;p.copied_texture=19u;
        p.target_x=10u;p.target_y=10u;p.target_field=1u;p.target_valid=1;
        p.selected_count=1u;p.selected[0]=(AvpMmSelectedFace){7u,7u,0u};
        avp_mm_history_begin(&p);
        CHECK(avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        CHECK(calls==1u && p.cells[((size_t)10u*64u+10u)*8u+1u]==19u);
        CHECK(p.selected_count==1u&&p.selected[0].x==7u);
        CHECK(!avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        CHECK(calls==1u);
        p.target_field=2u;
        CHECK(avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        CHECK(p.cells[((size_t)10u*64u+10u)*8u+2u]==19u);
        p.target_x=11u;
        CHECK(!avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        CHECK(p.cells[((size_t)10u*64u+11u)*8u+2u]==0u);
        p.target_x=12u;p.target_field=1u;
        CHECK(avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        p.target_field=6u;
        CHECK(!avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        p.target_field=1u;p.target_valid=0;
        CHECK(!avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        p.target_valid=1;
        CHECK(!avp_mm_paint_3d_target_once(&p,visited,1u,
                                            fake_3d_stroke_paste,&calls));
        avp_mm_history_commit(&p);
        CHECK(avp_mm_undo(&p));
        CHECK(p.cells[((size_t)10u*64u+10u)*8u+1u]==0u);
        CHECK(p.cells[((size_t)10u*64u+10u)*8u+2u]==0u);
        CHECK(p.cells[((size_t)10u*64u+12u)*8u+1u]==0u);
        CHECK(avp_mm_redo(&p));
        CHECK(p.cells[((size_t)10u*64u+12u)*8u+1u]==19u);
        /* Release then press again: repainting is a new undoable stroke. */
        memset(visited,0,sizeof visited);
        CHECK(avp_mm_paint_3d_target_once(&p,visited,sizeof visited,
                                            fake_3d_stroke_paste,&calls));
        avp_mm_destroy(&p);
    }
    /* R35: mouse-range selection anchors once, is reversible, doesn't edit
     * cells/objects and works through Walls/Objects mode changes. */
    {
        AvpMapMaker p={0};
        u8 before[AVP_MM_MAX_BYTES];
        avp_mm_init(&p);
        CHECK(avp_mm_blank(&p));
        memcpy(before,p.cells,sizeof before);
        CHECK(!avp_mm_begin_2d_drag_selection(&p,64u,3u));
        CHECK(!p.selection_2d_range);
        CHECK(avp_mm_begin_2d_drag_selection(&p,12u,14u));
        CHECK(p.x==12u && p.y==14u && p.select_anchor_x==12u && p.select_anchor_y==14u);
        CHECK(p.selection_2d_valid && p.selection_2d_range);
        CHECK(avp_mm_update_2d_drag_selection(&p,19u,22u));
        CHECK(avp_mm_in_2d_selection(&p,12u,14u));
        CHECK(avp_mm_in_2d_selection(&p,19u,22u));
        CHECK(!avp_mm_in_2d_selection(&p,11u,14u));
        CHECK(avp_mm_update_2d_drag_selection(&p,9u,7u));
        CHECK(p.select_anchor_x==12u && p.select_anchor_y==14u);
        CHECK(avp_mm_in_2d_selection(&p,9u,7u));
        CHECK(!avp_mm_in_2d_selection(&p,19u,22u));
        CHECK(!avp_mm_update_2d_drag_selection(&p,65u,7u));
        CHECK(p.x==9u && p.y==7u);
        avp_mm_cycle_2d_mode(&p);
        CHECK(p.edit_mode_2d==1u && p.selection_2d_range);
        CHECK(avp_mm_update_2d_drag_selection(&p,17u,20u));
        avp_mm_cycle_2d_mode(&p);
        CHECK(p.edit_mode_2d==2u && p.selection_2d_range);
        CHECK(avp_mm_in_2d_selection(&p,17u,20u));
        CHECK(memcmp(before,p.cells,sizeof before)==0 && p.object_count==0u);
        avp_mm_toggle_3d(&p);
        CHECK(p.mode_3d && p.selection_2d_range);
        CHECK(!avp_mm_update_2d_drag_selection(&p,1u,1u));
        CHECK(avp_mm_in_2d_selection(&p,12u,14u));
        avp_mm_toggle_3d(&p);
        avp_mm_clear_selection(&p);
        CHECK(!p.selection_2d_range && !p.selection_2d_valid);
        /* R36: while holding Shift+left drag at close zoom, edge scrolling
         * continues even when no new mouse-motion events arrive. Selection
         * changes without modifying a single Jaguar cell or object. */
        {
            AvpMmOverheadLayout l,after;
            unsigned start_x,start_y,cx,cy,initial_edge_x=0;
            int panned=0,edge_x,edge_y;
            CHECK(avp_mm_overhead_layout(&p,1280u,720u,&l));
            /* Pointer-anchored zoom around the middle of the visible canvas. */
            for(unsigned i=0;i<24u;i++)
                avp_mm_overhead_zoom_at(&p,1280u,720u,800,330,1);
            for(unsigned i=0;i<120u;i++)avp_mm_overhead_zoom_tick(&p,1.0/60.0);
            CHECK(avp_mm_overhead_layout(&p,1280u,720u,&l));
            CHECK(l.cell_px>=96u);
            CHECK(avp_mm_overhead_hit(&p,1280u,720u,800,330,&start_x,&start_y));
            CHECK(avp_mm_begin_2d_drag_selection(&p,start_x,start_y));
            edge_x=l.clip_x1-4;edge_y=330;
            CHECK(avp_mm_overhead_hit(&p,1280u,720u,edge_x,edge_y,
                                      &initial_edge_x,&cy));
            CHECK(!avp_mm_overhead_selection_auto_pan(&p,1280u,720u,
                                      10,edge_y,24,&cx,&cy)); /* HUD is inert */
            CHECK(!avp_mm_overhead_selection_auto_pan(&p,1280u,720u,
                                      800,330,24,&cx,&cy)); /* center is inert */
            CHECK(!avp_mm_overhead_selection_auto_pan(&p,1280u,720u,
                                      edge_x,edge_y,0,&cx,&cy)); /* zero dt */
            for(unsigned i=0;i<40u;i++){
                if(avp_mm_overhead_selection_auto_pan(&p,1280u,720u,
                                         edge_x,edge_y,32,&cx,&cy)){
                    CHECK(avp_mm_update_2d_drag_selection(&p,cx,cy));
                    panned++;
                }
            }
            CHECK(panned>0);
            CHECK(p.x>initial_edge_x); /* selection expands past visible edge */
            CHECK(p.select_anchor_x==start_x && p.select_anchor_y==start_y);
            CHECK(avp_mm_in_2d_selection(&p,start_x,start_y));
            CHECK(memcmp(before,p.cells,sizeof before)==0 && !p.object_count);
            CHECK(avp_mm_overhead_layout(&p,1280u,720u,&after));
            avp_mm_clear_selection(&p);
            CHECK(!avp_mm_overhead_selection_auto_pan(&p,1280u,720u,
                                      edge_x,edge_y,24,&cx,&cy));
            CHECK(avp_mm_overhead_layout(&p,1280u,720u,&l));
            CHECK(l.x0==after.x0 && l.y0==after.y0); /* not panning without drag */
        }
        avp_mm_destroy(&p);
    }
    /* RE32: holding Enter paints a batch in the active 2D mode. Only the
     * active layer may change; the complete hold is one undo transaction. */
    {
        AvpMapMaker p={0};
        size_t a=((size_t)10u*64u+10u)*8u;
        avp_mm_init(&p);
        CHECK(avp_mm_blank(&p));
        CHECK(p.edit_mode_2d==0u);
        avp_mm_history_begin(&p);
        CHECK(avp_mm_paint_2d_cell(&p,10u,10u,1u));
        CHECK(avp_mm_paint_2d_cell(&p,11u,10u,1u));
        CHECK(avp_mm_paint_2d_cell(&p,12u,10u,1u));
        avp_mm_history_commit(&p);
        CHECK(p.cells[a+5u]==1u && p.cells[a+8u+5u]==1u && p.cells[a+16u+5u]==1u);
        CHECK(avp_mm_undo(&p));
        CHECK(p.cells[a+5u]==0u && p.cells[a+8u+5u]==0u && p.cells[a+16u+5u]==0u);
        CHECK(avp_mm_redo(&p));
        CHECK(p.cells[a+5u]==1u && p.cells[a+8u+5u]==1u && p.cells[a+16u+5u]==1u);
        p.x=10u;p.y=10u;
        avp_mm_extend_2d_selection(&p,2,0);
        avp_mm_extend_2d_selection(&p,0,1);
        CHECK(p.selection_2d_range&&p.x==12u&&p.y==11u);
        CHECK(avp_mm_in_2d_selection(&p,10u,10u));
        CHECK(avp_mm_in_2d_selection(&p,12u,11u));
        CHECK(!avp_mm_in_2d_selection(&p,13u,10u));
        CHECK(avp_mm_delete_2d_selection(&p)==3u);
        CHECK(p.cells[a+5u]==0u && p.cells[a+8u+5u]==0u && p.cells[a+16u+5u]==0u);
        avp_mm_clear_selection(&p);
        CHECK(!p.selection_2d_range && !p.selection_2d_valid);
        avp_mm_cycle_2d_mode(&p);
        CHECK(p.edit_mode_2d==1u);
        avp_mm_wall_brush_select(&p,1u);
        CHECK(avp_mm_paint_2d_cell(&p,10u,10u,1u));
        CHECK(p.cells[a+1u]==1u && p.cells[a+5u]==0u);
        CHECK(avp_mm_paint_2d_cell(&p,11u,10u,1u));
        p.x=10u;p.y=10u;p.field=1u;p.selection_2d_valid=1;
        CHECK(avp_mm_delete_2d_selection(&p)==1u);
        CHECK(p.cells[a+1u]==0u && p.cells[a+8u+1u]==1u);
        avp_mm_cycle_2d_mode(&p);
        CHECK(p.edit_mode_2d==2u);
        CHECK(!avp_mm_paint_2d_cell(&p,10u,10u,1u)); /* no floor */
        p.cells[a+5u]=1u;
        CHECK(avp_mm_paint_2d_cell(&p,10u,10u,1u));
        CHECK(p.object_count==1u && p.objects[0].def==21u);
        CHECK(avp_mm_paint_2d_cell(&p,10u,10u,1u));
        CHECK(p.object_count==1u); /* update existing marker; no duplicate */
        CHECK(avp_mm_delete_2d_selection(&p)==1u);
        CHECK(p.object_count==0u && p.cells[a+5u]==1u);
        {AvpMmOverheadLayout l={0};unsigned bx,by;
         CHECK(avp_mm_overhead_layout(&p,1280u,720u,&l));
         CHECK(avp_mm_overhead_hit(&p,1280u,720u,800,300,&bx,&by));
         avp_mm_overhead_zoom_at(&p,1280u,720u,800,300,1);
         for(unsigned j=0;j<60u;j++)avp_mm_overhead_zoom_tick(&p,1.0/60.0);
         {unsigned ax,ay;
          CHECK(avp_mm_overhead_hit(&p,1280u,720u,800,300,&ax,&ay));
          CHECK(ax==bx&&ay==by);}
         for(unsigned j=0;j<40u;j++)avp_mm_overhead_zoom_at(&p,1280u,720u,800,300,1);
         for(unsigned j=0;j<150u;j++)avp_mm_overhead_zoom_tick(&p,1.0/60.0);
         CHECK(p.overhead_zoom==28u);
         CHECK(avp_mm_overhead_layout(&p,1280u,720u,&l));
         CHECK(l.cell_px>48u && l.cell_px<=256u);
         /* An extreme zoom must not paint or hit-test under the left HUD.
          * The screen-space map scissor is independent of the panned origin. */
         CHECK(l.clip_x0>=260 && l.clip_x1<=1280);
         CHECK(!avp_mm_overhead_hit(&p,1280u,720u,50,300,NULL,NULL));
         CHECK(!avp_mm_overhead_hit(&p,1280u,720u,900,20,NULL,NULL));
         {unsigned prev=p.overhead_zoom;
          avp_mm_overhead_zoom_at(&p,1280u,720u,50,300,-1);
          CHECK(p.overhead_zoom==prev);}
         {uint32_t *screen=malloc((size_t)1280u*720u*sizeof(uint32_t));
          CHECK(screen!=NULL);
          avp_mm_overhead_pan(&p,1280u,720u,-400,0);
          avp_mm_render_overhead(&p,screen,1280u,720u,fake_flat,NULL);
          CHECK(screen[(size_t)300u*1280u+50u]==0x080a0du);
          CHECK(screen[(size_t)20u*1280u+900u]==0x080a0du);
          free(screen);}
        }
        avp_mm_destroy(&p);
    }
    CHECK(avp_mm_object_catalog_count()==55u);
    CHECK(avp_mm_object_catalog_def(0u)==21u);
    CHECK(avp_mm_object_catalog_id(21u)==0u);
    CHECK(!strcmp(avp_mm_object_catalog_name(0u),"Pickup: Food"));
    CHECK(avp_mm_object_catalog_def(1u)==0u); /* retail AMP0 is ALIEN */
    CHECK(!strcmp(avp_mm_object_name(0u),"ALIEN"));
    CHECK(avp_mm_object_catalog_id(0u)==1u);
    CHECK(avp_mm_object_catalog_def(55u)==60u);
    CHECK(avp_mm_object_catalog_id(6u)==55u); /* projectile not placeable */
    CHECK(m.object_def==21u); /* start brush is editor Object 0: Food */

    /* Native MAZE2 wall handlers and DOORS.S InitDoors group split: the
     * browser must never mislabel the unrendered type-12 group as an arch. */
    CHECK(!strcmp(avp_mm_panel_kind(0u),"Empty surface"));
    CHECK(!strcmp(avp_mm_panel_kind(0x80u),"Empty surface"));
    CHECK(!strcmp(avp_mm_panel_kind(1u),"Solid wall"));
    CHECK(!strcmp(avp_mm_panel_kind(0x81u),"Solid wall"));
    CHECK(!strcmp(avp_mm_panel_kind(0x18u),"Shaped solid wall"));
    CHECK(!strcmp(avp_mm_panel_kind(0x20u),"Door panel"));
    CHECK(!strcmp(avp_mm_panel_kind(0xd8u),"Door panel"));
    CHECK(!strcmp(avp_mm_panel_kind(0x5fu),"Door panel"));
    CHECK(!strcmp(avp_mm_panel_kind(0x60u),"Unrendered/unused"));
    CHECK(!strcmp(avp_mm_panel_kind(0x68u),"Partial/strip panel"));
    CHECK(!strcmp(avp_mm_panel_kind(0xf8u),"Partial/strip panel"));
    CHECK(avp_mm_panel_type(0xeau)==13u);
    CHECK(avp_mm_panel_type(0x80u)==0u);
    {unsigned jump=250u;
     CHECK(avp_mm_palette_category_start(0u,'w',&jump)&&jump==1u);
     CHECK(avp_mm_palette_category_start(1u,'S',&jump)&&jump==24u);
     CHECK(avp_mm_palette_category_start(2u,'d',&jump)&&jump==32u);
     CHECK(avp_mm_palette_category_start(3u,'A',&jump)&&jump==104u);
     CHECK(!avp_mm_palette_category_start(4u,'d',&jump)&&jump==104u);
     CHECK(!avp_mm_palette_category_start(0u,'?',&jump)&&jump==104u);
     CHECK(!avp_mm_palette_category_start(0u,'a',NULL));
    }
    CHECK(!avp_mm_bind(&m,0u,source,sizeof source));
    CHECK(!avp_mm_bind(&m,16u,source,sizeof source));
    CHECK(!avp_mm_bind(&m,1u,source,7u));
    source[1]=8u;source[3]=8u;source[5]=2u;
    for(size_t i=8u;i<sizeof source;i+=8u){source[i+4]=3u;source[i+5]=4u;}
    CHECK(avp_mm_bind(&m,3u,source,sizeof source));
    CHECK(avp_mm_select(&m,3u));
    CHECK(m.w==8u&&m.h==8u);
    CHECK(avp_mm_value(&m)==0u);
    CHECK(m.source[3].original[8+5]==4u);
    m.field=5u;CHECK(avp_mm_value(&m)==4u);
    CHECK(avp_mm_assign(&m,19u));CHECK(m.dirty);
    CHECK(source[8+5]==4u); /* never change retail source */
    CHECK(m.source[3].original[8+5]==4u); /* never change retained original */
    avp_mm_move(&m,999,999);CHECK(m.x==7u&&m.y==7u);
    avp_mm_move(&m,-999,-999);CHECK(m.x==0u&&m.y==0u);
    CHECK(avp_mm_save(&m,path));CHECK(!m.dirty);
    CHECK(avp_mm_select(&m,3u));CHECK(avp_mm_value(&m)==0u);
    m.field=5u;CHECK(avp_mm_value(&m)==4u);
    CHECK(avp_mm_load(&m,path));CHECK(m.field==0u);
    m.field=5u;CHECK(avp_mm_value(&m)==19u);
    CHECK(m.source[3].original[8+5]==4u);
    CHECK(avp_mm_blank(&m));
    CHECK(m.w==64u && m.h==64u && !m.dirty);
    CHECK(avp_mm_value(&m)==0u); /* truly empty blank canvas */
    CHECK(m.custom_map && m.x==32u && m.y==32u && m.field==5u);
    CHECK(!avp_mm_test_start_valid(&m,32.5,32.5));
    CHECK(avp_mm_place_floor(&m));CHECK(avp_mm_value(&m)==1u);
    CHECK(avp_mm_test_start_valid(&m,32.5,32.5));
    {
        const size_t pos=((size_t)32u*64u+32u)*8u;
        m.cells[pos+1u]=3u;
        m.selection_2d_valid=1;
        avp_mm_clear_selection(&m);
        CHECK(!m.selection_2d_valid);
        CHECK(avp_mm_delete_flat(&m));
        CHECK(m.cells[pos+5u]==0u);
        CHECK(m.cells[pos+1u]==3u); /* walls preserved in Cells mode */
        m.overhead_ceiling=1;
        m.cells[pos+4u]=7u;
        CHECK(avp_mm_delete_flat(&m));
        CHECK(m.cells[pos+4u]==0u && m.cells[pos+1u]==3u);
        m.overhead_ceiling=0;
        m.cells[pos+5u]=1u; /* restore original fixture for later tests */
        m.cells[pos+1u]=0u;
    }

    CHECK(avp_mm_copy_surface(&m,32u,32u,5u));CHECK(m.copied_texture==1u);
    m.x=33u;CHECK(avp_mm_paste_surface(&m,33u,32u,5u));
    CHECK(m.cells[((size_t)32u*m.w+33u)*8u+5u]==1u);
    CHECK(avp_mm_delete_cell(&m));
    CHECK(m.cells[((size_t)32u*m.w+33u)*8u+5u]==0u);
    avp_mm_zoom(&m,1);CHECK(m.overhead_zoom==2u);
    avp_mm_zoom(&m,-1);CHECK(m.overhead_zoom==1u);
    CHECK(avp_mm_save(&m,"test_map_maker_custom.avpm"));
    CHECK(avp_mm_select(&m,3u));
    CHECK(avp_mm_load(&m,"test_map_maker_custom.avpm"));CHECK(m.custom_map);
    CHECK(m.cells[((size_t)32u*m.w+32u)*8u+5u]==1u);
    CHECK(m.source[3].original[8+5]==4u); /* still unchanged */
    CHECK(!avp_mm_select(&m,0u));CHECK(!avp_mm_select(&m,16u));
    CHECK(avp_mm_select(&m,15u));CHECK(m.w==64u&&m.h==64u);
    m.x=63u;m.y=63u;m.field=3u;CHECK(avp_mm_value(&m)==0u);
    CHECK(!avp_mm_load(&m,path)); /* wrong level is rejected */
    CHECK(avp_mm_select(&m,3u));
    CHECK(avp_mm_load(&m,path));
    CHECK(avp_mm_bind(&n,3u,source,sizeof source));
    CHECK(avp_mm_select(&n,3u));
    CHECK(!avp_mm_load(&n,"file_that_should_not_exist.avpm"));
    {FILE*f=fopen(path,"r+b");CHECK(f!=NULL);CHECK(fseek(f,-1,SEEK_END)==0);
     CHECK(fputc(0xFF,f)!=EOF);CHECK(fclose(f)==0);}
    m.field=5u;CHECK(avp_mm_value(&m)==19u);
    CHECK(!avp_mm_load(&m,path)); /* corrupt CRC leaves buffer unchanged */
    CHECK(avp_mm_value(&m)==19u);
    /* BUILD-style editor: a detached camera, separate pick target, and edits
     * that cannot reach the retail maze or currently active game map. */
    CHECK(avp_mm_select(&m,3u));
    CHECK(!m.mode_3d);
    avp_mm_toggle_3d(&m);CHECK(m.mode_3d);
    m.camera_x=3.5;m.camera_y=3.5;m.camera_heading=0.0;
    CHECK(!avp_mm_pick(&m,NAN,0.5)); /* no invalid cursor */
    m.cells[((size_t)3u*m.w+4u)*8u]=7u; /* left wall of east room */
    CHECK(avp_mm_pick(&m,0.5,0.5));
    CHECK(m.target_x==3u && m.target_y==3u && m.target_field==2u);
    CHECK(avp_mm_assign_target(&m,33u));
    CHECK(avp_mm_copy_target(&m));CHECK(m.copied_texture==33u);
    CHECK(avp_mm_paste_target(&m));
    CHECK(m.cells[((size_t)3u*m.w+3u)*8u+2u]==33u && m.dirty);
    CHECK(m.cells[((size_t)3u*m.w+4u)*8u]==7u); /* the far face is untouched */
    CHECK(m.source[3].original[8u+((size_t)3u*m.w+4u)*8u]==0u);
    /* R32: scrolling/painting a wall to ID zero must NEVER toggle Ghost
     * Walls on. The targeted surface and explicit selections are editable
     * while zero; manually enabling Ghost Walls still allows empty picks. */
    CHECK(!m.ghost_walls);
    CHECK(avp_mm_assign_target(&m,0u));CHECK(!m.ghost_walls);
    CHECK(m.target_x==3u&&m.target_y==3u&&m.target_field==2u);
    CHECK(m.cells[((size_t)3u*m.w+3u)*8u+2u]==0u);
    CHECK(avp_mm_assign_target(&m,9u));CHECK(!m.ghost_walls);
    CHECK(m.cells[((size_t)3u*m.w+3u)*8u+2u]==9u);
    avp_mm_toggle_ghost_walls(&m);CHECK(m.ghost_walls);
    CHECK(avp_mm_assign_target(&m,0u));CHECK(m.ghost_walls);
    CHECK(avp_mm_pick(&m,0.5,0.5));
    CHECK(m.target_x==3u&&m.target_y==3u&&m.target_field==2u);
    avp_mm_toggle_ghost_walls(&m);CHECK(!m.ghost_walls);
    /* Preserve the original preference when two sides encode a wall. */
    m.ghost_walls=0;
    m.cells[((size_t)3u*m.w+3u)*8u+2u]=21u;
    m.cells[((size_t)3u*m.w+4u)*8u+0u]=13u;
    CHECK(avp_mm_pick(&m,0.5,0.5));
    CHECK(m.target_x==3u&&m.target_field==2u);
    m.cells[((size_t)3u*m.w+3u)*8u+2u]=0u;
    CHECK(avp_mm_pick(&m,0.5,0.5));
    CHECK(m.target_x==3u&&m.target_field==2u);
    /* Approaching the same boundary from the EAST must select its
     * independent west-facing panel, not the west cell's east texture. */
    m.cells[((size_t)3u*m.w+4u)*8u+0u]=15u;
    m.camera_x=4.5;m.camera_y=3.5;m.camera_heading=3.141592653589793;
    CHECK(avp_mm_pick(&m,0.5,0.5));
    CHECK(m.target_x==4u && m.target_y==3u && m.target_field==0u);
    CHECK(avp_mm_assign_target(&m,19u));
    CHECK(m.cells[((size_t)3u*m.w+4u)*8u]==19u);
    CHECK(m.cells[((size_t)3u*m.w+3u)*8u+2u]==0u);
    m.camera_heading=0.0;
    m.cells[((size_t)3u*m.w+4u)*8u+0u]=0u;
    m.ghost_walls=1;
    m.camera_x=7.5;m.camera_y=3.5;
    CHECK(avp_mm_pick(&m,0.5,0.5));
    CHECK(m.target_x==7u&&m.target_y==3u&&m.target_field==2u);
    m.camera_x=3.5;m.ghost_walls=0;
    CHECK(avp_mm_pick(&m,0.5,0.85) && m.target_field==5u);
    CHECK(avp_mm_pick(&m,0.5,0.15) && m.target_field==4u);
    CHECK(avp_mm_adjust_target(&m,1));
    CHECK(m.source[3].original[8u+((size_t)3u*m.w+4u)*8u+4u]==3u);
    {double ox=m.camera_x,oy=m.camera_y;
     avp_mm_camera_motion(&m,0.15,0.0,0.0);
     CHECK(m.camera_x>ox && m.camera_y==oy);
     avp_mm_camera_motion(&m,0.0,0.0,0.1);
     CHECK(m.camera_heading>0.0);
     avp_mm_camera_pitch(&m,0.25);CHECK(m.camera_pitch>0.0);
     avp_mm_camera_pitch(&m,10.0);CHECK(m.camera_pitch<=0.85);
     avp_mm_camera_pitch(&m,-20.0);CHECK(m.camera_pitch>=-0.85);
     m.camera_pitch=0.0;
     avp_mm_camera_motion(&m,20000.0,20000.0,0.0);
     CHECK(m.camera_x>=0.0 && m.camera_x<m.w && m.camera_y>=0.0 && m.camera_y<m.h);
    }
    /* R22: H toggles a genuinely projected wireframe around the targeted
     * wall, floor or ceiling. Turning it off leaves pixels alone; toggling
     * never edits the source ROM copy or the draft's authored bytes. */
    {
        const unsigned tw=640u,th=456u;
        uint32_t *pix=(uint32_t *)calloc((size_t)tw*th,sizeof *pix);
        u8 *saved=(u8 *)malloc((size_t)m.w*m.h*8u);
        unsigned hits;
        CHECK(pix!=NULL&&saved!=NULL);
        memcpy(saved,m.cells,(size_t)m.w*m.h*8u);
        m.camera_x=2.5;m.camera_y=3.5;m.camera_heading=0.0;
        m.view_ray_scale=1.0;m.camera_pitch=0.0;
        CHECK(m.highlight_3d);
        m.target_x=4u;m.target_y=3u;m.target_field=0u;m.target_valid=1;
        CHECK(avp_mm_draw_3d_highlight(&m,pix,tw,th));
        avp_mm_toggle_highlight(&m);CHECK(!m.highlight_3d);
        CHECK(!avp_mm_draw_3d_highlight(&m,pix,tw,th));
        avp_mm_toggle_highlight(&m);CHECK(m.highlight_3d);
        CHECK(avp_mm_draw_3d_highlight(&m,pix,tw,th));
        hits=0;
        for(size_t i=0;i<(size_t)tw*th;i++)hits+=(pix[i]==0x00ffd44au);
        CHECK(hits>150u && hits<2500u); /* only an outline, never a fill */
        for(unsigned field=0u;field<=5u;field++){
            memset(pix,0,(size_t)tw*th*sizeof *pix);
            m.target_x=4u;m.target_y=3u;m.target_field=field;
            /* A perimeter/flat can be viewed from different camera sides. */
            m.camera_x=field==0u?2.5:field==2u?5.5:field==1u?4.5:4.5;
            m.camera_y=field==1u?1.5:field==3u?5.5:field>=4u?2.5:3.5;
            m.camera_heading=field==1u||field>=4u?1.5707963267948966:
                             field==2u?3.141592653589793:
                             field==3u?-1.5707963267948966:0.0;
            CHECK(avp_mm_draw_3d_highlight(&m,pix,tw,th));
        }
        m.target_field=6u;CHECK(!avp_mm_draw_3d_highlight(&m,pix,tw,th));
        m.target_field=0u;m.target_x=m.w;
        CHECK(!avp_mm_draw_3d_highlight(&m,pix,tw,th));
        m.target_x=4u;m.target_valid=0;
        CHECK(!avp_mm_draw_3d_highlight(&m,pix,tw,th));
        m.target_valid=1;
        avp_mm_toggle_highlight(&m);CHECK(!m.highlight_3d);
        memset(pix,0,(size_t)tw*th*sizeof *pix);
        CHECK(!avp_mm_draw_3d_highlight(&m,pix,tw,th));
        {int untouched=1;
         for(size_t i=0;i<(size_t)tw*th;i++)if(pix[i]!=0u)untouched=0;
         CHECK(untouched);}
        CHECK(!memcmp(saved,m.cells,(size_t)m.w*m.h*8u));
        CHECK(m.source[3].original[8u+5u]==4u);
        /* A very close wall must still have a clipped visible border. */
        m.camera_x=3.5;m.camera_y=3.5;m.camera_heading=0.0;
        m.target_x=4u;m.target_y=3u;m.target_field=0u;
        avp_mm_toggle_highlight(&m);
        memset(pix,0,(size_t)tw*th*sizeof *pix);
        CHECK(avp_mm_draw_3d_highlight(&m,pix,tw,th));
        CHECK(pix[(size_t)1u*tw+1u]==0x00ffd44au);
        avp_mm_toggle_highlight(&m);
        free(saved);free(pix);
    }
    avp_mm_toggle_3d(&m);CHECK(!m.mode_3d);
    avp_mm_toggle_highlight(&m);CHECK(!m.highlight_3d); /* only 3D H toggles */
    avp_mm_camera_motion(&m,0.1,0.1,0.1);CHECK(!m.mode_3d);
    /* Pixel-exact overhead: four original RGB texels, nearest-neighbor
     * sampling, walls drawn independently, selection survives painting. */
    {
        AvpMmOverheadLayout l; unsigned xx=999,yy=999;
        uint32_t *pixels=(uint32_t *)calloc(640u*480u,sizeof(*pixels));
        CHECK(pixels!=NULL);
        CHECK(avp_mm_overhead_layout(&m,640u,480u,&l));
        CHECK(!avp_mm_overhead_hit(&m,640u,480u,l.x0-1,l.y0,&xx,&yy));
        CHECK(avp_mm_overhead_hit(&m,640u,480u,l.x0+3,l.y0+3,&xx,&yy));
        CHECK(xx==0u&&yy==0u);
        m.x=7u;m.y=7u;m.cells[5]=4u;m.cells[4]=3u;m.cells[3]=1u;
        avp_mm_render_overhead(&m,pixels,640u,480u,fake_flat,NULL);
        CHECK(pixels[(size_t)(l.y0+3)*640u+(unsigned)(l.x0+3)]==0xff0000u);
        CHECK(pixels[(size_t)(l.y0+3)*640u+(unsigned)(l.x0+(int)l.cell_px/2+1)]==0x00ff00u);
        CHECK(pixels[(size_t)(l.y0+(int)l.cell_px/2+1)*640u+(unsigned)(l.x0+3)]==0x0000ffu);
        CHECK(pixels[(size_t)(l.y0+(int)l.cell_px-1)*640u+(unsigned)(l.x0+3)]==0xb4c5d0u); /* south wall */
        avp_mm_toggle_overhead_surface(&m);CHECK(m.overhead_ceiling);
        avp_mm_render_overhead(&m,pixels,640u,480u,fake_flat,NULL);
        CHECK(pixels[(size_t)(l.y0+3)*640u+(unsigned)(l.x0+3)]==0x61656bu);
        CHECK(m.source[3].original[8u+5u]==4u);
        avp_mm_toggle_overhead_surface(&m);CHECK(!m.overhead_ceiling);
        free(pixels);
    }
    /* R20: playtest spawn guard and the visual palette's actual pixel data. */
    CHECK(avp_mm_test_start_valid(&m,3.5,3.5));
    CHECK(!avp_mm_test_start_valid(&m,-1.0,3.5));
    CHECK(!avp_mm_test_start_valid(&m,INFINITY,3.5));
    CHECK(!avp_mm_test_start_valid(&m,63.99,0.5));
    {AvpMmBrowserLayout l;unsigned id=0;
     uint32_t *pix=(uint32_t*)calloc(640u*456u,sizeof(*pix));
     CHECK(pix!=NULL);
     CHECK(avp_mm_browser_layout(640u,456u,&l));
     CHECK(!avp_mm_browser_hit(640u,456u,l.x0-1,l.y0,0,&id));
     CHECK(avp_mm_browser_hit(640u,456u,l.x0+2,l.y0+2,0,&id));
     CHECK(id==0u);
     CHECK(avp_mm_browser_hit(640u,456u,l.x0+2+(int)l.pitch*5,
                              l.y0+2+(int)l.pitch*4,1u,&id));
     CHECK(id==101u);
     m.palette_id=4u;m.field=5u;
     avp_mm_browser_draw(&m,pix,640u,456u,fake_browser,NULL);
     CHECK(pix[(size_t)(l.y0+2)*640u+(unsigned)(l.x0+2+(int)l.pitch*4)]==0xff0000u);
     CHECK(pix[(size_t)(l.y0+1)*640u+(unsigned)(l.x0+1+(int)l.pitch*4)]==0xffd54au);
     m.palette_id=0u;
     avp_mm_browser_draw(&m,pix,640u,456u,fake_browser,NULL);
     CHECK(pix[(size_t)(l.y0+2)*640u+(unsigned)(l.x0+2)]!=0xff0000u);
     free(pix);
    }
    /* Versioned object sidecar: exact original 8.8 coordinates and groups.
     * Corruption must not alter the currently loaded editor marker list. */
    {
        const char *objects_path="test_map_maker_objects.avpo";
        remove(objects_path);
        m.object_def=21u;m.object_group=3u;
        CHECK(avp_mm_object_place(&m,3.5,3.5));
        CHECK(m.object_count==1u);
        CHECK(m.objects[0].x8==3u*256u+128u);
        CHECK(m.objects[0].y8==3u*256u+128u);
        CHECK(m.objects[0].def==21u&&m.objects[0].group==3u);
        CHECK(avp_mm_objects_save(&m,objects_path));
        CHECK(avp_mm_objects_load(&n,objects_path));
        CHECK(n.object_count==1u && n.objects[0].x8==m.objects[0].x8);
        m.object_def=2u;m.object_group=1u;
        CHECK(avp_mm_object_place(&m,3.5,3.5)); /* replace exact cell */
        CHECK(m.object_count==1u&&m.objects[0].def==2u&&m.objects[0].group==1u);
        CHECK(!avp_mm_object_place(&m,1000.0,1.0));
        CHECK(!avp_mm_object_remove(&m,0u,0u));
        CHECK(avp_mm_object_remove(&m,3u,3u));
        CHECK(m.object_count==0u);
        {FILE *f=fopen(objects_path,"r+b");CHECK(f!=NULL);
         CHECK(fseek(f,-1,SEEK_END)==0);CHECK(fputc(0x1f,f)!=EOF);CHECK(!fclose(f));}
        CHECK(!avp_mm_objects_load(&n,objects_path));
        CHECK(n.object_count==1u); /* transactional load */
        remove(objects_path);
    }
    /* R23: rotation is a panel relocation across the four native sides,
     * including reflected panels, partial passage/arch types and doors. */
    {
        AvpMmOverheadLayout l;
        u8 *c,*east,*south; unsigned before;
        CHECK(avp_mm_blank(&m));
        m.x=m.y=20u;m.field=1u;
        c=m.cells+((size_t)20u*m.w+20u)*8u;
        east=c+8u;south=c+(size_t)m.w*8u;
        c[1]=0x81u; /* mirrored solid panel, top */
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==2u&&c[2]==0x81u&&c[1]==0u&&m.dirty);
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==3u&&c[3]==0x81u&&c[2]==0u);
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==0u&&c[0]==0x81u&&c[3]==0u);
        CHECK(avp_mm_rotate_wall(&m,-1));
        CHECK(m.field==3u&&c[3]==0x81u&&c[0]==0u);
        CHECK(avp_mm_rotate_wall(&m,-1));
        CHECK(m.field==2u&&c[2]==0x81u&&c[3]==0u);
        c[3]=5u;before=m.field;
        CHECK(!avp_mm_rotate_wall(&m,1));
        CHECK(m.field==before&&c[2]==0x81u&&c[3]==5u);
        c[3]=0u;
        CHECK(avp_mm_rotate_wall(&m,-1)); /* right -> top */
        east[0]=4u;CHECK(!avp_mm_rotate_wall(&m,1));
        CHECK(c[1]==0x81u&&east[0]==4u);
        east[0]=0u;
        CHECK(avp_mm_rotate_wall(&m,1)); /* top -> right */
        CHECK(m.field==2u&&c[2]==0x81u);
        CHECK(avp_mm_rotate_wall(&m,-1));
        CHECK(m.field==1u&&c[1]==0x81u);
        c[1]=0u;c[0]=0x68u; /* passage/strip type group, mirrored */
        m.field=0u;CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(c[1]==0x68u&&c[0]==0u);
        c[1]=0u;c[0]=0u;
        /* Four orientations and door byte relocation.  0x20 = native door
         * group; opening state uses left/top in the owning physical cell. */
        c[0]=0x20u;c[6]=73u;m.field=0u;
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==1u&&c[0]==0u&&c[1]==0x20u&&c[6]==0u&&c[7]==73u);
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==2u&&c[1]==0u&&c[2]==0x20u&&c[7]==0u&&east[6]==73u);
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==3u&&c[2]==0u&&c[3]==0x20u&&east[6]==0u&&south[7]==73u);
        CHECK(avp_mm_rotate_wall(&m,1));
        CHECK(m.field==0u&&c[3]==0u&&c[0]==0x20u&&south[7]==0u&&c[6]==73u);
        east[6]=64u; /* don't destroy an existing door byte */
        CHECK(avp_mm_rotate_wall(&m,1)); /* left -> top is free */
        CHECK(!avp_mm_rotate_wall(&m,1)); /* top -> right is occupied */
        CHECK(c[1]==0x20u&&c[7]==73u&&east[6]==64u);
        /* A paired physical door cannot give up a shared opening byte. */
        c[1]=0u;c[7]=0u;c[0]=0x20u;c[6]=15u;
        c[-6]=0x20u; /* west neighbor opposite face */
        m.field=0u;CHECK(!avp_mm_rotate_wall(&m,1));
        CHECK(c[0]==0x20u&&c[6]==15u);
        c[-6]=0u;
        /* Perimeter door may not rotate into a nonexistent state owner. */
        m.x=m.w-1u;m.y=20u;m.field=1u;
        c=m.cells+((size_t)m.y*m.w+m.x)*8u;
        c[1]=0x20u;c[7]=12u;
        CHECK(!avp_mm_rotate_wall(&m,1));
        CHECK(c[1]==0x20u&&c[7]==12u);
        m.x=20u;m.y=20u;m.field=5u;
        CHECK(!avp_mm_rotate_wall(&m,1));
        m.field=0u;c=m.cells+((size_t)20u*m.w+20u)*8u;c[0]=0u;
        CHECK(!avp_mm_rotate_wall(&m,1));
        /* The 2D highlighted edge follows the selected wall field. */
        CHECK(avp_mm_overhead_layout(&m,640u,480u,&l));
        CHECK(avp_mm_overhead_edge(&m,640u,480u,l.x0+20*(int)l.cell_px,
                      l.y0+20*(int)l.cell_px+2)==0u);
        CHECK(avp_mm_overhead_edge(&m,640u,480u,l.x0+20*(int)l.cell_px+2,
                      l.y0+20*(int)l.cell_px)==1u);
        CHECK(avp_mm_overhead_edge(&m,640u,480u,l.x0+21*(int)l.cell_px-1,
                      l.y0+20*(int)l.cell_px+2)==2u);
        CHECK(avp_mm_overhead_edge(&m,640u,480u,l.x0+20*(int)l.cell_px+2,
                      l.y0+21*(int)l.cell_px-1)==3u);
        CHECK(avp_mm_overhead_edge(&m,640u,480u,-99,-99)==4u);
        CHECK(m.source[3].original[8u+5u]==4u); /* retail bytes unaffected */
    }
    /* RE32 R25: LEVDOOR.S offsets remain attached to their original door
     * state and panel byte locations. Verify catalog and fail-closed edits.
     * Synthetic 64*64 reference: no retail ROM required or embedded. */
    {
        AvpMapMaker retail={0};AvpMmScriptInfo info={0};
        u8 *reference=(u8*)calloc(8u+64u*64u*8u,1u);
        u8 *cell;unsigned x=18u,y=2u;
        CHECK(reference!=NULL);
        reference[0]=reference[2]=0u;reference[1]=reference[3]=64u;
        reference[8u+1169u]=0x20u; /* level 1 exit panel at cell (18,2) top */
        CHECK(avp_mm_script_catalog_size()==144u);
        CHECK(avp_mm_script_at(1u,1175u,&info));
        CHECK((info.flags&(AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK))==
              (AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK));
        CHECK(info.access_code==-2); /* source airlock permission */
        CHECK(avp_mm_script_at(1u,1169u,&info));
        CHECK(info.flags==AVP_MM_SCRIPT_EXIT);
        CHECK(info.destination_x==22u&&info.destination_y==27u&&info.destination_level==15u);
        CHECK(avp_mm_script_at(1u,3223u,&info));
        CHECK(info.flags==AVP_MM_SCRIPT_REQUIRED&&info.interlock_required_by==1175u);
        CHECK(avp_mm_script_at(4u,22087u,&info));
        CHECK(info.flags==AVP_MM_SCRIPT_ACCESS&&info.access_code==-3);
        CHECK(!avp_mm_script_at(16u,1175u,&info));
        CHECK(!avp_mm_script_at(1u,65535u,&info));
        avp_mm_init(&retail);
        CHECK(avp_mm_bind(&retail,1u,reference,8u+64u*64u*8u));
        CHECK(avp_mm_select(&retail,1u));
        retail.x=x;retail.y=y;retail.field=1u;
        cell=retail.cells+((size_t)y*64u+x)*8u;
        CHECK((avp_mm_script_flags(&retail,x,y,1u)&
               (AVP_MM_SCRIPT_EXIT|AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK))==
              (AVP_MM_SCRIPT_EXIT|AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK));
        CHECK(avp_mm_inspect_script(&retail));
        CHECK(strstr(retail.message,"Exit L15")!=NULL);
        CHECK(!avp_mm_rotate_wall(&retail,1)); /* must not strand exit records */
        CHECK(cell[1]==0x20u&&cell[2]==0u&&!retail.dirty);
        CHECK(!avp_mm_assign(&retail,0u)); /* no deletion through T palette */
        CHECK(!avp_mm_delete_cell(&retail));
        CHECK(cell[1]==0x20u&&!retail.dirty);
        retail.field=7u;
        CHECK(avp_mm_script_flags(&retail,x,y,7u)&AVP_MM_SCRIPT_ACCESS);
        CHECK(avp_mm_inspect_script(&retail));
        CHECK(strstr(retail.message,"Access airlock")!=NULL);
        CHECK(avp_mm_assign(&retail,5u)); /* state still editable */
        CHECK(cell[7]==5u&&retail.dirty);
        retail.x=18u;retail.y=6u;retail.field=1u;
        CHECK(avp_mm_script_flags(&retail,18u,6u,1u)&AVP_MM_SCRIPT_REQUIRED);
        CHECK(avp_mm_inspect_script(&retail));
        CHECK(strstr(retail.message,"Required by interlock @1175")!=NULL);
        retail.x=18u;retail.y=2u;retail.field=0u;
        cell[0]=1u;CHECK(!avp_mm_rotate_wall(&retail,1));
        CHECK(cell[0]==1u&&cell[1]==0x20u);
        CHECK(avp_mm_script_flags(&retail,0u,0u,0u)==0u);
        retail.x=20u;retail.y=20u;retail.field=1u;
        cell=retail.cells+((size_t)20u*64u+20u)*8u;
        CHECK(avp_mm_assign(&retail,1u)); /* unrelated panel remains editable */
        CHECK(avp_mm_rotate_wall(&retail,1));
        CHECK(cell[1]==0u&&cell[2]==1u);
        CHECK(avp_mm_blank(&retail)); /* user-made map has NO retail scripts */
        retail.x=x;retail.y=y;retail.field=1u;
        CHECK(avp_mm_script_flags(&retail,x,y,1u)==0u);
        CHECK(avp_mm_assign(&retail,0x20u));
        CHECK(avp_mm_rotate_wall(&retail,1));
        CHECK(!avp_mm_inspect_script(&retail));
        avp_mm_destroy(&retail);free(reference);
    }
    /* RE32 R26: explicit editor-only offset relocation, interlock linkage,
     * v2 draft round-trip and failure-atomic rejection. No ROM required. */
    {
        AvpMapMaker draft={0},different={0};AvpMmScriptInfo info={0};
        u8 *reference=calloc(8u+64u*64u*8u,1u);
        u8 *changed=calloc(8u+64u*64u*8u,1u);
        const char *v2path="test_map_maker_r26.avpm";
        u8 *a;FILE *f;unsigned before;
        CHECK(reference!=NULL && changed!=NULL);
        reference[1]=reference[3]=64u;
        reference[8u+1169u]=0x20u;
        reference[8u+3223u-6u]=0x20u; /* required door top panel */
        memcpy(changed,reference,8u+64u*64u*8u);
        changed[8u+100u]=7u; /* distinct source fingerprint */
        avp_mm_init(&draft);avp_mm_init(&different);
        CHECK(avp_mm_bind(&draft,1u,reference,8u+64u*64u*8u));
        CHECK(avp_mm_bind(&different,1u,changed,8u+64u*64u*8u));
        CHECK(avp_mm_select(&draft,1u));
        CHECK(avp_mm_select(&different,1u));
        CHECK(draft.script_count==15u);
        CHECK(avp_mm_script_validate(draft.script_rows,draft.script_count,1u));
        /* Opposite panel may hold an EXIT even if door-state has no rule. */
        draft.x=1u;draft.y=4u;draft.field=2u;
        CHECK(!avp_mm_script_flags(&draft,1u,4u,2u));
        CHECK(!avp_mm_assign(&draft,0x20u));
        CHECK(draft.cells[((size_t)4u*64u+1u)*8u+2u]==0u);
        /* Texture mode must reject the WHOLE multi-cell selection when
         * a retail scripted edge is reached, not paint the first cell. */
        draft.edit_mode_2d=3u;draft.texture_target_2d=2u;draft.field=2u;
        CHECK(avp_mm_begin_2d_drag_selection(&draft,0u,4u));
        CHECK(avp_mm_update_2d_drag_selection(&draft,1u,4u));
        avp_mm_texture_brush_select(&draft,0x20u);
        CHECK(!avp_mm_texture_paint_selection(&draft));
        CHECK(draft.cells[((size_t)4u*64u+0u)*8u+2u]==0u);
        CHECK(draft.cells[((size_t)4u*64u+1u)*8u+2u]==0u);
        draft.edit_mode_2d=0u;draft.selection_2d_valid=0;
        draft.x=18u;draft.y=2u;draft.field=1u;
        CHECK(!avp_mm_rotate_wall(&draft,1)); /* ordinary rotation still protected */
        CHECK(!draft.dirty && !draft.script_modified);
        a=draft.cells+((size_t)2u*64u+18u)*8u;
        avp_mm_history_begin(&draft);
        CHECK(avp_mm_relocate_scripted_wall(&draft,1));
        avp_mm_history_commit(&draft);
        CHECK(avp_mm_undo(&draft));
        CHECK(a[1]==0x20u && a[2]==0u && !draft.script_modified);
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,1175u,&info));
        CHECK(avp_mm_redo(&draft));
        CHECK(draft.script_modified && draft.dirty && draft.field==2u);
        CHECK(a[1]==0u && a[2]==0x20u);
        CHECK(avp_mm_script_validate(draft.script_rows,draft.script_count,1u));
        CHECK(!avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,1169u,&info));
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,1170u,&info));
        CHECK(info.flags==AVP_MM_SCRIPT_EXIT && info.destination_level==15u);
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,1182u,&info));
        CHECK((info.flags&(AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK))==
              (AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK));
        CHECK(info.access_code==-2);
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,3223u,&info));
        CHECK(info.flags==AVP_MM_SCRIPT_REQUIRED&&info.interlock_required_by==1182u);
        CHECK(avp_mm_inspect_script(&draft));
        CHECK(strstr(draft.message,"DRAFT SCRIPT")!=NULL &&
              strstr(draft.message,"Exit L15")!=NULL);
        before=draft.script_rows[0].offset;
        a[3]=1u;
        CHECK(!avp_mm_relocate_scripted_wall(&draft,1)); /* occupied destination */
        CHECK(draft.script_rows[0].offset==before);
        a[3]=0u;
        /* Rotate the updated source back: all metadata should return to the
         * exact original LEVDOOR.S offsets, not just source panel bytes. */
        CHECK(avp_mm_relocate_scripted_wall(&draft,-1));
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,1175u,&info));
        CHECK(info.flags&(AVP_MM_SCRIPT_ACCESS|AVP_MM_SCRIPT_INTERLOCK));
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,3223u,&info));
        CHECK(info.interlock_required_by==1175u);
        CHECK(avp_mm_relocate_scripted_wall(&draft,1));
        CHECK(avp_mm_save(&draft,v2path));
        /* Rebinding a different original while the overlay is active must
         * refuse to misattribute the working offsets to a new cartridge. */
        CHECK(avp_mm_bind(&draft,1u,changed,8u+64u*64u*8u));
        CHECK(!avp_mm_save(&draft,v2path));
        CHECK(!avp_mm_relocate_scripted_wall(&draft,-1));
        CHECK(avp_mm_bind(&draft,1u,reference,8u+64u*64u*8u));
        CHECK(avp_mm_save(&draft,v2path));
        f=fopen(v2path,"rb");CHECK(f!=NULL);
        {char magic[8];CHECK(fread(magic,1,8u,f)==8u);
         CHECK(memcmp(magic,"AVPMAP02",8u)==0);}
        CHECK(fclose(f)==0);
        before=different.cells[1169u];
        CHECK(!avp_mm_load(&different,v2path)); /* wrong original ROM */
        CHECK(different.cells[1169u]==before && !different.script_modified);
        CHECK(avp_mm_select(&draft,1u));
        CHECK(avp_mm_load(&draft,v2path));
        CHECK(draft.script_modified && draft.field==0u);
        CHECK(draft.cells[1169u]==0u && draft.cells[1170u]==0x20u);
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,1182u,&info));
        CHECK(info.flags&AVP_MM_SCRIPT_INTERLOCK);
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,3223u,&info));
        CHECK(info.interlock_required_by==1182u);
        CHECK(!avp_mm_rotate_wall(&draft,-1)); /* no panel on selected field */
        /* CRC failure must preserve all cells and metadata in memory. */
        f=fopen(v2path,"r+b");CHECK(f!=NULL);
        CHECK(fseek(f,-1,SEEK_END)==0);CHECK(fputc(0x55,f)!=EOF);
        CHECK(fclose(f)==0);
        before=draft.script_rows[0].offset;
        CHECK(!avp_mm_load(&draft,v2path));
        CHECK(draft.script_rows[0].offset==before && draft.cells[1170u]==0x20u);
        remove(v2path);
        /* Deleting the new script-bearing cell is also guarded. */
        draft.x=18u;draft.y=2u;draft.field=2u;
        CHECK(!avp_mm_delete_cell(&draft));
        CHECK(draft.cells[1170u]==0x20u);
        /* Editing a required-state door changes its own row offset only,
         * not its relationship to the relocated interlock. */
        draft.x=18u;draft.y=6u;draft.field=1u;
        CHECK(avp_mm_script_flags(&draft,18u,6u,1u)&AVP_MM_SCRIPT_REQUIRED);
        CHECK(avp_mm_relocate_scripted_wall(&draft,1));
        CHECK(avp_mm_script_lookup(draft.script_rows,draft.script_count,1u,3230u,&info));
        CHECK(info.flags==AVP_MM_SCRIPT_REQUIRED&&info.interlock_required_by==1182u);
        CHECK(avp_mm_script_validate(draft.script_rows,draft.script_count,1u));
        /* Reject fabricated links and merged anchors. */
        draft.script_rows[0].arg=99;
        CHECK(!avp_mm_script_validate(draft.script_rows,draft.script_count,1u));
        avp_mm_destroy(&draft);avp_mm_destroy(&different);
        free(reference);free(changed);
    }
    /* RE32 R27: a single wall brush, no second geometry representation.
     * Drawing/erasing never overwrites neighbors, door state or script
     * anchors and keeps flats and the source retail map untouched. */
    {
        AvpMapMaker w={0};u8 *c,*east;
        AvpMmOverheadLayout layout0,layout1;
        avp_mm_init(&w);
        CHECK(w.wall_brush==1u&&!w.wall_tool);
        CHECK(avp_mm_blank(&w));
        avp_mm_toggle_wall_tool(&w);CHECK(w.wall_tool);
        CHECK(avp_mm_wall_place(&w,20u,20u,1u));
        c=w.cells+((size_t)20u*64u+20u)*8u;
        east=c+8u;
        CHECK(c[1]==1u && w.x==20u && w.y==20u && w.field==1u);
        CHECK(c[5]==0u && w.dirty); /* no implicit floor creation */
        CHECK(avp_mm_wall_place(&w,20u,20u,1u)); /* repeat harmless */
        avp_mm_wall_brush_select(&w,0x81u);
        CHECK(!avp_mm_wall_place(&w,20u,20u,1u));
        CHECK(c[1]==1u); /* no replacement by accidental click */
        CHECK(avp_mm_wall_turn(&w,1));
        CHECK(c[1]==1u && c[2]==0u && w.field==2u);
        CHECK(c[1]==1u && c[2]==0u); /* rotating MUST NOT move placed panel */
        CHECK(avp_mm_wall_turn(&w,-1)); /* occupied: still turn brush only */
        CHECK(w.field==1u && c[1]==1u && !c[2]);
        CHECK(avp_mm_wall_turn(&w,-1));
        CHECK(w.field==0u && c[1]==1u && c[0]==0u);
        CHECK(avp_mm_wall_turn(&w,-1));
        CHECK(w.field==3u && c[1]==1u && !c[3]);
        CHECK(avp_mm_wall_place(&w,20u,20u,w.field));
        CHECK(c[3]==0x81u);
        CHECK(avp_mm_wall_erase(&w,20u,20u,3u));CHECK(!c[3]);
        avp_mm_wall_brush_select(&w,0u);
        CHECK(w.wall_brush==0x81u); /* 0 means erase, not a brush */
        avp_mm_wall_brush_select(&w,0x20u); /* Jaguar door group */
        CHECK(avp_mm_wall_place(&w,20u,20u,2u));
        CHECK(c[2]==0x20u);
        east[6]=11u; /* state must follow deletion, without a ghost door */
        CHECK(avp_mm_wall_erase(&w,20u,20u,2u));
        CHECK(c[2]==0u && east[6]==0u);
        CHECK(!avp_mm_wall_place(&w,63u,20u,2u)); /* no door-state owner */
        CHECK(!avp_mm_wall_place(&w,20u,64u,0u));
        CHECK(!avp_mm_wall_erase(&w,20u,20u,4u));
        CHECK(avp_mm_overhead_layout(&w,640u,456u,&layout0));
        avp_mm_zoom(&w,1);
        w.wall_drag_lock=1;w.wall_drag_x=20u;w.wall_drag_y=20u;
        CHECK(avp_mm_overhead_layout(&w,640u,456u,&layout0));
        w.x=35u;w.y=30u;
        CHECK(avp_mm_overhead_layout(&w,640u,456u,&layout1));
        CHECK(layout1.x0==layout0.x0 && layout1.y0==layout0.y0);
        w.wall_drag_lock=0;
        CHECK(avp_mm_overhead_layout(&w,640u,456u,&layout1));
        CHECK(layout1.x0!=layout0.x0 && layout1.y0!=layout0.y0);
        avp_mm_toggle_wall_tool(&w);CHECK(!w.wall_tool && !w.wall_drag_lock);
        CHECK(avp_mm_save(&w,"test_map_maker_brush.avpm"));
        CHECK(avp_mm_select(&w,w.level));
        CHECK(avp_mm_load(&w,"test_map_maker_brush.avpm"));
        CHECK(!w.wall_tool && w.wall_brush==0x20u); /* UI-only, not serialized */
        remove("test_map_maker_brush.avpm");avp_mm_destroy(&w);
    }
    /* RE32 R28: wide edge-snapping, visible hover, gap-free drag interpolation.
     * Every sample uses its initial direction: fast cursor jumps cannot
     * leave gaps or accidentally turn into another wall face. */
    {
        AvpMapMaker w={0};AvpMmOverheadLayout l;
        uint32_t *preview;
        unsigned px,py;
        avp_mm_init(&w);CHECK(avp_mm_blank(&w));
        avp_mm_toggle_wall_tool(&w);
        w.x=10u;w.y=10u;w.field=1u;
        CHECK(avp_mm_wall_stroke(&w,10u,10u,1u,0));
        CHECK(w.wall_drag_lock && w.wall_drag_side==1u);
        CHECK(avp_mm_wall_stroke(&w,15u,10u,0u,0)); /* 5 cells skipped */
        for(unsigned x=10u;x<=15u;x++){
            const u8 *cell=w.cells+((size_t)10u*64u+x)*8u;
            CHECK(cell[1]==1u && cell[0]==0u && cell[2]==0u);
        }
        CHECK(w.field==1u); /* pointer cannot alter orientation mid-drag */
        avp_mm_wall_drag_end(&w);CHECK(!w.wall_drag_lock && !w.wall_last_valid);
        CHECK(avp_mm_wall_stroke(&w,10u,10u,1u,1));
        CHECK(avp_mm_wall_stroke(&w,15u,10u,3u,1));
        for(unsigned x=10u;x<=15u;x++)
            CHECK(w.cells[((size_t)10u*64u+x)*8u+1u]==0u);
        avp_mm_wall_drag_end(&w);
        avp_mm_wall_brush_select(&w,0x20u); /* native door group */
        CHECK(avp_mm_wall_stroke(&w,60u,10u,2u,0));
        CHECK(!avp_mm_wall_stroke(&w,63u,10u,2u,0));
        CHECK(w.cells[((size_t)10u*64u+60u)*8u+2u]==0x20u);
        CHECK(w.cells[((size_t)10u*64u+63u)*8u+2u]==0u); /* missing owner */
        avp_mm_wall_drag_end(&w);
        w.x=20u;w.y=20u;w.field=1u;
        CHECK(avp_mm_overhead_layout(&w,640u,480u,&l));
        /* First quarter of the tile snaps; center leaves direction alone. */
        CHECK(l.cell_px>=5u);
        CHECK(avp_mm_overhead_edge(&w,640u,480u,
           l.x0+20*(int)l.cell_px+1,l.y0+20*(int)l.cell_px+2)==0u);
        CHECK(avp_mm_overhead_edge(&w,640u,480u,
           l.x0+20*(int)l.cell_px+(int)l.cell_px/2,
           l.y0+20*(int)l.cell_px+(int)l.cell_px/2)==4u);
        preview=calloc((size_t)640u*480u,sizeof *preview);CHECK(preview!=NULL);
        w.wall_hover_valid=1;w.wall_hover_x=25u;w.wall_hover_y=25u;
        w.wall_hover_side=3u;
        avp_mm_render_overhead(&w,preview,640u,480u,NULL,NULL);
        px=(unsigned)(l.x0+25*(int)l.cell_px);
        py=(unsigned)(l.y0+26*(int)l.cell_px-1);
        CHECK(preview[(size_t)py*640u+px]==0x3cff7bu);
        w.wall_hover_valid=0;
        avp_mm_render_overhead(&w,preview,640u,480u,NULL,NULL);
        CHECK(preview[(size_t)py*640u+px]!=0x3cff7bu);
        CHECK(avp_mm_wall_placeable(&w,25u,25u,3u));
        w.cells[((size_t)25u*64u+25u)*8u+3u]=2u;
        CHECK(!avp_mm_wall_placeable(&w,25u,25u,3u));
        w.wall_hover_valid=1;
        avp_mm_render_overhead(&w,preview,640u,480u,NULL,NULL);
        CHECK(preview[(size_t)py*640u+px]==0xff605bu);
        w.wall_hover_valid=0;
        w.cells[((size_t)25u*64u+25u)*8u+3u]=0u;
        avp_mm_wall_brush_select(&w,0x20u);
        CHECK(!avp_mm_wall_placeable(&w,63u,25u,2u));
        free(preview);
        avp_mm_destroy(&w);
    }
    /* R27: clicked face persists independently from hover; left-click
     * replaces, either Shift+click toggles, and wheel edits are atomic. */
    {
        AvpMapMaker q={0};
        avp_mm_init(&q);CHECK(q.highlight_3d);
        CHECK(avp_mm_blank(&q));
        q.mode_3d=1;
        q.target_valid=1;q.target_x=8u;q.target_y=8u;q.target_field=2u;
        CHECK(avp_mm_select_target(&q,0));CHECK(q.selected_count==1u);
        CHECK(avp_mm_assign_target(&q,17u));
        /* Wheel through zero on an explicitly selected 3D wall face.
         * Neither the single-face nor multi-face path may enable ghost. */
        CHECK(avp_mm_assign_target(&q,1u));CHECK(!q.ghost_walls);
        CHECK(avp_mm_adjust_target(&q,-1));CHECK(!q.ghost_walls);
        CHECK(q.cells[((size_t)8u*64u+8u)*8u+2u]==0u);
        CHECK(q.selected_count==1u);
        CHECK(avp_mm_adjust_target(&q,1));CHECK(!q.ghost_walls);
        CHECK(q.cells[((size_t)8u*64u+8u)*8u+2u]==1u);
        q.target_x=9u;q.target_y=8u;q.target_field=0u;
        CHECK(avp_mm_select_target(&q,1));CHECK(q.selected_count==2u);
        CHECK(avp_mm_assign_target(&q,28u));
        CHECK(q.cells[((size_t)8u*64u+8u)*8u+2u]==28u);
        CHECK(q.cells[((size_t)8u*64u+9u)*8u]==28u);
        CHECK(avp_mm_assign_target(&q,0u));CHECK(!q.ghost_walls);
        CHECK(q.selected_count==2u);
        CHECK(avp_mm_assign_target(&q,28u));CHECK(!q.ghost_walls);
        CHECK(avp_mm_adjust_target(&q,1));
        CHECK(q.cells[((size_t)8u*64u+8u)*8u+2u]==29u);
        CHECK(q.cells[((size_t)8u*64u+9u)*8u]==29u);
        CHECK(avp_mm_mirror_target(&q));
        CHECK(q.cells[((size_t)8u*64u+8u)*8u+2u]==(29u|0x80u));
        CHECK(q.cells[((size_t)8u*64u+9u)*8u]==(29u|0x80u));
        CHECK(avp_mm_select_target(&q,1));CHECK(q.selected_count==1u);
        CHECK(avp_mm_adjust_target(&q,1));
        CHECK(q.cells[((size_t)8u*64u+8u)*8u+2u]==(30u|0x80u));
        CHECK(q.cells[((size_t)8u*64u+9u)*8u]==(29u|0x80u));
        CHECK(avp_mm_select_target(&q,0));CHECK(q.selected_count==1u);
        avp_mm_toggle_ghost_walls(&q);CHECK(q.ghost_walls);
        CHECK(avp_mm_assign_target(&q,0u));CHECK(q.ghost_walls);
        CHECK(avp_mm_adjust_target(&q,1));CHECK(q.ghost_walls);
        CHECK(avp_mm_blank(&q));CHECK(!q.selected_count);
        CHECK(avp_mm_select_target(&q,0)==0); /* empty hover never added */
        avp_mm_destroy(&q);
    }
    /* RE32 R29: one undo for a full wall stroke, object/3D/script state
     * restoration, no-op history, redo invalidation and map-load isolation. */
    {
        AvpMapMaker w={0};
        size_t ca=((size_t)12u*64u+10u)*8u+1u;
        size_t cb=((size_t)12u*64u+16u)*8u+1u;
        avp_mm_init(&w);CHECK(avp_mm_blank(&w));
        avp_mm_toggle_wall_tool(&w);
        CHECK(!avp_mm_undo(&w));
        CHECK(!avp_mm_redo(&w));
        avp_mm_history_begin(&w);
        CHECK(avp_mm_wall_stroke(&w,10u,12u,1u,0));
        CHECK(avp_mm_wall_stroke(&w,16u,12u,1u,0));
        avp_mm_wall_drag_end(&w);
        avp_mm_history_commit(&w);
        CHECK(w.cells[ca]==1u && w.cells[cb]==1u);
        CHECK(avp_mm_undo(&w));
        CHECK(w.cells[ca]==0u && w.cells[cb]==0u);
        CHECK(!avp_mm_undo(&w)); /* stroke is exactly one undo */
        CHECK(avp_mm_redo(&w));
        CHECK(w.cells[ca]==1u && w.cells[cb]==1u);
        /* A no-op must never fill an undo slot or discard redo. */
        CHECK(avp_mm_undo(&w));
        avp_mm_history_begin(&w);avp_mm_history_commit(&w);
        CHECK(avp_mm_redo(&w));
        CHECK(w.cells[ca]==1u && w.cells[cb]==1u);
        CHECK(avp_mm_undo(&w));
        /* A new actual edit invalidates redo. */
        avp_mm_history_begin(&w);
        CHECK(avp_mm_wall_place(&w,10u,12u,2u));
        avp_mm_history_commit(&w);
        CHECK(!avp_mm_redo(&w));
        CHECK(avp_mm_undo(&w));
        CHECK(w.cells[ca+1u]==0u);
        /* Capture map-native script and object arrays, not UI hover. */
        avp_mm_history_begin(&w);
        w.script_count=1u;w.script_modified=1u;
        w.script_rows[0].level=1u;w.script_rows[0].offset=22u;
        w.object_count=1u;w.objects[0].x8=104u;
        avp_mm_history_commit(&w);
        CHECK(avp_mm_undo(&w));
        CHECK(w.script_count==0u && w.object_count==0u);
        CHECK(avp_mm_redo(&w));
        CHECK(w.script_count==1u && w.script_rows[0].offset==22u);
        CHECK(w.object_count==1u && w.objects[0].x8==104u);
        /* Restore only authoring data. Changing UI hover is not undoable. */
        w.wall_hover_valid=1;w.camera_heading=0.77;
        CHECK(avp_mm_undo(&w));
        CHECK(w.wall_hover_valid==0 && w.camera_heading==0.77);
        CHECK(avp_mm_blank(&w));
        CHECK(!avp_mm_undo(&w));CHECK(!avp_mm_redo(&w));
        avp_mm_history_begin(&w);
        w.mode_3d=1;w.target_x=4u;w.target_y=4u;w.target_field=1u;w.target_valid=1;
        CHECK(avp_mm_assign_target(&w,24u));
        avp_mm_history_commit(&w);
        CHECK(avp_mm_undo(&w));
        CHECK(w.cells[((size_t)4u*64u+4u)*8u+1u]==0u);
        CHECK(avp_mm_redo(&w));
        CHECK(w.cells[((size_t)4u*64u+4u)*8u+1u]==24u);
        CHECK(avp_mm_select(&w,2u));
        CHECK(!avp_mm_undo(&w));
        for(unsigned i=0;i<30u;i++){
            avp_mm_history_begin(&w);w.cells[0]=(u8)(i+1u);
            avp_mm_history_commit(&w);
        }
        for(unsigned i=0;i<24u;i++)CHECK(avp_mm_undo(&w));
        CHECK(w.cells[0]==6u); /* older six edits fell off the ring */
        CHECK(!avp_mm_undo(&w));
        for(unsigned i=0;i<24u;i++)CHECK(avp_mm_redo(&w));
        CHECK(w.cells[0]==30u);
        CHECK(!avp_mm_redo(&w));
        avp_mm_destroy(&w);
    }
    /* R37: Shift+left-drag in 3D adds each aimed face once, without
     * toggling an existing selection, changing a map byte or consuming Undo.
     * Staged object markers can be selected in the same drag and are visible
     * only if no blocking Jaguar panel occludes the marker. */
    {
        AvpMapMaker q={0};
        u8 before[AVP_MM_MAX_BYTES];
        uint32_t *img=calloc(1024u*576u,sizeof *img);
        CHECK(img!=NULL);
        avp_mm_init(&q);CHECK(avp_mm_blank(&q));
        q.mode_3d=1;
        memcpy(before,q.cells,sizeof before);
        q.target_valid=1;q.target_x=10u;q.target_y=10u;q.target_field=2u;
        CHECK(avp_mm_select_target_drag(&q));CHECK(q.selected_count==1u);
        CHECK(avp_mm_select_target_drag(&q));CHECK(q.selected_count==1u);
        q.target_x=11u;q.target_field=0u;
        CHECK(avp_mm_select_target_drag(&q));CHECK(q.selected_count==2u);
        CHECK(avp_mm_select_target_drag(&q));CHECK(q.selected_count==2u);
        q.target_valid=0;CHECK(!avp_mm_select_target_drag(&q));
        CHECK(q.selected_count==2u);
        q.target_valid=1;q.target_field=8u;
        CHECK(!avp_mm_select_target_drag(&q));CHECK(q.selected_count==2u);
        /* Shift+click's deliberate toggle still works independently. */
        q.target_field=0u;CHECK(avp_mm_select_target(&q,1));
        CHECK(q.selected_count==1u);
        q.camera_x=10.5;q.camera_y=10.5;q.camera_heading=0.0;
        q.camera_pitch=0.0;q.view_ray_scale=1.0;
        q.object_count=2u;
        q.objects[0]=(AvpMmObjectRecord){(u16)(12u*256u+128u),(u16)(10u*256u+128u),21u,3u};
        q.objects[1]=(AvpMmObjectRecord){(u16)(13u*256u+128u),(u16)(11u*256u+128u),21u,3u};
        CHECK(avp_mm_3d_object_at(&q,1024u,576u,512,288)==0);
        CHECK(avp_mm_3d_object_at(&q,1024u,576u,683,288)==1);
        CHECK(avp_mm_3d_object_at(&q,1024u,576u,-1,288)==-1);
        CHECK(avp_mm_draw_3d_objects(&q,img,1024u,576u)==2);
        CHECK(img[288u*1024u+512u]!=0u);
        CHECK(avp_mm_select_object(&q,0u,1,0));
        CHECK(avp_mm_select_object(&q,0u,1,0)); /* drag: idempotent */
        CHECK(avp_mm_selected_object_count(&q)==1u);
        CHECK(avp_mm_select_object(&q,1u,1,0));
        CHECK(avp_mm_selected_object_count(&q)==2u);
        CHECK(avp_mm_select_object(&q,1u,1,1)); /* Shift+click toggles */
        CHECK(avp_mm_selected_object_count(&q)==1u);
        CHECK(q.selected_count==1u); /* face selection kept */
        CHECK(memcmp(before,q.cells,sizeof before)==0);
        CHECK(!avp_mm_undo(&q)); /* selection-only: no authoring history */
        avp_mm_history_begin(&q);
        CHECK(avp_mm_delete_selected_objects(&q)==1u);
        avp_mm_history_commit(&q);
        CHECK(q.object_count==1u && avp_mm_selected_object_count(&q)==0u);
        CHECK(avp_mm_undo(&q));CHECK(q.object_count==2u);
        CHECK(avp_mm_redo(&q));CHECK(q.object_count==1u);
        /* Panel on camera-facing edge occludes a staged object marker. */
        q.objects[0]=q.objects[1];q.object_count=1u;
        q.objects[0]=(AvpMmObjectRecord){(u16)(12u*256u+128u),(u16)(10u*256u+128u),21u,3u};
        CHECK(avp_mm_3d_object_at(&q,1024u,576u,512,288)==0);
        q.cells[((size_t)10u*64u+10u)*8u+2u]=1u;
        CHECK(avp_mm_3d_object_at(&q,1024u,576u,512,288)==-1);
        avp_mm_clear_selection(&q);
        CHECK(q.selected_count==0u&&avp_mm_selected_object_count(&q)==0u);
        free(img);avp_mm_destroy(&q);
    }
    avp_mm_destroy(&m);avp_mm_destroy(&n);remove(path);
    remove("test_map_maker_custom.avpm");
    CHECK(strcmp(avp_mm_name(14u),"Alien Ship")==0);
    CHECK(strcmp(avp_mm_name(15u),"Predator Ship")==0);
    printf("Map Maker: %d PASS\n",checks);
    return 0;
}
