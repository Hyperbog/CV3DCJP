import csv
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from cviii_camera_report_5h4 import load,report,REQUIRED
class ReportTest(unittest.TestCase):
    def test_gate_report_and_transition(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'trace.csv'
            with path.open('w',newline='') as f:
                writer=csv.DictWriter(f,fieldnames=REQUIRED)
                writer.writeheader()
                for i,(area,reason) in enumerate(((0,'CAMERA_MISMATCH'),(1,'CHR_UNRESOLVED'),(1,'ATLAS_PRESENTED'))):
                    values=dict.fromkeys(REQUIRED,0)
                    values.update(frame=1566+i,stage=0,area=area,room=0,
                                  gate_5h4=(7,5,10)[i],gate_label=reason,
                                  best_tested=120,best_matched=10+i,
                                  qualified=int(i==2),source=(5 if i==2 else 3),
                                  transition=int(i==1),anchor=2,chr_resolved=int(i!=1))
                    writer.writerow(values)
            result=report(load(path),span=2)
            self.assertEqual(result['first_verified_transition_frame'],1567)
            self.assertEqual(result['reason_counts']['CHR_UNRESOLVED'],1)
            self.assertEqual(result['unobserved_room_edges_promoted'],0)
            self.assertEqual(result['independent_mesen_parity'],'NOT_EXERCISED')
            with path.open('w') as f:f.write('frame,stage,area\n1,0,0\n')
            with self.assertRaises(ValueError):load(path)
if __name__=='__main__':unittest.main()
