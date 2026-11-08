#!/usr/bin/env python3
"""Build a user-local Prototype Texture Restoration pack, without redistributing game art.

Linux: python3 tools/create_prototype_texture_pack.py --oct 'Oct 1993.7z' \\
    --aug '1993-08-18.7z' --apr '1994-04-08.7z' \\
    --output prototype_texture_restoration

Directory inputs are also accepted if the archives have already been extracted.
Uses only Python stdlib + system libarchive for .7z. Output is 128x128 raw RGB888 plus a checksum header (.prgb).
"""
import argparse, hashlib, json, os, pathlib, shutil, struct, sys, tempfile, zlib

BASE=pathlib.Path(__file__).resolve().parent
PNG_SIG=b'\x89PNG\r\n\x1a\n'

def paeth(a,b,c):
 p=a+b-c
 d=(abs(p-a),abs(p-b),abs(p-c))
 return (a,b,c)[d.index(min(d))]

def png_rgb8(path):
 blob=path.read_bytes()
 if not blob.startswith(PNG_SIG): raise ValueError(f'Not a PNG: {path}')
 pos=8; compressed=[]; desc=None
 while pos+12<=len(blob):
  n=struct.unpack('>I',blob[pos:pos+4])[0]
  typ=blob[pos+4:pos+8]
  if n>len(blob)-pos-12:raise ValueError(f'Bad PNG chunk: {path}')
  data=blob[pos+8:pos+8+n]
  if zlib.crc32(typ+data) != struct.unpack('>I',blob[pos+8+n:pos+12+n])[0]:raise ValueError(f'Bad PNG CRC: {path}')
  pos+=12+n
  if typ==b'IHDR':
   desc=struct.unpack('>IIBBBBB',data)
  elif typ==b'IDAT':compressed.append(data)
  elif typ==b'IEND':break
 if desc != (128,128,8,2,0,0,0):raise ValueError(f'Expected 128x128 RGB8 non-interlaced PNG: {path} got {desc}')
 raw=zlib.decompress(b''.join(compressed));stride=128*3
 if len(raw)!=128*(stride+1):raise ValueError(f'Unexpected PNG decompressed length: {path}')
 prior=bytearray(stride); out=bytearray();offset=0
 for _ in range(128):
  mode=raw[offset];offset+=1;row=bytearray(raw[offset:offset+stride]);offset+=stride
  if mode>4:raise ValueError(f'Unexpected PNG filter {mode}: {path}')
  for i in range(stride):
   a=row[i-3] if i>=3 else 0;b=prior[i];c=prior[i-3] if i>=3 else 0
   if mode==1:pred=a
   elif mode==2:pred=b
   elif mode==3:pred=(a+b)//2
   elif mode==4:pred=paeth(a,b,c)
   else:pred=0
   row[i]=(row[i]+pred)&255
  out.extend(row);prior=row
 return bytes(out)

def main():
 ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
 ap.add_argument('--oct',required=True,help='Oct 1993.7z or extracted directory')
 ap.add_argument('--aug',required=True,help='1993-08-18.7z or extracted directory')
 ap.add_argument('--apr',required=True,help='1994-04-08.7z or extracted directory')
 ap.add_argument('--output',default='prototype_texture_restoration',help='Destination beside game executable')
 args=ap.parse_args()
 mapping=json.loads((BASE/'prototype_texture_restoration_manifest.json').read_text())
 output=pathlib.Path(args.output).resolve();output.mkdir(parents=True,exist_ok=True)
 from avp_7z_reader import files as read7z
 with tempfile.TemporaryDirectory(prefix='avp_proto_assets_') as t:
  roots={}
  for k,src in [('oct',args.oct),('aug',args.aug),('apr',args.apr)]:
   src=pathlib.Path(src).resolve()
   if src.is_dir():roots[k]=src
   elif src.is_file():
    root=pathlib.Path(t)/k;root.mkdir();read7z(str(src),str(root));roots[k]=root
   else:raise FileNotFoundError(src)
  # Preflight ALL entries before touching an existing pack. A wrong extracted
  # file must never quietly create a mismatched replacement under a valid ID.
  verified=[];provenance=[]
  for slot in mapping['texture_slots']:
   k=slot['priority']; id=slot['retail_file_id']
   if k=='retail':continue
   png=roots[k]/slot['selected_member']
   blob=png.read_bytes()
   if hashlib.sha256(blob).hexdigest()!=slot['source_png_sha256']:
    raise ValueError(f'Source PNG mismatch: slot {id:02}, {png}')
   data=png_rgb8(png)
   if hashlib.sha256(data).hexdigest()!=slot['source_rgb_sha256']:
    raise ValueError(f'Decoded texture mismatch: slot {id:02}, {png}')
   crc=zlib.crc32(data)
   if f'{crc:08x}'!=slot['source_rgb_crc32']:
    raise ValueError(f'Unexpected RGB CRC: slot {id:02}')
   packed=struct.pack('<8sII',b'AVPTEX1',len(data),crc)+data
   verified.append((id,packed))
   provenance.append({'retail_file_id':id,'source_tier':k,'source_member':png.name,
                      'sha256':hashlib.sha256(data).hexdigest(),
                      'crc32':f'{crc:08x}','format':'AVPTEX1'})
  # Stage inside the output directory so file replacement remains atomic.
  with tempfile.TemporaryDirectory(prefix='.avp_pack_stage_',dir=output) as staging:
   stage=pathlib.Path(staging)
   for id,packed in verified:(stage/f'{id:02}.prgb').write_bytes(packed)
   (stage/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')
   for id,_ in verified:os.replace(stage/f'{id:02}.prgb',output/f'{id:02}.prgb')
   os.replace(stage/'PROVENANCE.json',output/'PROVENANCE.json')
  # Clear only obsolete numbered S105 RGB files; never delete unrelated files.
  for id in range(56):
   old=output/f'{id:02}.rgb'
   if old.is_file():old.unlink()
  # Any orphan framed file from a previous mapping is also not trustworthy.
  selected={id for id,_ in verified}
  for id in range(56):
   if id not in selected:
    old=output/f'{id:02}.prgb'
    if old.is_file():old.unlink()
  print(f'Created {len(provenance)} verified image replacements in {output}')
  print('Unmatched slots fall back to original ROM when the Enhancement is enabled.')

if __name__=='__main__':main()
