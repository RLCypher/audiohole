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


## Follow-up: Windows scanner contract fixtures

The subsequently supplied Windows Debug CTest log reports **135/136 CTest entries
passed**, including both GUI smoke tests and all eight scanner/controller integration
tests. `Scanner.Contracts` ran 80 internal checks, of which four failed. Thus the
GUI compiled; the CTest gate prevented packaging.

The append-only log fixture wrote in Windows text mode (CRLF) but asserted an LF
byte prefix. It now writes explicit binary LF and CRLF fixtures and checks exact
preservation plus actual appended bytes. The three automated-suite failures were
reproduced on the development host by enforcing a 260-character file-path limit
under a runner-sized temporary root: nested case/report/device paths and atomic
write suffixes exceeded that limit before the intended checks could run. Windows
contract fixtures now use absolute extended paths, including UNC handling. A
Windows-only regression performs real Unicode writes, atomic replacement and
backup reads beyond 280 path characters. Suite failures also include their manifest
rather than only a final result assertion.

After this fixture fix, all 78 portable scanner checks pass in Debug and Release,
and the updated Windows contract executable cross-compiles/links. The new
Windows-only regression and the full Windows CI rerun remain pending. No scanner
runtime, GUI behavior, test skip, success criterion, routing logic or queued taskbar
menu feature was changed by this repair. Native package validation remains gated
on successful CTest; it has not been claimed as passed.

## Follow-up: native separators and taskbar controls

The next supplied Windows Debug log again reports 135/136 CTest entries passed,
including both GUI smoke tests. The six-scope suite and new deep Unicode/atomic
write regression pass, confirming that extended temporary paths work. Remaining
failures show fixtures joining forward-slash literals onto those Windows paths;
extended paths do not normalize `/` into `\`. Eleven fixture path expressions now
join individual components instead, and the initial absolute root is made native
before adding its extended prefix. Fixture reads/writes now report an explicit
file error instead of turning failed opens into misleading empty-JSON errors.
No contract assertion or test gate is removed.

After the separator repair, all 127 portable Debug CTest entries pass and the
Release scanner contracts pass. The Windows cross-build includes the corrected
scanner executable and the owner-authorized taskbar controls. Native smoke now
checks the shared menu's Close/Exit and capability gating, native system-menu
entries and malformed shell-message rejection, without publishing Jump Lists in
CI. Native CI and interactive taskbar behavior still need a new Windows run.
The last owner-confirmed successful baseline remains e78eb64 (#88/#89).
