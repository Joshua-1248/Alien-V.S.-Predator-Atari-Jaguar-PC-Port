#!/usr/bin/env python3
"""Read-only R100 retail cartridge check for Do_Escap sampler D1 semantics.
No copyrighted cartridge bytes are embedded or exported.
"""
import hashlib
import re
import struct
import sys
import zlib
from pathlib import Path

EXPECTED = 'b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
FIVE_TO_ONE = (0x22848,0x22898,0x228e8,0x22938,0x22988)
ZERO = 0x21718

def audit(rom_path: Path, source_path: Path):
    rom = rom_path.read_bytes()
    assert len(rom) == 4*1024*1024
    assert hashlib.sha256(rom).hexdigest() == EXPECTED, 'Not the original verified retail AVP.jag'
    rdb = zlib.decompress(rom[0xE840:],31)
    for address in FIVE_TO_ONE + (ZERO,):
        off = address - 0x20300
        typ, ctrl = struct.unpack_from('>II', rdb, off)
        expected = 0x6000 if address == ZERO else 0x2A00
        assert typ == 32 and ctrl & 0xFFFF == expected, (hex(address),hex(typ),hex(ctrl))
    source = source_path / 'src/game/mjp.c'
    text = source.read_text()
    for address in FIVE_TO_ONE:
        assert re.search(r'mjp_audio_id\(0x0*%x[uU]?,\s*0x2600\)'%address,text,re.I),hex(address)
    assert re.search(r'mjp_audio_id\(0x0*21718[uU]?,\s*-1\)',text,re.I)
    runtime=(source_path/'src/platform/pc/pc_runtime.c').read_text()
    synth=(source_path/'src/jaguar/host/fullsynth_host.c').read_text()
    assert 'avp_music_tfx_d1((unsigned)a,b)' in runtime
    assert 'tfx_store_volume(&v,d1);' in synth
    assert '(*w&0xffff0000u)|word' in synth
    print('PASS retail ROM: Five..One default $2A00, ZERO default $6000')
    print('PASS original MJP: Five..One command D1=$2600, ZERO command D1=-1')
    print('PASS PC audio bridge: D1 forwarded to original voice and DSP module volume')

if __name__ == '__main__':
    audit(Path(sys.argv[1]),Path(sys.argv[2]) if len(sys.argv)>2 else Path(__file__).resolve().parents[2])
