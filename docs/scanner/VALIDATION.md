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

## Queued GUI refinements and analysis-bundle follow-up

This revision adds draggable workspace panes, compact device status cells,
editable scanner metadata, human-facing evidence summaries, decimal-second Deep
controls and conditional console following. Per-device scans now live only in
Devices context menus. It also corrects requested Deep scope in session reports,
includes referenced raw histories (or explicit unavailable reasons), and separates
selected-endpoint Basic completion from unrelated global enumeration warnings.
The scanner's Deep verification criteria and the audio engine are unchanged.

All **136 portable CTest entries pass in both Debug and Release**, including
the imported scanner contracts and new exact-decimal, metadata rename/restart,
stale-editor, conflict, rollback, case-only rename, evidence-summary and scoped
completion checks. The six-scope integration test checks exported history files,
requested Deep scope and ZIP-relative path separators. The Windows x64 MinGW
cross-build compiles and links the application and test targets, including new
native smoke checks for divider movement, pinned actions, decimal inputs and
console scroll anchoring/follow resumption. The runtime installation matches all
822 approved files, and canonical PE icon/avatar resource checks pass.

These are local portable/cross-build results, not an executed MSVC or interactive
Windows GUI pass. Windows Debug/Release CI, extracted-package smoke, mouse/keyboard
context menus, tooltip/assistive-technology behavior, high DPI, local-time/DST
rendering, filename operations on Windows and real hardware scans remain native
validation tasks. See the appended manual checklist. No test is skipped and no
existing test gate is weakened to produce a package.

## Follow-up: divider smoke fixture on compact Windows desktops

The supplied Windows log reports both GUI smoke modes failing the same shared
assertion: `Profile divider cannot be adjusted`. That fixture attempted only a
12-DIP positive movement from the monitor-dependent startup layout. wxSplitterWindow
correctly clamps a sash at pane-size limits; nested Notes/Holes can have no spare
travel on a short desktop. A clamped move alone does not establish a broken divider.

The smoke fixture now makes a 1000-DIP square client area, gives the nested rows
spare space, and requires real movement, trying the opposite direction if one
direction is clamped. It restores the original size and sash positions and checks
the compact layout's field bounds and pinned Save bar again. Failures name the
specific divider and report sash/client/minimum dimensions. Both smoke modes
retain these checks; no test is disabled. Application startup size, pane minimums,
user configuration and scanner behavior are unchanged. The native Windows rerun
is required to confirm the repair. The updated GUI cross-compiles and links for
Windows x64 successfully; native smoke execution was not available locally.

## Active-backlog revision

For the latest four-item GUI/scanner follow-up, all **168 portable CTest entries**
pass in Debug and Release. Tests distinguish positively absent metadata from
active/unknown/permission/identity failures, preserve structured API/state/HRESULT
context, and check inactive Full-scan progress and retained IDs/names/notes.
All Windows x64 application/test targets cross-build and canonical executable
branding checks pass. PowerShell distribution-tool regressions and actual Runtime
packaging pass the exact 729-file allowlist (731 ZIP files with metadata/README),
with all 699 avatar PNG hashes preserved. Native GUI smoke compiles with status
divider and Settings-order checks; native Windows execution and physical-device
assessment remain outstanding. The older counts below describe prior revisions.

The owner-authorized follow-up implements the active GUI backlog plus unlimited
overall Deep time at zero and local recovery for invalid numeric input. See
[GUI_REVISION_NOTES.md](../../GUI_REVISION_NOTES.md) for behavior, persistence and
interruption contracts. The uploaded 20-minute scan motivated removing full JSON
rewrites from API-stage callbacks. A regression with 80 stage events retains all
stages/external edits and bounds AtomicWriter calls to at most eight per attempt;
it does not measure real Windows disk speed or prove a hardware speedup.

All 146 portable CTest entries pass in both Debug and Release, including scanner contracts, typed
command completion, stage retention/write bounds, zero-budget completion and
cancellation, destructive-clear isolation/confirmation/restart, invalid-admission
severity, name/count progress, repeat/continuous/static playback, inactivity sleep
and preference compatibility. A delayed-checkpoint test additionally proves that
a worker is not launched after checkpoint latency exhausts its device budget.
All Windows x64 application/test targets cross-compile and link. The Runtime
installation matches its exact 825-file allowlist, including the byte-preserved
Material font/license/notice; canonical PE icon checks and all 796 avatar PNG checks
pass. Native MSVC Debug/Release GUI smoke,
extracted-package execution, theme/selection/Material rendering at multiple DPIs,
Windows file-operation failure cases and real hardware timing remain required.
No automated test deletes actual user scan data or performs unconsented probing.
