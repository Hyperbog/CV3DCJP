"""Contract tests for the repository's no-proprietary-binaries policy."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_repository import inspect  # noqa: E402


class RepositoryPolicyTests(unittest.TestCase):
    def test_source_is_allowed(self):
        self.assertEqual(inspect("src/renderer/camera.cpp", b"int x = 1;\\n"), [])

    def test_rom_extension_is_rejected(self):
        self.assertTrue(inspect("data/game.nes", b"anything"))

    def test_rom_header_is_rejected_even_with_innocent_name(self):
        self.assertTrue(inspect("data/notes.txt", b"NES\\x1a" + b"x" * 16))

    def test_private_capture_is_rejected(self):
        self.assertTrue(inspect("captures/test.csv", b"frame,x\\n"))

    def test_large_file_is_rejected(self):
        self.assertTrue(inspect("data/huge.txt", b"x" * (5 * 1024 * 1024 + 1)))


if __name__ == "__main__":
    unittest.main()
