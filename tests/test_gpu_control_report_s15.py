#!/usr/bin/env python3
"""Synthetic, ROM-free tests of event parsing and GO transition classification."""
import io
import os
import sys
import unittest
from contextlib import redirect_stdout
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0,os.path.join(ROOT,'tools','jagref'))
from gpu_control_report import HEADER,decode,summarize,report

def table(rows):
    return ','.join(HEADER)+'\n'+ '\n'.join(','.join(str(r[k]) for k in HEADER) for r in rows)+'\n'

def evt(**kw):
    d={'field':'12','cycles':'1500','vc':'29','master':'0','addr':'f02110',
       'value':'00f03220','pc_before':'00f03000','pc_after':'00f03220',
       'go_before':'0','go_after':'0','pc68':'008080'}
    d.update(kw)
    return d

class TraceTest(unittest.TestCase):
    def test_pc_and_go_sequence(self):
        t=table([evt(),evt(addr='f02114',value='00000001',go_after='1'),
                 evt(master='1',addr='f02114',value='00000000',
                     go_before='1',go_after='0')])
        starts,stops,writes=summarize(list(decode(io.StringIO(t))))
        self.assertEqual((len(starts),len(stops),len(writes)),(1,1,1))
        self.assertEqual(starts[0]['pc_after'],0xF03220)
        out=io.StringIO()
        with redirect_stdout(out):report(list(decode(io.StringIO(t))))
        self.assertIn('writer=68000',out.getvalue())
    def test_no_start_not_inferred(self):
        rows=list(decode(io.StringIO(table([evt()]))))
        self.assertEqual(summarize(rows)[0],[])
    def test_bad_header_rejected(self):
        with self.assertRaisesRegex(ValueError,'header'):
            list(decode(io.StringIO('foo,bar\n1,2\n')))
    def test_bad_go_rejected(self):
        with self.assertRaisesRegex(ValueError,'GO state'):
            list(decode(io.StringIO(table([evt(go_after='2')]))))
    def test_bad_register_rejected(self):
        with self.assertRaisesRegex(ValueError,'register'):
            list(decode(io.StringIO(table([evt(addr='f02200')]))))
    def test_truncated_rejected(self):
        with self.assertRaisesRegex(ValueError,'malformed'):
            list(decode(io.StringIO(','.join(HEADER)+'\n1,2,3\n')))

if __name__=='__main__':unittest.main(verbosity=2)
