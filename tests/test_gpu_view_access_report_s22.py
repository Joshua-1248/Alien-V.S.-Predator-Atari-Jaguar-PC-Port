#!/usr/bin/env python3
"""Pure synthetic tests. No AVP memory, images, or ROM data."""
import io,os,sys,unittest
from contextlib import redirect_stdout
sys.path.insert(0,os.path.join(os.path.dirname(__file__),'..','tools','jagref'))
from gpu_view_access_report import HEADER,decode,ranges,report

def row(page,gr=0,gw=0,br=0,bw=0):
    return f'{page},{page*256:06x},{gr},{gw},{br},{bw},f03220,f03226'
def source(*lines):return io.StringIO(','.join(HEADER)+'\n'+'\n'.join(lines)+'\n')

class Test(unittest.TestCase):
    def test_merge_and_separate(self):
        rows=list(decode(source(row(0,1),row(1,3),row(3,0,2,2,0))))
        self.assertEqual(ranges(rows,'gpu_reads'),[(0,512,4)])
        self.assertEqual(ranges(rows,'blitter_reads'),[(768,1024,2)])
        out=io.StringIO()
        with redirect_stdout(out): report(rows,10)
        self.assertIn('$000000..$0001FF',out.getvalue())
        self.assertIn('resource labels inferred',out.getvalue())
    def test_bad_header(self):
        with self.assertRaisesRegex(ValueError,'header'):list(decode(io.StringIO('page,x\n')))
    def test_unsorted_duplicate(self):
        with self.assertRaisesRegex(ValueError,'unsorted'):list(decode(source(row(3,1),row(3,2))))
    def test_out_of_bounds(self):
        with self.assertRaisesRegex(ValueError,'physical address'):list(decode(source(row(8192,1))))
    def test_negative_count(self):
        with self.assertRaisesRegex(ValueError,'counter'):list(decode(source(row(2,-1))))
    def test_bad_address(self):
        x=row(3,1).replace('000300','000400')
        with self.assertRaisesRegex(ValueError,'physical address'):list(decode(source(x)))
    def test_truncated(self):
        with self.assertRaisesRegex(ValueError,'malformed'):list(decode(source('1,000100,1')))
    def test_empty_row(self):
        with self.assertRaisesRegex(ValueError,'empty page'):list(decode(source(row(8))))
if __name__=='__main__':unittest.main(verbosity=2)
