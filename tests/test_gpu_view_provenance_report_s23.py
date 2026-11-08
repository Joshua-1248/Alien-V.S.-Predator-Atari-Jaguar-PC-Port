#!/usr/bin/env python3
"""S23 pure synthetic CPU writer provenance format and validation tests."""
import io
import contextlib
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'jagref'))
from gpu_view_provenance_report import HEADER, decode, report


def payload(lines):
    return ','.join(HEADER) + '\n' + '\n'.join(lines) + '\n'

class Tests(unittest.TestCase):
    def test_valid_mixed_and_report(self):
        rows = list(decode(io.StringIO(payload([
            '18,001200,10,0,1,3,802000,80200a,2,20',
            '20,001400,0,1,0,0,000000,000000,0,0']))))
        self.assertEqual(len(rows), 2)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            report(rows)
        self.assertIn('$80200A', out.getvalue())
        self.assertIn('no observed CPU write: 1', out.getvalue())
    def test_reject_missing_writer_but_count(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO(payload(['18,001200,1,0,0,1,000000,000000,0,0']))))
    def test_reject_writer_without_writes(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO(payload(['18,001200,1,0,1,0,802000,802000,0,0']))))
    def test_reject_bad_field_order(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO(payload(['18,001200,1,0,1,2,802000,802000,5,4']))))
    def test_reject_bad_header(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO('wrong,header\n')))
    def test_reject_unsorted(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO(payload(['20,001400,1,0,0,0,000000,000000,0,0',
                                           '18,001200,1,0,0,0,000000,000000,0,0']))))
    def test_reject_unread_page(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO(payload(['18,001200,0,0,1,3,802000,802000,0,1']))))
    def test_reject_bad_pc(self):
        with self.assertRaises(ValueError):
            list(decode(io.StringIO(payload(['18,001200,1,0,1,1,1802000,802000,0,0']))))

if __name__ == '__main__':
    unittest.main()
