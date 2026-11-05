# Test-mode integration validation — 2026-11-05

Validated on the Linux development host; **not a native Windows or hardware pass**.

| Check | Result |
|---|---|
| Debug portable CTest | 127/127 passed |
| Release portable CTest | 127/127 passed |
| Scanner contract executable (included above) | 78/78 portable checks passed; Windows-only child protocol checks await native execution |
| Scanner/controller integration (included above) | 8/8 passed, using explicitly labelled injected evidence |
| Windows x64 MinGW cross-build | GUI, private probe helper and test targets compiled/linked |
| Cross-build PE branding inspection | Canonical embedded PNG and all seven icon payloads preserved; first shell group is AudioHole |
| Runtime-component install | Exact 822-file allowlist, including helper and miniz notice; no test executables |
| Cross-build helper import inspection | System DLLs only; no external compiler runtime DLL |
| Native MSVC Debug/Release, PowerShell package checks, GUI smoke | Pending Windows CI |
| Actual device discovery/Deep evidence, visual/layout acceptance | Pending Windows in-situ testing |

The integration checks cover all scope/depth combinations, known-device Update
metadata refresh without repeated capability queries, Deep Update restricted to
new targets, exact Selected targeting/no substitution, metadata-only refresh,
partial enumeration without false absence, consent validation, busy rejection,
cancellation cleanup acknowledgement, read-only inventory, filename authority,
unknown fields/notes, per-record hidden flags, external malformed-file retention
with actionable file paths, restart persistence, separation from the provisional
profile file, correlated completions and non-overwriting archive export.

Cross-build install/import/branding inspection is useful evidence but cannot prove
MSVC static CRT settings, native Windows launch, Windows shell caching behavior,
Job Object behavior, WASAPI support or driver reliability. The native CI workflow
retains its Debug/Release and extracted-package validation gates. See
[SCANNER_TEST_MODE.md](../../SCANNER_TEST_MODE.md) and the manual checklist for the
remaining hardware/visual checks and the inherited Deep evidence limitation.
