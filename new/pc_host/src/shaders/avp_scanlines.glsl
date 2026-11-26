/* Same output-pixel integer kernel as pc_scanlines.h. Both source-world
 * and opaque foreground output pass through it exactly once. */
uint avp_scan_axis(uint pos,bool soft)
{
    uint t=avp_params.scan_thickness, period=t+avp_params.scan_spacing;
    uint p=(pos%period+avp_params.scan_phase)%period;
    uint a=(avp_params.scan_strength*256u+50u)/100u;
    if(p>=t)return 256u;
    if(soft){
        uint edge=min(p,t-1u-p), peak=2u*((t-1u)/2u)+1u;
        a=(a*(2u*edge+1u)+peak/2u)/peak;
    }
    return 256u-a;
}
uint avp_scan_rgb(uint rgb,uint x,uint y)
{
    if(avp_params.scan_enabled!=1u||avp_params.scan_style>11u||
       avp_params.scan_thickness<1u||avp_params.scan_thickness>16u||
       avp_params.scan_spacing<1u||avp_params.scan_spacing>16u||
       avp_params.scan_strength<1u||avp_params.scan_strength>100u||
       avp_params.scan_phase>31u)return rgb;
    uint w;
    uint style=avp_params.scan_style;
    if(style==4u||style==11u){
        bool soft=style==11u;
        w=avp_scan_axis(x,soft)*avp_scan_axis(y,soft)/256u;
    }else if(style>=5u){
        uint period=avp_params.scan_thickness+avp_params.scan_spacing;
        uint down=(x%period+period-y%period)%period;
        uint up=(x%period+y%period)%period;
        bool soft=style==6u||style==8u||style==10u;
        if(style>=9u)w=avp_scan_axis(down,soft)*avp_scan_axis(up,soft)/256u;
        else w=avp_scan_axis(style<=6u?down:up,soft);
    }else if(style>=2u)w=avp_scan_axis(x,style==3u);
    else w=avp_scan_axis(y,avp_params.scan_style==1u);
    return (((rgb&0x00ff00ffu)*w>>8u)&0x00ff00ffu)|
           (((rgb&0x0000ff00u)*w>>8u)&0x0000ff00u);
}
