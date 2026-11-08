#!/usr/bin/env python3
"""RE24/S14 independent, synthetic and non-ROM descriptor audit regressions."""
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'jagref'))
import gpu_overlay_descriptors as g


def w16(b, at, n): struct.pack_into('>H', b, at, n)
def w32(b, at, n): struct.pack_into('>I', b, at, n)


def fixture():
    gpu = bytearray(4096)
    ram = bytearray(2 * 1024 * 1024)
    desc = [0x12000, 0x12100]
    entries = [0xF03000, 0xF03060]
    tabs = [0x7F64, 0x7F68]
    src = [0x18000, 0x19000]
    vars_ = [0x280, 0x284]
    patch_lists = [0x12200, 0x12300]
    vals = [0x1234ABCD, 0xCAFE1357]
    for k in range(2):
        off = entries[k] - g.BASE
        # resident MOVEI r1,table; LOAD; MOVEI r4,length
        w16(gpu, off, 0x9801)
        w16(gpu, off + 2, tabs[k])
        w16(gpu, off + 4, 0)
        w16(gpu, off + 6, 0xA421)
        w16(gpu, off + 8, 0x9804)
        w16(gpu, off + 10, 16)
        w16(gpu, off + 12, 0)
        w32(ram, tabs[k], src[k])
        w32(ram, desc[k], 0x00800000 + k * 0x1000)
        w32(ram, desc[k] + 4, 0x00800040 + k * 0x1000)
        w32(ram, desc[k] + 8, src[k])
        w32(ram, desc[k] + 12, entries[k] + 8)
        target = g.GPU_OVERLAY + 0x20
        w32(ram, desc[k] + 16, patch_lists[k])
        w32(ram, patch_lists[k], target)
        w32(ram, patch_lists[k] + 4, vars_[k])
        w32(ram, patch_lists[k] + 8, 0xFFFFFFFF)
        w32(ram, vars_[k], vals[k])
        w16(ram, src[k] + 0x20, 0x9802)
        w16(ram, src[k] + 0x22, vals[k] & 0xFFFF)
        w16(ram, src[k] + 0x24, vals[k] >> 16)
    return gpu, ram, desc, vals


class DescriptorTest(unittest.TestCase):
    def test_verified_two_stage_lookup(self):
        gpu, ram, desc, vals = fixture()
        data = g.discover(gpu, ram)
        self.assertEqual(len(data), 2)
        self.assertEqual([row['descriptor'] for row in data], desc)
        self.assertEqual([row['fixups'][0]['movei_immediate'] for row in data], vals)
        self.assertTrue(all(row['fixups'][0]['matches'] for row in data))

    def test_indirect_patch_list_pointer_is_required(self):
        gpu, ram, desc, _ = fixture()
        # 68000 MOVEA.L 16(A4),A0 dereferences this 32-bit pointer.
        self.assertEqual(g.be32(ram, desc[0] + 16), 0x12200)
        # The earlier parser incorrectly treated this word as a GPU target.
        self.assertNotEqual(g.be32(ram, desc[0] + 16), g.GPU_OVERLAY + 0x20)
        self.assertEqual(len(g.discover(gpu, ram)[0]['fixups']), 1)

    def test_invalid_patch_list_pointer_rejected(self):
        gpu, ram, desc, _ = fixture()
        for pointer in (g.DRAM_SIZE, g.DRAM_SIZE - 2, 0x123):
            w32(ram, desc[0] + 16, pointer)
            self.assertIsNone(g.discover(gpu, ram)[0]['descriptor'])

    def test_empty_indirect_patch_list_allowed(self):
        gpu, ram, desc, _ = fixture()
        w32(ram, 0x12200, 0xFFFFFFFF)
        entries = g.discover(gpu, ram)
        self.assertEqual(entries[0]['descriptor'], desc[0])
        self.assertEqual(entries[0]['fixups'], [])

    def test_byte_order_low_first(self):
        _, ram, _, vals = fixture()
        self.assertEqual(g.movei_imm(ram, 0x18020), vals[0])
        self.assertNotEqual(g.be32(ram, 0x18022), vals[0])

    def test_wrong_source_not_accepted(self):
        gpu, ram, desc, _ = fixture()
        w32(ram, 0x280, 0x01020304)
        stage = g.sequencer(gpu)[0]
        with self.assertRaisesRegex(ValueError, 'RAM variable'):
            g.parse_descriptor(ram, desc[0], (stage[0], stage[1], 0x18000, stage[2]))
        self.assertIsNone(g.discover(gpu, ram)[0]['descriptor'])

    def test_non_movei_not_accepted(self):
        gpu, ram, _, _ = fixture()
        w16(ram, 0x18020, 0x0002)
        self.assertIsNone(g.discover(gpu, ram)[0]['descriptor'])

    def test_unavailable_source_not_accepted(self):
        gpu, ram, desc, _ = fixture()
        w32(ram, 0x12204, g.DRAM_SIZE + 4)
        self.assertIsNone(g.discover(gpu, ram)[0]['descriptor'])

    def test_missing_descriptor_is_not_guessed(self):
        gpu, ram, desc, _ = fixture()
        w32(ram, desc[1] + 12, 0x123456)
        data = g.discover(gpu, ram)
        self.assertIsNone(data[1]['descriptor'])
        self.assertIn('not found', data[1]['status'])

    def test_ambiguous_valid_candidates_rejected(self):
        gpu, ram, desc, _ = fixture()
        ram[0x12400:0x12414] = ram[desc[0]:desc[0]+20]
        with self.assertRaisesRegex(ValueError, 'ambiguous descriptors'):
            g.discover(gpu, ram)

    def test_parameterized_pipeline_uses_sources_not_constants(self):
        gpu, ram, _, _ = fixture()
        with tempfile.TemporaryDirectory() as d:
            gp, dr, mf, generated = (os.path.join(d, f) for f in
                                     ('gpu.bin','dram.bin','params.json','generated.c'))
            with open(gp,'wb') as f: f.write(gpu)
            with open(dr,'wb') as f: f.write(ram)
            mapping={'format':'AVP-RE24-S14-verified-overlay-fixups-v1',
                     'stages':g.discover(gpu,ram)}
            with open(mf,'w',encoding='utf8') as f: json.dump(mapping,f)
            cmd=[sys.executable,os.path.join(ROOT,'tools','jagref',
                                              'gpu_pipeline_recomp.py'),gp,dr,
                 '--patch-map',mf]
            proc=subprocess.run(cmd,text=True,capture_output=True)
            self.assertEqual(proc.returncode,0,proc.stderr)
            self.assertIn('R[2]=g->param[0];',proc.stdout)
            self.assertIn('R[2]=g->param[1];',proc.stdout)
            self.assertIn('#define AVP_GPU_PARAM_COUNT 2u',proc.stdout)
            self.assertNotIn('R[2]=0x1234abcdu;',proc.stdout)
            self.assertIn('avp_gpu_resident_hash=',proc.stdout)
            self.assertIn('avp_gpu_overlay_hashes[]=',proc.stdout)
            with open(generated,'w') as f: f.write(proc.stdout)
            cc=os.environ.get('CC','cc')
            check=os.path.join(ROOT,'tools','jagref','gpu_pipeline_check.c')
            args=[cc,'-std=c99','-fsyntax-only','-w','-I'+os.path.join(ROOT,'include'),
                  '-DPIPELINE_SOURCE="'+generated+'"',check]
            result=subprocess.run(args,text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            # Change both verified source values and the corresponding MOVEI
            # bytes.  The function implementations must be identical: only the
            # parameter metadata may change between these captures.
            for dest,var,value in ((0x18020,0x280,0xBEEFF00D),
                                   (0x19020,0x284,0xdeadbeef)):
                w32(ram,var,value)
                w16(ram,dest+2,value & 0xffff)
                w16(ram,dest+4,value >> 16)
            with open(dr,'wb') as f: f.write(ram)
            mapping['stages']=g.discover(gpu,ram)
            with open(mf,'w',encoding='utf8') as f: json.dump(mapping,f)
            proc2=subprocess.run(cmd,text=True,capture_output=True)
            self.assertEqual(proc2.returncode,0,proc2.stderr)
            funcs=lambda s: s.split('static const struct { uint32_t entry,table,src,longs')[0]
            self.assertEqual(funcs(proc.stdout),funcs(proc2.stdout))
            # The resident and normalized stage fingerprints must also be
            # stable across verified 68000-patched MOVEI immediate values.
            import re
            extract=lambda x:re.findall(r'UINT64_C\(0x[0-9a-f]{16}\)',x)
            self.assertEqual(extract(proc.stdout),extract(proc2.stdout))

    def test_dynamic_generator_rejects_stale_map(self):
        gpu, ram, _, _ = fixture()
        with tempfile.TemporaryDirectory() as d:
            gp,dr,mf=[os.path.join(d,f) for f in ('gpu.bin','dram.bin','params.json')]
            with open(gp,'wb') as f: f.write(gpu)
            with open(dr,'wb') as f: f.write(ram)
            mapping={'format':'AVP-RE24-S14-verified-overlay-fixups-v1',
                     'stages':g.discover(gpu,ram)}
            mapping['stages'][0]['fixups'][0]['variable_value'] ^= 1
            with open(mf,'w') as f: json.dump(mapping,f)
            cmd=[sys.executable,os.path.join(ROOT,'tools','jagref',
                                              'gpu_pipeline_recomp.py'),gp,dr,
                 '--patch-map',mf]
            proc=subprocess.run(cmd,text=True,capture_output=True)
            self.assertNotEqual(proc.returncode,0)
            self.assertIn('not a verified patched MOVEI',proc.stderr)

    def test_cli_json_matches_synthetic(self):
        gpu, ram, desc, _ = fixture()
        with tempfile.TemporaryDirectory() as d:
            gp, dr, out = (os.path.join(d, f) for f in ('gpu.bin','dram.bin','out.json'))
            with open(gp,'wb') as f: f.write(gpu)
            with open(dr,'wb') as f: f.write(ram)
            res=subprocess.run([sys.executable, g.__file__, gp, dr, '--json', out],
                               text=True, capture_output=True)
            self.assertEqual(res.returncode, 0, res.stderr + res.stdout)
            with open(out, encoding='utf8') as f: info=json.load(f)
            self.assertEqual([r['descriptor'] for r in info['stages']], desc)

if __name__ == '__main__':
    unittest.main(verbosity=2)
