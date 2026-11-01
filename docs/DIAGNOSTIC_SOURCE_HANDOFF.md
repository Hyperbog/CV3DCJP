# Historical 5H8F diagnostics needed for a clean build

The authenticated 5H8F Windows launcher refers to five Python files that postdate the pinned original source revision and are not part of the 77-file host import. They are present in the owner's uploaded `NESDev.zip`, under `CVIII-Decomp/JP/recomp/tools/` and `CVIII-Decomp/JP/recomp/tests/`.

The historical ROM host source is **not modified**. These five sources are imported separately with SHA-256 identities measured from the uploaded ZIP. The importer refuses unexpected content, source modifications, and overwrites.

## Windows staging commands

After `git pull --ff-only` on `main`, create a focused branch before staging:

```powershell
cd C:\NESDev\CV3DCJP-Repo
git switch main
git pull --ff-only origin main
git switch -c import/5h8f-diagnostics

py -3.12 .\tools\import_local_diagnostics.py `
  --workspace 'C:\NESDev\CVIII-Decomp'

py -3.12 .\tools\check_repository.py
py -3.12 -m unittest discover -s tests -p 'test_*.py' -v
git status --short
```

The five staged source files must then be reviewed, committed and pushed in that branch, followed by a pull request. No automatic push or ROM copy is performed.

## Verification and remaining limitations

- Exact SHA-256 for each of the five source files is recorded in `tools/import_local_diagnostics.py`.
- This imports diagnostic helpers, **not** original ROM content, tile layouts, or proprietary resources.
- The original game ROM remains the sole authority for game behavior.
- The legacy launcher also expects an external NESRecomp tree, optional private UI checkout, owner-local `game.toml`, asset packs, and an MSVC toolchain.
- **Do not run the historical launcher unmodified against the existing working build**: its defaults point to an existing `build-5h8f` output and it may modify a private `recomp-ui` theme file. A clean repository-only Windows build should use isolated build/output directories and avoid editing external checkout source.
- Import and test success do not establish a playable or graphically accurate native game.

Source-dependency readiness is tracked under GitHub Issue #2.
