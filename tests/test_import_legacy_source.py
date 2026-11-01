"""Fail-closed behavior of the historical ROM-host source importer."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

src=Path(__file__).resolve().parents[1]/"tools/import_legacy_source.py"
spec=importlib.util.spec_from_file_location("cviii_source_import",src)
mod=importlib.util.module_from_spec(spec)
sys.modules[spec.name]=mod
spec.loader.exec_module(mod)

class ImportTests(unittest.TestCase):
    def test_rejects_path_escape(self):
        for name in ("../game.nes","/tmp/game.nes","C:\\private\\game.nes"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError): mod.safe_path(name)

    def test_rejects_binary_or_oversized(self):
        for data in (b"NES\x1a\0",b"abc\0def",b"x"*1048577):
            with self.assertRaises(ValueError): mod.source(data,"file.c")

    def test_rejects_mismatched_original_host(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with zipfile.ZipFile(root/"test.zip","w") as z:
                z.writestr("CMakeLists.txt","not the pinned source\n")
            with self.assertRaisesRegex(ValueError,"authenticated"):
                mod.collect(root/"test.zip",root)

    def test_missing_local_sources_block_staging(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with zipfile.ZipFile(root/"test.zip","w") as z:
                for key in mod.ANCHORS:
                    z.writestr(key,"not a real archive\n")
            with self.assertRaises(ValueError):
                mod.stage(root/"test.zip",root,root/"out")
            self.assertFalse((root/"out").exists())

    def test_hash_is_deterministic(self):
        self.assertEqual(mod.sha(b"abc"),"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")

if __name__=="__main__": unittest.main()
