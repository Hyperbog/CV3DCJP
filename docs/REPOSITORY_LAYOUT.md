# Current repository organization

```text
.github/workflows/            ROM-free CI, source policy, C11 smoke tests
data/                         Public schemas and small non-proprietary evidence summaries
docs/                         Architecture, project status and research records
  rom-translation/            6502 translation protocol, authoritative ledger, collision evidence
  repository/                 Branch audit, historic merged-ref manifest, maintenance records
legacy/rom-backed/            Protected historical working ROM-backed code, native host and extraction tools
src/collision/                Canonical Collision.c/.h native translation, NOT yet live gameplay replacement
tests/                        Repository policy tests, collision API test, ROM-byte audit
tools/                       ROM extractors and validation tooling
  git/                        Guarded branch cleanup
```

`legacy/rom-backed/` is **not** an orphaned directory: it still includes the original-ROM executable build, launcher interfaces, graphics, audio and original-stage data extraction. Relocating or deleting its files requires a dependency-verified change and independent Windows tests. There are no local copyrighted ROMs or generated frame captures in Git.

## One-branch policy

`main` is the only permanent active development branch. Changes to native subsystem code are committed into canonical source files with tests, exact ROM offsets and a ledger update. Do not create another accumulating set of permanent phase branches. If a short-lived review branch is required, delete it after its PR is merged and `main` CI passes. The audited historical branch refs can be safely removed by [`tools/git/Prune-Merged-Branches.ps1`](../tools/git/Prune-Merged-Branches.ps1), which checks merged PR history and the exact recorded SHA before deletion.

**Source status:** C11 collision translation code is offline-tested but not live integrated; the classic launcher and experimental 426px/512px renderer remain in the historical ROM-backed tree. Widescreen is **not verified**.

## Data policy

Original ROMs, private capture binaries, extracted proprietary graphics and local builds belong outside tracked source. See [ROM authority](ROM_AUTHORITY.md), [programming specification](rom-translation/PROGRAMMING_SPEC.md), and [repository audit](repository/GIT_AUDIT_2026-11-02.md).
