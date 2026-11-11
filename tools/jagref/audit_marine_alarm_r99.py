#!/usr/bin/env python3
"""Read-only retail ROM audit of the Marine self-destruct alarm module.
Original cartridge RDB carries FM tuning, but Jaguar JERRY wavetable is a
separate on-chip hardware ROM, not part of AVP.jag. No guessed retune here.
"""
import hashlib
import struct
import sys
import zlib
from pathlib import Path
ROM_HASH='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
ADDR=0x229D8
BASE=0x20300

def audit(rom_path, source_path):
    rom=Path(rom_path).read_bytes()
    assert hashlib.sha256(rom).hexdigest()==ROM_HASH,'non-retail or mismatched ROM'
    rdb=zlib.decompress(rom[0xE840:],31)
    mod=struct.unpack_from('>20I',rdb,ADDR-BASE)
    expected=(4,0xF1DE00,0xF1DE00,0,0x28F00,0,0x80004000)
    assert mod[:7]==expected,(mod[:7],expected)
    source=Path(source_path)
    synth=(source/'src/jaguar/host/fullsynth_host.c').read_text()
    pcm=(source/'src/platform/pc/pc_audio.c').read_text()
    game=(source/'src/platform/pc/pc_main.c').read_text()
    assert 'if(start)StartAla(AVP_FX_NEWAL);' in game
    assert 'wave=host_wave_sample(m->p[1],pos)' in synth
    assert 'host_wave_table[7][i]=(s16)lrint(n*32767.0)' in synth
    assert 'avp_jaguar_fullsynth_load_wave_rom(wave)' in pcm
    assert 'avp_jaguar_fullsynth_load_wave_rom("jagwave.rom")' in pcm
    assert '0x7a25ee5bu' in synth
    print('PASS retail AVP.jag SHA-256 and RDB alarm module')
    print('PASS FX_newal at %08X type=FM(4), carrier=modulator=JERRY NOISE 0xF1DE00' % ADDR)
    print('PASS ROM pitch=%#x, p6 control=%#x, CPU StartAla routes to reserved alarm voice' % (mod[4],mod[6]))
    print('PASS PC attempts verified optional JERRY hardware wavetable before approximate generated noise')
    print('UNRESOLVED: output pitch/timbre under approximation cannot be certified Jaguar-exact without wave-ROM or hardware reference capture')

if __name__=='__main__':
    audit(sys.argv[1],sys.argv[2] if len(sys.argv)>2 else Path(__file__).resolve().parents[2])
