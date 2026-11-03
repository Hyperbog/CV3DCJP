# Contributing to CV3DCJP

## One permanent branch

Use **`main`** as the canonical work branch. Commit a focused, independently testable change to one subsystem at a time. Do not create persistent phase/version/diagnostic branches. If short-lived PR review is necessary, merge verified changes and delete the branch promptly. No forced rewriting of the shared `main` history.

Each translation change must update the canonical C source and exact physical PRG-byte ledger. Keep `IDENTIFIED`, `DECODED`, `IMPLEMENTED`, `VERIFIED` and `RETIRED` statuses distinct. Do not count offline equivalence as retired 6502. Keep unresolved hardware behavior, untested branches and timing explicitly open.

## Verification expectations

Run repository checks and relevant native/offline tests. Record the exact Japanese ROM identity used, physical ROM bank/offset, instruction bytes, registers and flags, RAM reads/writes, stack, bank interactions, external-call boundaries and return PC. Compare behavior to independent 6502 ROM execution and original-game captures; synthetic tests are not live equivalence.

Live changes to gameplay, camera, UI, audio or widescreen additionally require Windows/MSVC build acceptance, actual in-game validation and independent parity evidence. Preserve the known-working ROM-backed reference and launcher. Do not use diagnostic screenshots as widescreen acceptance.

## Data and history

Do not commit private ROM, ripped CHR/assets, generated asset packs, saves, large capture files or proprietary screenshots. Retain historic source and test files until their dependencies are understood and alternatives pass regression checks. Before pruning a merged branch, verify its PR was merged into `main` and its remote SHA matches the audited manifest.

Document remaining gaps without claiming progress that has not been verified. Commit style: `type(scope): concise change`.
