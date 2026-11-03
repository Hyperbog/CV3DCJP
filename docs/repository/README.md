# Repository maintenance and branch provenance

- [Git audit — 2026-11-02](GIT_AUDIT_2026-11-02.md): verified cleanup decisions and remaining requirements.
- [Merged branch inventory](merged-branch-inventory.json): exact previous remote SHA and merged PR number for each safe-to-prune branch. This is immutable audit evidence, not a live branch list.
- [Branch pruning tool](../../tools/git/Prune-Merged-Branches.ps1): refuses deletion by default, validates each merged PR and SHA before remote removal.

**Working branch: `main` only.** ROM-backed gameplay remains protected; zero collision bytes have been formally retired.
