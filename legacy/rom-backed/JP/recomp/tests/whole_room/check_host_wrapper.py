#!/usr/bin/env python3
"""Ensure tested synthetic-host comparison wrapper equals live host callback.

Does not build an emulator and does not require a ROM. Run from repository root.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[4]
edition = (root / 'JP/recomp/native/enhanced/cviii_edition.c').read_text()
fixture = (Path(__file__).with_name('host_wrapper_test.c')).read_text()
start = 'static const uint32_t *present_room_comparison('
end = 'static void audio_mix('
assert edition.count(start) == 1 and edition.count(end) == 1
assert fixture.count(start) == 1
actual = edition[edition.index(start):edition.index(end)].rstrip()
expected = fixture[fixture.index(start):fixture.index('\n\nstatic void select_room(', fixture.index(start))].rstrip()
assert actual == expected, 'Synthetic host test wrapper differs from native host callback'
print('PASS: native Enhanced comparison callback matches synthetic-host regression test')