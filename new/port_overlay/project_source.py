#!/usr/bin/env python3
"""Make a disposable PC compilation view. Never write to original source paths."""
import argparse, hashlib, json, os, shutil, importlib.util
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
_gate_spec = importlib.util.spec_from_file_location('project_damage_gate', ROOT/'new/cheats/project_damage_gate.py')
_gate = importlib.util.module_from_spec(_gate_spec)
_gate_spec.loader.exec_module(_gate)
_collision_spec = importlib.util.spec_from_file_location("project_player_collision", ROOT/"new/gameplay_fidelity/project_player_collision.py")
_collision = importlib.util.module_from_spec(_collision_spec)
_collision_spec.loader.exec_module(_collision)
_health_spec = importlib.util.spec_from_file_location("project_enemy_health", ROOT/"new/gameplay_enhancements/project_enemy_health.py")
_health = importlib.util.module_from_spec(_health_spec)
_health_spec.loader.exec_module(_health)
_stats_spec = importlib.util.spec_from_file_location("project_enemy_stats", ROOT/"new/gameplay_modifiers/project_enemy_stats.py")
_stats = importlib.util.module_from_spec(_stats_spec)
_stats_spec.loader.exec_module(_stats)
DIRECTORIES = ('src','include','new','tests','third_party','cmake','tools','scripts','docs')
def digest(data): return hashlib.sha256(data).hexdigest()
def load(): return json.loads((HERE/'manifest.json').read_text())
def replay(path, data, delta):
    if delta['path'] != path or digest(data) != delta['base_sha256']:
        raise ValueError('Incorrect original source for '+path)
    lines=data.decode('utf-8').splitlines(keepends=True)
    out=[]; pos=0
    for edit in delta['edits']:
        start,end=edit['start'],edit['end']
        if not pos <= start <= end <= len(lines): raise ValueError('Invalid edit range: '+path)
        out.extend(lines[pos:start]);out.append(edit['replacement']);pos=end
    out.extend(lines[pos:]);result=''.join(out).encode('utf-8')
    if digest(result)!=delta['output_sha256']: raise ValueError('Incorrect projected source hash: '+path)
    return result

def verify():
    m=load(); outputs={}
    for path, sha in m['reference_sha256'].items():
        data=(ROOT/path).read_bytes()
        if digest(data)!=sha: raise ValueError('Original source changed: '+path)
        if path in m['deltas']:
            delta=json.loads((HERE/m['deltas'][path]).read_text())
            outputs[path]=replay(path,data,delta)
    if not set(m['deltas'])<=set(m['reference_sha256']): raise ValueError('Unknown delta target')
    return outputs

def project(dest):
    dest=dest.resolve()
    if dest==ROOT or dest in ROOT.parents or any(ROOT/d in dest.parents or dest==ROOT/d for d in DIRECTORIES):
        raise ValueError('Projection must be outside the protected source directories')
    outputs=verify(); dest.mkdir(parents=True,exist_ok=True)
    marker=dest/'.avp_generated_projection'
    if not marker.exists() and any(dest.iterdir()): raise ValueError('Refusing to reuse non-projection directory')
    marker.write_text('Disposable AVP PC compilation view; edit new/ at source root, not here.\n')
    files={p.relative_to(ROOT).as_posix():p for name in DIRECTORIES for p in (ROOT/name).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc'}
    files.update({p.name:p for p in ROOT.iterdir() if p.is_file() and (p.suffix in ('.md','.txt','.json','.plist','.csv','.tsv','.patch','.log','.exit','.sh') or p.name in ('.gitignore','.gitattributes','LICENSE','COPYING'))})
    files['CMakeLists.txt']=HERE/'port_build.cmake'
    for name,p in sorted(files.items()):
        data=outputs[name] if name in outputs else p.read_bytes(); target=dest/name
        data=_gate.apply_damage_gate(name,data)
        data=_collision.apply_collision_fidelity(name,data)
        data=_health.apply_enemy_health(name,data)
        data=_stats.apply_enemy_stats(name,data)
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.is_file() or target.read_bytes()!=data: target.write_bytes(data)
        os.chmod(target,p.stat().st_mode & 0o777)
    for p in dest.rglob('*'):
        if p.is_file() and p!=marker and p.relative_to(dest).as_posix() not in files: p.unlink()
    print(f'PASS: {len(load()["reference_sha256"])} pristine original files; {len(outputs)} exact compatibility delta replays; {len(files)} projected files')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path);args=parser.parse_args()
    if args.output: project(args.output)
    else: print(f'PASS: {len(verify())} compatibility delta replays')
