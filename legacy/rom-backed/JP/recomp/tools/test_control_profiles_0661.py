#!/usr/bin/env python3
"""Real production controls-profile module regression, owner-ROM-free."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

MODULE = Path(__file__).resolve().parents[1] / 'release' / 'alpha' / 'Control-Profiles.py'
spec = importlib.util.spec_from_file_location('cviii_control_profiles_0661', MODULE)
profiles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profiles)

HEADER = (b'# genuine-compatible invented host settings\r\n[Display]\r\n'
          b'WindowScale = 3\r\n[Input]\r\nPlayer1Source = 2\r\n'
          b'Player2Source = 2\r\n[Keyboard.Player1]\r\n'
          b'a = Z\r\nb = X\r\nselect = Backslash\r\nstart = Return\r\n'
          b'up = Up\r\ndown = Down\r\nleft = Left\r\nright = Right\r\n'
          b'[Keyboard.Shortcuts]\r\nsave_state = F8\r\nload_state = F9\r\n'
          b'[Game]\r\nMasterVolume = 70\r\n')


class ControlProfilesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cviii-controls-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root/'config.ini').write_bytes(HEADER)

    def original(self):
        return (self.root/'config.ini').read_bytes()

    def cli(self, *args):
        return subprocess.run([sys.executable, str(MODULE), '--root', str(self.root), *args],
                              capture_output=True, text=True)

    def test_canonical_upstream_default(self):
        self.assertEqual(profiles.active_profile(HEADER), profiles.DEFAULT)
        self.assertEqual(profiles.execute(self.root, 'show'),
                         'Current Player 1: a=Z, b=X, select=Backslash, start=Return, up=Up, down=Down, left=Left, right=Right')

    def test_wasd_switch_and_unrelated_engine_settings(self):
        message = profiles.execute(self.root, 'apply', 'wasd')
        self.assertIn('Applied wasd', message)
        raw = self.original()
        self.assertEqual(profiles.active_profile(raw), profiles.WASD)
        for field in (b'WindowScale = 3', b'Player2Source = 2',
                      b'save_state = F8', b'load_state = F9',
                      b'MasterVolume = 70'):
            self.assertIn(field, raw)
        self.assertIn(b'Player1Source = 1', raw)
        self.assertIn(b'\r\n', raw)
        backups = list((self.root/'control-backups').glob('*.bak'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), HEADER)

    def test_default_restores_player1_after_wasd(self):
        # Windows clock granularity can return the SAME nanoseconds twice.
        with mock.patch.object(profiles.time, 'time_ns', return_value=12345):
            profiles.execute(self.root, 'apply', 'wasd')
            profiles.execute(self.root, 'apply', 'default')
        self.assertEqual(profiles.active_profile(self.original()), profiles.DEFAULT)
        self.assertEqual(len(list((self.root/'control-backups').glob('*.bak'))), 2)
        self.assertIn(b'Player1Source = 1', self.original())

    def test_idempotent_profile_no_new_backup(self):
        profiles.execute(self.root, 'apply', 'wasd')
        count = len(list((self.root/'control-backups').glob('*.bak')))
        self.assertIn('already active', profiles.execute(self.root, 'apply', 'wasd'))
        self.assertEqual(len(list((self.root/'control-backups').glob('*.bak'))), count)

    def test_named_profile_round_trip(self):
        before = self.original()
        self.assertIn('Saved custom', profiles.execute(self.root, 'save', 'trevor_jp'))
        self.assertEqual(self.original(), before)
        doc = json.loads((self.root/'control-profiles/trevor_jp.json').read_text())
        self.assertEqual(doc['schema'], profiles.SCHEMA)
        profiles.execute(self.root, 'apply', 'wasd')
        profiles.execute(self.root, 'apply', 'trevor_jp')
        self.assertEqual(profiles.active_profile(self.original()), profiles.DEFAULT)

    def test_duplicate_name_save_refused_no_overwrite(self):
        profiles.execute(self.root, 'save', 'partner1')
        payload = (self.root/'control-profiles/partner1.json').read_bytes()
        with self.assertRaises(FileExistsError):
            profiles.execute(self.root, 'save', 'partner1')
        self.assertEqual((self.root/'control-profiles/partner1.json').read_bytes(), payload)

    def test_list_profiles_without_mutating_config(self):
        profiles.execute(self.root, 'save', 'keyboard1')
        raw = self.original()
        self.assertIn('keyboard1', profiles.execute(self.root, 'list'))
        self.assertEqual(self.original(), raw)
        self.assertEqual(self.cli('--list').returncode, 0)

    def test_unknown_profile_cannot_write(self):
        before = self.original()
        with self.assertRaises((ValueError, FileNotFoundError)):
            profiles.execute(self.root, 'apply', 'does-not-exist')
        self.assertEqual(self.original(), before)
        self.assertFalse((self.root/'control-backups').exists())

    def test_traversal_and_reserved_profile_ids_rejected(self):
        for name in ('../outside', '..', 'default', 'wasd', 'Bad!', 'a/b', 'A', '',
                     'x'*33, 'evil.json'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                profiles.name_ok(name)

    def test_reject_duplicate_scancodes_and_special_menu_keys(self):
        for mapping in (dict(profiles.DEFAULT, b='Z'),dict(profiles.DEFAULT, a='Escape'),
                        dict(profiles.DEFAULT, a='F4'),dict(profiles.DEFAULT, right='F10'),
                        dict(profiles.DEFAULT, up='Unsupported SDL')):
            with self.assertRaises(ValueError):
                profiles.validate(mapping)

    def test_corrupt_saved_profile_cannot_rewrite_config(self):
        folder = self.root/'control-profiles'
        folder.mkdir()
        file = folder/'bad.json'
        file.write_text('{"schema": "wrong", "bindings": {}}')
        before = self.original()
        with self.assertRaises(ValueError):
            profiles.execute(self.root, 'apply', 'bad')
        self.assertEqual(before, self.original())

    def test_missing_config_cannot_be_rebuilt_incorrectly(self):
        (self.root/'config.ini').unlink()
        with self.assertRaises(ValueError):
            profiles.execute(self.root, 'apply', 'wasd')
        self.assertFalse((self.root/'control-backups').exists())

    def test_incomplete_config_rejected_for_capture(self):
        (self.root/'config.ini').write_text('[Keyboard.Player1]\na = Z\n')
        with self.assertRaisesRegex(ValueError, 'complete'):
            profiles.execute(self.root, 'save', 'partial')

    def test_source_with_missing_target_section_preserves_unrelated(self):
        data = b'[Display]\nWindowScale = 6\n[Game]\nMasterVolume = 42\n'
        (self.root/'config.ini').write_bytes(data)
        profiles.execute(self.root, 'apply', 'wasd')
        self.assertEqual(profiles.active_profile(self.original()), profiles.WASD)
        self.assertIn(b'WindowScale = 6', self.original())
        self.assertIn(b'MasterVolume = 42', self.original())
        self.assertIn(b'Player1Source = 1', self.original())

    def test_nonfinal_section_adds_missing_keys_without_overwriting_following(self):
        content = b'[Keyboard.Player1]\na = Z\n[Audio]\nVolume = 72\n[Input]\nPlayer1Source = 0\n'
        (self.root/'config.ini').write_bytes(content)
        profiles.execute(self.root, 'apply', 'default')
        out = self.original()
        self.assertEqual(profiles.active_profile(out), profiles.DEFAULT)
        self.assertIn(b'[Audio]\nVolume = 72\n', out)

    def test_duplicate_binding_or_section_refuses_changes(self):
        for raw in (HEADER+b'[Keyboard.Player1]\na = Z\n', HEADER.replace(b'up = Up\r\n',b'up = Up\r\nup = Up\r\n')):
            (self.root/'config.ini').write_bytes(raw)
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                profiles.execute(self.root, 'apply', 'wasd')
            self.assertEqual(self.original(), raw)

    def test_no_changes_outside_player1_shortcuts(self):
        profiles.execute(self.root, 'apply', 'wasd')
        out = self.original()
        self.assertIn(b'[Keyboard.Shortcuts]\r\nsave_state = F8', out)
        self.assertIn(b'[Game]\r\nMasterVolume = 70', out)
        self.assertIn(b'Player2Source = 2', out)
        self.assertIn(b'[Display]\r\nWindowScale = 3', out)

    def test_binary_and_symlink_config_rejected(self):
        (self.root/'config.ini').write_bytes(b'[Input]\n\x00evil')
        with self.assertRaisesRegex(ValueError, 'NUL'):
            profiles.execute(self.root, 'apply', 'wasd')
        if hasattr(os, 'symlink'):
            path = self.root/'config.ini'
            path.unlink()
            try:
                path.symlink_to(self.root/'invented.ini')
            except OSError:
                return
            with self.assertRaises(ValueError):
                profiles.execute(self.root, 'apply', 'wasd')


if __name__ == '__main__':
    unittest.main(verbosity=2)
