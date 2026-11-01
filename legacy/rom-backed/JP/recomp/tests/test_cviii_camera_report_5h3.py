"""Synthetic review of candidate room links and optional reference parity."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from cviii_camera_report_5h3 import analyze


def observation(frame, stage=0, area=0, room=0, qualified=1):
    return dict(frame=frame, stage=stage, area=area, room=room,
                transition=0, camera_ram=frame, ppu_x=frame,
                ppu_y=0, ppu_bg_table=4096, qualified=qualified,
                camera_x=frame, camera_y=0, top=32,
                tested=100, matched=97, world_placed=1, source=5)


class CameraReportTests(unittest.TestCase):
    def test_known_edge_evidence(self):
        a=observation(1);b=observation(2, area=1)
        b["transition"]=1
        report=analyze([a,b])
        self.assertEqual(report["observed_connections"][0]["status"],"SOURCE_VERIFIED_JP0803")
        self.assertEqual(report["independent_mesen_parity"],"NOT_EXERCISED")

    def test_unknown_not_promoted(self):
        a=observation(1);b=observation(2,area=9)
        b["transition"]=1
        report=analyze([a,b])
        self.assertEqual(report["observed_connections"][0]["status"],"OBSERVED_ONLY_NOT_PROVEN")
        self.assertEqual(len(report["unverified_connections"]),1)

    def test_reference_mismatch(self):
        a=observation(1);b=observation(1)
        b["ppu_x"] += 1
        self.assertEqual(analyze([a],[b])["independent_mesen_parity"],"MISMATCH")

    def test_transition_must_be_marked(self):
        with self.assertRaises(ValueError):
            analyze([observation(1),observation(2,area=1)])


if __name__=="__main__":
    unittest.main()
