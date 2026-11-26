/* PC-only Marine vent population: 4B/4C (10/11) get three each,
 * 5B/5C (12/13) get five each; all other levels get none. Run after
 * enemy multiplication so this bonus is independent of that multiplier. */
static int host_vent_facehuggers;
void avp_amp_set_vent_facehuggers(int enabled){host_vent_facehuggers=!!enabled;}
static void modifier_vent_facehuggers(void)
{
    enum { CELLS=64*64, FACEHUG_TEMPLATE=4, MAX_COUNT=5 };
    u8 occupied[CELLS]={0};
    unsigned placed[MAX_COUNT],added=0;
    if(!host_vent_facehuggers||player_type!=PT_HUMAN||cur_level<10||cur_level>13)return;
    const unsigned count=cur_level<=11?3u:5u;
    const u8 *maze=avp_collision_maze_data();
    unsigned w=avp_collision_maze_width(),h=avp_collision_maze_height();
    unsigned px=x_pos>>16,py=y_pos>>16;
    if(!maze||!w||!h||w>64u||h>64u||bound_template_count<=FACEHUG_TEMPLATE)return;
    for(unsigned i=0;i<host_amp_capacity;i++){
        const AvpAmp *a=&amp_data[i];
        unsigned x=(u32)a->xpos>>16,y=(u32)a->ypos>>16;
        if((a->mode||a->host_static)&&x<w&&y<h)occupied[y*64u+x]=1;
    }
    /* Choose dispersed clear cells, not the entry or a sealed tile.
     * The native exclusion grid protects authored doorway/placement cells.
     * No gameplay RNG calls; enabling this does not perturb other spawns. */
    while(added<count&&free_count>64u){
        unsigned best=CELLS,best_dist=0,best_tie=0;
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
            unsigned cell=y*64u+x,open=0;
            const u8 *c=maze+(y*w+x)*8u;
            unsigned d=(x>px?x-px:px-x)+(y>py?y-py:py-y);
            if(!c[5]||occupied[cell]||collmap[cell]||d<2u)continue;
            for(unsigned dir=0;dir<4;dir++){
                static const int dx[4]={-1,0,1,0},dy[4]={0,-1,0,1};
                int nx=(int)x+dx[dir],ny=(int)y+dy[dir];
                if(nx<0||ny<0||(unsigned)nx>=w||(unsigned)ny>=h)continue;
                const u8 *n=maze+((unsigned)ny*w+(unsigned)nx)*8u;
                unsigned wall=c[dir]&127u,other=n[(dir+2u)%4u]&127u;
                if(n[5]&&!(wall>0u&&wall<32u)&&!(other>0u&&other<32u)){open=1;break;}
            }
            if(!open)continue;
            for(unsigned j=0;j<added;j++){
                unsigned sx=placed[j]%64u,sy=placed[j]/64u;
                unsigned sep=(x>sx?x-sx:sx-x)+(y>sy?y-sy:sy-y);
                if(sep<d)d=sep;
            }
            unsigned tie=cell*2654435761u+(unsigned)cur_level*668265263u;
            if(best==CELLS||d>best_dist||(d==best_dist&&tie>best_tie)){best=cell;best_dist=d;best_tie=tie;}
        }
        if(best==CELLS)break;
        (void)make_template_at(FACEHUG_TEMPLATE,(s32)(((best%64u)<<16)|0x8000u),
            (s32)(((best/64u)<<16)|0x8000u),1);
        occupied[best]=1;placed[added++]=best;
    }
    fprintf(stderr,"Marine Vent Facehuggers: L%d added=%u requested=%u\n",cur_level,added,count);
}
