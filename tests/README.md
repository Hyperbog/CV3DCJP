# Tests

- `test_repository.py`, `test_*.py`: ROM-free source-policy and extractors regression tests run in CI.
- `test_collision_api.c`: native C11 collision interface smoke test, run by Linux CI.
- `check_collision_ledger.py --rom /path/to/original-japanese.nes`: owner-local ROM identity, instruction byte and physical PRG span audit. This private-ROM gate is not run on GitHub CI.

No unit test proves live emulator timing, complete input handling or widescreen parity. Preserve independent game comparisons for any runtime replacement.
