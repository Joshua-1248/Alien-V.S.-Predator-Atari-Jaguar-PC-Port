"""Retail movement branch repair in the disposable PC build view, R437."""

def apply_collision_fidelity(path, data):
    if path == 'src/game/player.c':
        text = data.decode('utf-8')
        old = '    if(!WALL_SAFE(nx,ny)){'
        new = '''    /* PLAYER.S $10068/$1006e: either wall OR AMP hit enters the
     * same Y-only then X-only retry sequence. Never cancel both axes
     * merely because the initial endpoint hit an actor. */
    if(!WALL_SAFE(nx,ny) || !AMP_SAFE(nx,ny)){'''
        tail = '    } else if(!AMP_SAFE(nx,ny)) { nx=(s32)x_pos;ny=(s32)y_pos;x_vel=y_vel=0; }'
        if text.count(old) != 1 or text.count(tail) != 1:
            raise ValueError('Unexpected MovePlayer collision branch')
        return text.replace(old,new).replace(tail,'    }').encode('utf-8')
    if path == 'src/game/amp.c':
        text = data.decode('utf-8')
        old = '    if(amp_contact_in_range(a,90)){queen_choose_fight(a);return;}'
        new = '''    /* AMP.S $17e66: retain the Jaguar square for the Queen's approach
     * decision. An optional circular damage range cannot also shrink this
     * stopping range: it cuts into the unchanged square solid footprint.
     * The optional circular damaging-event test remains independent. */
    if(amp_in_range(a,90)){queen_choose_fight(a);return;}'''
        if text.count(old) != 1:
            raise ValueError('Unexpected Queen approach branch')
        return text.replace(old,new).encode('utf-8')
    return data
