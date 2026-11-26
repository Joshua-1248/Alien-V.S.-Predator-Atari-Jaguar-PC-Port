#ifdef AVP_PC_HOST
/* Game-thread only. Stop retired handles before reducing the search limit;
 * clear newly enabled entries so a zero-initialized type is never occupied. */
void avp_music_set_sound_channels(unsigned count)
{
    unsigned old=pc_sound_channels;
    count=avp_sound_channels_valid(count);
    if(count<old){
        for(unsigned i=count;i<old;i++){
            if(voice[i].handle)request_stop(i,1);
            memset(&voice[i],0,sizeof voice[i]);
            voice[i].type=AVP_SOUND_TYPE_STOPPED;
        }
    }else{
        for(unsigned i=old;i<count;i++){
            memset(&voice[i],0,sizeof voice[i]);
            voice[i].type=AVP_SOUND_TYPE_STOPPED;
        }
    }
    pc_sound_channels=count;
    if(protected_voices>count)protected_voices=count;
}
#endif
