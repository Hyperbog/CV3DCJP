"""No-ROM checks for the recorded merged branch cleanup inventory."""
import json
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class BranchInventoryTests(unittest.TestCase):
    def test_manifest_is_sha_guarded_and_only_merged_prs(self):
        info=json.loads((ROOT/'docs/repository/merged-branch-inventory.json').read_text())
        self.assertEqual(info['repository'],'Hyperbog/CV3DCJP')
        self.assertEqual(info['default_branch'],'main')
        rows=info['branches']
        self.assertEqual(len(rows),14)
        self.assertEqual(len({r['name'] for r in rows}),len(rows))
        self.assertEqual(len({r['merged_pr'] for r in rows}),len(rows))
        self.assertNotIn('main',{r['name'] for r in rows})
        for rec in rows:
            self.assertRegex(rec['expected_sha'],r'^[0-9a-f]{40}$')
            self.assertGreater(rec['merged_pr'],0)
    def test_guarded_script_dry_run_default(self):
        script=(ROOT/'tools/git/Prune-Merged-Branches.ps1').read_text()
        for required in ('[switch]$Execute','--force-with-lease',
                         'Invoke-RestMethod','remote','expected_sha','merged_at'):
            self.assertIn(required,script)
        self.assertIn("$branch -eq 'main'",script)

if __name__=='__main__': unittest.main()
