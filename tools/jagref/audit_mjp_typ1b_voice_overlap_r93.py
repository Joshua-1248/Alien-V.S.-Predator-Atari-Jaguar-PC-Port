#!/usr/bin/env python3
"""Read-only retail ROM TYP1B lifetime/voice-overlap audit.

This is not a physical Jaguar/JERRY DSP timing model. It checks the retail
module's sample end/pitch fields against the currently reconstructed host.
"""
import argparse,hashlib,zlib,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--rom',required=True,type=Path)
a=p.parse_args();raw=a.rom.read_bytes()
assert len(raw)==4*1024*1024
assert hashlib.sha256(raw).hexdigest()=='b31ca5c2415881ce50d0c076d327297547214a6240f0058b0f225a74f7ce440b'
rdb=zlib.decompress(raw[0xe840:],31)
o=0x221b8-0x20300
module=struct.unpack_from('>20I',rdb,o)
assert module[0]==32 and module[1]==0x1000 and module[3]==0x100 and module[4]==0x620ff
# Current host synth uses a 20774 sample/s PCM clock; Jaguar's exact DSP
# serial clock / output-sample phase still requires validation.
frames=(module[4]+module[3]-1)//module[3]
print(f'TYP1B module $221B8: type={module[0]}, priority={module[1]}, rate={module[3]}, end=${module[4]:X}')
print(f'Current host effect duration: {frames} samples / 20774 Hz = {1000*frames/20774:.3f} ms.')
print('Retail Show_Tex calls TFX for non-space/underscore chars in modes 1/2/4/7, except fast.')
print('Retail MAIN postgame ProtectVoices(7) leaves 1 generic effect lane; cold title InitSynt leaves 6.')
print('Effect requests and audible results MUST be tested separately; do not skip arbitrary letters.')
