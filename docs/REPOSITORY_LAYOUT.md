# Repository organization

Current directories are lightweight placeholders until real, verified sources are imported.

```text
.github/workflows/      CI quality gates
docs/                   Architecture, authority, verification, status
src/                    Native engine and ROM behavior adapter (future code)
tests/                  Executable regression / parity suites (future code)
tools/                  Deterministic ROM compilers and developer checks
data/                   Schemas, non-proprietary metadata only
```

As code is imported, organize `src/` by subsystem (`assets/`, `world/`, `camera/`, `gameplay/`, `renderer/`, `platform/`) rather than historical phase names. Put private ROMs, extracted artwork, frame recordings and generated databases in ignored local directories.

The `main` branch should always be a reproducible, explicitly labeled state. Work occurs on topic branches through pull requests with clear acceptance criteria; failed experiments stay isolated. CI tests are necessary but live Windows visual parity remains a separate release gate.
