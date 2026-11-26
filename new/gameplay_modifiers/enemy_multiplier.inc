/* PC-only initial population modifier. Runs after the original initial population, and never on the visited-level restore path. */
void avp_amp_set_enemy_number_multiplier(unsigned n)
{host_enemy_number_multiplier=n>=1u&&n<=4u?n:1u;}

static int modifier_enemy(const AvpAmp *a)
{
    return a->mode && a->energy>0 && (a->flags&(1u<<AMP_KILLABLE)) &&
        ((a->astype>=0 && a->astype<=5)||(a->astype>=29 && a->astype<=37)||a->astype==51);
}
/* Use the native random-placement exclusion grid, with a separate occupancy
 * bitmap for nonsolid pickups and other template positions. BFS from each
 * original enemy prevents placement across sealed walls or on void cells. */
static void modifier_initial_population(void)
{
    enum { CELLS=64*64, RESERVE=64 };
    unsigned originals[AVP_NUM_AMPS],count=0;
    u8 occupied[CELLS]={0};
    const u8 *maze;
    unsigned w,h,requested,added=0;
    if(host_enemy_number_multiplier==1u)return;
    maze=avp_collision_maze_data();w=avp_collision_maze_width();h=avp_collision_maze_height();
    if(!maze||!w||!h||w>64u||h>64u)return;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        const AvpAmp *a=&amp_data[i];
        unsigned x=(u32)a->xpos>>16,y=(u32)a->ypos>>16;
        if(!a->mode&&!a->host_static)continue;
        if(x<64u&&y<64u)occupied[y*64u+x]=1;
        if(modifier_enemy(a))originals[count++]=i;
    }
    /* Do not occupy the starting square or scripted encounter spawn cells. */
    {unsigned px=x_pos>>16,py=y_pos>>16;if(px<64u&&py<64u)occupied[py*64u+px]=1;}
    if(cur_level==14)occupied[14u*64u+24u]=1;
    if(cur_level==15){occupied[15u*64u+38u]=1;occupied[17u*64u+42u]=1;
        occupied[15u*64u+40u]=1;occupied[14u*64u+43u]=1;occupied[21u*64u+14u]=1;}
    requested=count*(host_enemy_number_multiplier-1u);
    /* Round-robin across originals so a capacity limit does not preferentially
     * multiply the first species/list. No changes to the gameplay PRNG. */
    for(unsigned pass=1;pass<host_enemy_number_multiplier;pass++){
        for(unsigned j=0;j<count && free_count>RESERVE;j++){
            const AvpAmp *a=&amp_data[originals[j]];
            unsigned sx=(u32)a->xpos>>16,sy=(u32)a->ypos>>16;
            u16 queue[CELLS];u8 seen[CELLS]={0};
            unsigned head=0,tail=0,best=CELLS,best_rank=0xffffffffu;
            if(sx<w&&sy<h&&maze[(sy*w+sx)*8u+5u]){
                queue[tail++]=(u16)(sy*64u+sx);seen[sy*64u+sx]=1;
            }
            while(head<tail){
                unsigned cell=queue[head++],x=cell%64u,y=cell/64u;
                const u8 *c=maze+(y*w+x)*8u;
                if(!occupied[cell]&&!collmap[cell]){
                    unsigned px=x_pos>>16,py=y_pos>>16;
                    unsigned pd=(x>px?x-px:px-x)+(y>py?y-py:py-y);
                    unsigned d=(x>sx?x-sx:sx-x)+(y>sy?y-sy:sy-y);
                    /* Prefer a nearby distinct position 3–8 cells away, but
                     * never on/adjacent to the player's entry point. */
                    if(pd>=2u){
                        unsigned penalty=d<3u?3u-d:d>8u?d-8u:0u;
                        unsigned tie=(cell*2654435761u+j*2246822519u+pass*3266489917u+(unsigned)cur_level*668265263u);
                        unsigned rank=penalty*65536u+(tie>>16);
                        if(rank<best_rank){best_rank=rank;best=cell;}
                    }
                }
                for(unsigned dir=0;dir<4u;dir++){
                    static const int dx[4]={-1,0,1,0},dy[4]={0,-1,0,1};
                    int nx=(int)x+dx[dir],ny=(int)y+dy[dir];
                    unsigned next,wall,other;
                    const u8 *n;
                    if(nx<0||ny<0||(unsigned)nx>=w||(unsigned)ny>=h)continue;
                    next=(unsigned)ny*64u+(unsigned)nx;
                    if(seen[next])continue;
                    n=maze+((unsigned)ny*w+(unsigned)nx)*8u;
                    wall=c[dir]&127u;other=n[(dir+2u)%4u]&127u;
                    if(!n[5]||(wall>0u&&wall<32u)||(other>0u&&other<32u))continue;
                    seen[next]=1;queue[tail++]=(u16)next;
                }
            }
            /* A native enemy can occupy an isolated cell or a saturated
             * room. Fall back to another walkable connected region instead
             * of silently losing that clone. Require at least one open
             * neighbor; never use a sealed single cell or occupied square. */
            if(best==CELLS){
                for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
                    unsigned cell=y*64u+x,open=0;
                    const u8 *c=maze+(y*w+x)*8u;
                    unsigned px=x_pos>>16,py=y_pos>>16;
                    unsigned pd=(x>px?x-px:px-x)+(y>py?y-py:py-y);
                    if(!c[5]||occupied[cell]||collmap[cell]||pd<2u)continue;
                    for(unsigned dir=0;dir<4u;dir++){
                        static const int dx[4]={-1,0,1,0},dy[4]={0,-1,0,1};
                        int nx=(int)x+dx[dir],ny=(int)y+dy[dir];
                        if(nx<0||ny<0||(unsigned)nx>=w||(unsigned)ny>=h)continue;
                        const u8 *n=maze+((unsigned)ny*w+(unsigned)nx)*8u;
                        unsigned wall=c[dir]&127u,other=n[(dir+2u)%4u]&127u;
                        if(n[5]&&!(wall>0u&&wall<32u)&&!(other>0u&&other<32u)){open=1;break;}
                    }
                    if(open){
                        unsigned rank=cell*2654435761u+j*2246822519u+pass*3266489917u+(unsigned)cur_level*668265263u;
                        if(rank<best_rank){best_rank=rank;best=cell;}
                    }
                }
            }
            if(best==CELLS)continue;
            (void)make_template_at((u16)a->astype,(s32)(((best%64u)<<16)|0x8000u),
                   (s32)(((best/64u)<<16)|0x8000u),1);
            occupied[best]=1;++added;
        }
    }
    /* Diagnostic makes capacity/geometry limits visible instead of silently
     * overwriting the last allocated AMP (the retail underflow behavior). */
    fprintf(stderr,"Enemy Number Multiplier: L%d %ux originals=%u added=%u requested=%u free=%u\n",
            cur_level,host_enemy_number_multiplier,count,added,requested,free_count);
}
