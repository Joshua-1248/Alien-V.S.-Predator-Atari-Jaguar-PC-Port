#!/usr/bin/env python3
"""R97 retail-AVP/JERRY proof of sampler signed-volume and affected title notes.

Requires the user's original AVP.jag. Does not extract, modify or export any ROM
sample, score, DSP, or JERRY waveform bytes. Audits only instruction semantics
and scalar volumes from the supplied cartridge.
"""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_retail_dsp_master_gain_r230 import decode_original, word, fields, ROM_SHA
import hashlib
import zlib

BASE = 0x20300

def u32(p, o):
    return struct.unpack_from('>I', p, o)[0]

def signed16(v):
    v &= 65535
    return v if v < 32768 else v-65536

def audit(romfile, source):
    cpu, dsp = decode_original(romfile)
    rom=romfile.read_bytes()
    rdb=zlib.decompress(rom[0xE840:],31)
    assert hashlib.sha256(rom).hexdigest()==ROM_SHA

    # Original full-wave global music-level coefficient in the 68000 InitSynt
    # program, stored to DSP-local $F1B030 before the DSP is allowed to run.
    assert cpu[0x9854-0x8000:0x9854-0x8000+12] == bytes.fromhex(
        '23fc00007fff00f1b03023fc')
    assert fields(word(dsp,0xF1B68E))==(41,23,23) # LOAD (r23),r23
    assert fields(word(dsp,0xF1B690))==(17,23,6)  # signed IMULT
    assert fields(word(dsp,0xF1B692))==(27,15,6)  # SHARQ #15
    # A sampled patch is scaled by its encoded score velocity.  The volume
    # result is stored whole, including bit 15: MUL unsigned, SHRQ 8, STORE.
    for p,expected in ((0xF1B702,(16,26,6)),(0xF1B708,(25,8,6)),
                       (0xF1B740,(16,26,6)),(0xF1B742,(25,8,6))):
        assert fields(word(dsp,p))==expected,(hex(p),fields(word(dsp,p)))
    # The actual original compressed handler multiplies by the signed low
    # 16-bit word WITHOUT clearing its bit 15 (volume is not an unsigned 15-bit
    # field). Both handler variants use IMULT followed by SHARQ #15.
    assert fields(word(dsp,0xF1B2CC))==(17,0,13)
    assert fields(word(dsp,0xF1B2CE))==(27,15,13)
    assert fields(word(dsp,0xF1B446))==(17,5,13)
    assert fields(word(dsp,0xF1B448))==(27,15,13)
    # Do not derive output gain from absolute video RMS; prove the actual
    # 68000/JERRY scalar values used for the title's rare original samples.
    desc=0x20918-BASE
    score_ref=u32(rdb,desc)
    patch_base=u32(rdb,desc+4)
    div,pre=struct.unpack_from('>HH',rdb,desc+16)
    tempo=26590906/((div+1)*(pre+1))
    cell=score_ref & 0x7fffff
    start=u32(rom,cell)-0x800000 if (score_ref&0x80000000) else score_ref-BASE
    found={6:[],8:[]}
    for k in range(0,0x40000,8):
        when,event=struct.unpack_from('>II',rom,start+k)
        if when==0x7fffffff:break
        if not (event & 0x80000000):continue
        idx=(event>>21)&31
        if idx not in found:continue
        module_off=patch_base-BASE+80*idx
        assert u32(rdb,module_off)==32
        patch_volume=u32(rdb,module_off+4)&0xffff
        velocity=(event&127)<<6
        # $F1B030 was initialized to $7fff, then the original DSP runs
        # IMULT signed-low16 + SHARQ 15 before applying module volume.
        scaled_velocity=(velocity*32767)>>15
        result=(patch_volume*scaled_velocity)>>8
        assert result&0x8000 and result<65536
        signed=signed16(result)
        assert signed<0
        assert (result&0x7fff)!=result
        found[idx].append((when/tempo,patch_volume,velocity,result,signed))
    assert len(found[6])==len(found[8])==1,found
    host=(source/'src/jaguar/host/fullsynth_host.c').read_text()
    assert 'u32 amp=(u32)(((uint64_t)velocity*0x7fffu)>>15);' in host
    assert 'vol=m->p[1];' in host
    assert 'vol=m->p[1]&0x7fffu' not in host
    for idx,values in sorted(found.items()):
        secs,v0,vel,wordval,signed=values[0]
        print(f'PASS original TITLE3 patch {idx}: time={secs:.3f}s type32 '
              f'base-vol=0x{v0:04X} velocity={vel} final=0x{wordval:04X} '
              f'signed-IMULT={signed}')
    print('PASS original CPU master coefficient + DSP velocity, store, and signed sampler instructions')
    print('PASS host retains signed bit15 of full compressed-sample volume')
    print('LIMIT: original DSP instructions + scalar parity, not capture-audio waveform certification')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--rom',required=True,type=Path)
    p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
    args=p.parse_args()
    audit(args.rom,args.source)
