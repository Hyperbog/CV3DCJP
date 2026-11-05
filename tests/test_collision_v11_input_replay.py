"""ROM-free controller to named-button schedule contract regression tests.

Synthetic state sequences DO NOT validate cross-emulator input frame origin.
"""
import csv
import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'instrumentation'))
import collision_v11_make_native_input as replay

class V11InputContract(unittest.TestCase):
    def setUp(self):
        t=tempfile.TemporaryDirectory()
        self.addCleanup(t.cleanup)
        self.root=Path(t.name)
        self.ctrl=self.root/'ctrl.csv'
        self.col=self.root/'collision.csv'
        self.rom=self.root/'not-a-real-rom.nes'
        self.rom.write_bytes(b'SYNTHETIC'+b'x'*(393232-9))
        self.events=[
            (0,2,100,0,0),(0,3,200,0,0),
            (1,2,100,0,0),(1,3,200,4,0),
            (1,4,300,0x81,0),(1,5,400,0x81,0),
            (1,6,500,0,0)]
        self.write_events()
        with self.col.open('w',newline='') as f:
            w=csv.writer(f)
            w.writerow(('caller_pc','entry_cycle','return_cycle'))
            w.writerow((0x8429,250,270))
            w.writerow((0x96ed,350,370))

    def write_events(self):
        with self.ctrl.open('w',newline='') as f:
            w=csv.writer(f);w.writerow(replay.FIELDS);w.writerows(self.events)

    def test_buttons_use_name_identity(self):
        self.assertEqual(replay.buttons(0),'-')
        self.assertEqual(replay.buttons(4),'SELECT')
        self.assertEqual(replay.buttons(0x81),'A+RIGHT')
        self.assertEqual(replay.buttons(0x18),'START+UP')
        self.assertEqual(replay.buttons(0xc3),'A+B+LEFT+RIGHT')

    def test_epoch_one_and_source_changes(self):
        rows,counts=replay.load_epoch(self.ctrl,1)
        self.assertEqual(len(rows),5)
        self.assertEqual(counts,{0:2,1:5})
        self.assertEqual(replay.changes(rows,0),
                         [(0,0,0),(1,4,0),(2,0x81,0),(4,0,0)])

    def test_three_unverified_shifts_and_refusal_to_overwrite(self):
        fingerprint=hashlib.sha256(self.rom.read_bytes()).hexdigest()
        with patch.object(replay,'EXPECTED_ROM',fingerprint):
            r=replay.export(self.rom,self.ctrl,self.col,self.root/'out')
        self.assertEqual(r['input_poll_rows'],5)
        self.assertEqual(r['state_runs'],4)
        self.assertFalse(r['start_phase_alignment_proven'])
        self.assertEqual(r['collision_window']['mesen_last_input_poll_before_first_collision'],3)
        self.assertEqual(r['collision_window']['mesen_last_input_poll_before_last_collision'],4)
        a=(self.root/'out'/'cv3dcjp-native-input-UNVERIFIED-shift-0.txt').read_text()
        b=(self.root/'out'/'cv3dcjp-native-input-UNVERIFIED-shift-1.txt').read_text()
        self.assertIn('1 SELECT\n',a)
        self.assertIn('2 A+RIGHT\n',a)
        self.assertIn('2 SELECT\n',b)
        with patch.object(replay,'EXPECTED_ROM',fingerprint):
            with self.assertRaises(FileExistsError):
                replay.export(self.rom,self.ctrl,self.col,self.root/'out')

    def test_frame_gap_fails(self):
        self.events.pop(4);self.write_events()
        with self.assertRaisesRegex(ValueError,'frame sequence'):
            replay.load_epoch(self.ctrl,1)

    def test_clock_regression_fails(self):
        self.events[4]=(1,4,150,0x81,0);self.write_events()
        with self.assertRaisesRegex(ValueError,'non-monotone'):
            replay.load_epoch(self.ctrl,1)

    def test_wrong_rom_fails(self):
        with self.assertRaisesRegex(ValueError,'wrong original Japanese ROM'):
            replay.export(self.rom,self.ctrl,self.col,self.root/'out')

    def test_unbounded_shift_fails(self):
        rows,_=replay.load_epoch(self.ctrl,1)
        with self.assertRaisesRegex(ValueError,'invalid frame shift'):
            replay.changes(rows,-1)

if __name__=='__main__':
    unittest.main()
