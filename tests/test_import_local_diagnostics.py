"""Regression tests for the five-file local diagnostic source importer."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

script = Path(__file__).resolve().parents[1] / "tools/import_local_diagnostics.py"
spec = importlib.util.spec_from_file_location("diagnostic_import", script)
tool = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = tool
spec.loader.exec_module(tool)


class DiagnosticImportTests(unittest.TestCase):
    def test_source_list_is_exact_and_scoped(self):
        self.assertEqual(len(tool.SOURCE_HASHES), 5)
        self.assertTrue(all(k.startswith(("JP/recomp/tools/", "JP/recomp/tests/"))
                            for k in tool.SOURCE_HASHES))

    def test_missing_workspace_does_not_create_dest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            dest = root / "git"
            with self.assertRaises(FileNotFoundError):
                tool.check_sources(root / "missing", dest)
            self.assertFalse(dest.exists())

    def test_mismatched_sources_block_before_write(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for path in tool.SOURCE_HASHES:
                f = root / "local" / path
                f.parent.mkdir(parents=True, exist_ok=True)
                f.write_text("modified source\n", encoding="utf8")
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                tool.check_sources(root / "local", root / "git")
            self.assertFalse((root / "git").exists())

    def test_preexisting_source_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            first = next(iter(tool.SOURCE_HASHES))
            existing = root / "git" / first
            existing.parent.mkdir(parents=True, exist_ok=True)
            existing.write_text("pinned\n")
            with self.assertRaises(FileExistsError):
                tool.check_sources(root / "local", root / "git")
            self.assertEqual(existing.read_text(), "pinned\n")

    def test_checked_copy_is_identical(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / "git" / "JP/recomp/tests/file.py"
            tool.import_checked([(out, b"print('source')\n")])
            self.assertEqual(out.read_bytes(), b"print('source')\n")
            with self.assertRaises(FileExistsError):
                tool.import_checked([(out, b"overwritten\n")])
            self.assertEqual(out.read_bytes(), b"print('source')\n")


if __name__ == "__main__":
    unittest.main()
