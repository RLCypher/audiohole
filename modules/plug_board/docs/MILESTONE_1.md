# Milestone 1: isolated Windows feasibility harness

Status: implementation and available software checks completed. Native Windows
routing feasibility is still awaiting hardware evidence. This is not the finished
Plug Board module and is not integration-ready. Assessment approval authorized this
isolated work; the main AudioHole application has not been changed or merged.

## Completed

The standalone CMake module contains a portable observation model and reversible
policy experiment, Windows identity/session/endpoint adapters, an isolated private
policy ABI, a finite Shell icon worker, a WMI process-start probe, durable recovery
evidence and a disposable silent-audio fixture. Public headers and nontrivial code
have `//` contracts covering ownership, thread affinity, failures and limitations.
No EarTrumpet implementation or new major third-party dependency was imported.

Application grouping happens across endpoints, retains all underlying sessions,
deduplicates repeated observations and gives later state updates precedence.
Only Active sessions determine current destinations; simultaneous active endpoints
remain visible. Inactive sessions do not create extra applications. Process tokens
include creation time and verified application evidence; they are not persistent
application IDs. Known generic hosts remain provisional without an application ID.

Before any temporary setter, the experiment snapshots each process/role and flushes
a new recovery file. It checks per-role HRESULTs and readback separately from actual
session destinations, handles shared application/role preferences, and restores
touched values after success, refusal, cancellation or ordinary exceptions.
Observed external changes are preserved and reported. Live process objects are
retained across PID-based native calls. Crash recovery never replays a historical
PID and is restricted to a new verified instance of the same disposable fixture.

## Verification performed

| Check | Actual result and scope |
| --- | --- |
| Linux GCC 14.2 Debug | 42 portable tests passed, including six icon-path regressions added in 0.1.2. |
| AddressSanitizer, UndefinedBehaviorSanitizer, leak detection | The same 42 tests passed with no sanitizer findings. |
| Windows x64 Release cross-build | MinGW-w64 GCC 14-posix built the core, native adapters, desktop launcher, probe, fixture and both test executables. New C++ files passed AudioHole's clang-format check. |
| Supplemental Wine 10 contract execution | 42 tests passed; these use simulated policy adapters and pure path helpers and establish no Windows routing behavior. |
| Supplemental Wine evidence execution | 11 tests passed, including actual embedded-icon export from an extended Unicode path; 1 native process/package-identity test was explicitly skipped because Wine exports an aborting `GetApplicationUserModelId` stub. |
| Unsupported-environment behavior | Probe help ran. An attempted fixture exercise under Wine returned Unsupported (exit 3) before recovery preparation or any preference experiment. |
| Supplemental Wine GUI execution | 44 assertions passed against actual controls and an explicitly labeled sample-data executor, covering selection, active destinations, results, help/names, minimum geometry, appearance, disabled states, wheel suppression, cancellation, failed restoration and deferred closure. First-inspection, dark and light screenshots were reviewed. |
| Ordinary desktop launch under Wine | No-argument launch opened the window, saved an actual read-only inspection beneath local application data and exited normally through the window-close protocol. No recovery record or preference experiment was created. |
| Standalone wxWidgets configuration | The default source-build dependency path configured successfully with the pinned wxWidgets 3.2.6 source. The cross-built launcher used the matching already-built static wx libraries. Native MSVC compilation remains pending. |
| User-provided native Windows inspection | Build 26300.9974 produced a complete snapshot with 45 endpoints, 10 available outputs, 8 applications and 27 sessions. Spotify and Firefox were active on Music (RØDECaster). Six generic icons exposed the path-handling defect corrected in 0.1.2; native retesting of the fix remains pending. See [findings](NATIVE_INSPECTION_8852.md). |
| Isolation | Existing application, engine, scanner, profile, GUI, build and CI files have no tracked changes from this work. No native Windows preference setter was executed in this Linux environment. |

The software tests cover stale/duplicate sessions, multiple active endpoints,
same-name/different-image apps, package IDs, unknown owners, cross-process ambiguity,
PID reuse, activity boundaries/clock anomalies, all processes/roles, default/null
preferences, shared Windows preferences, failed/readback-mismatched setters,
exceptions, cancellation, external edits, lost processes, restoration refusal,
interrupted/idempotent recovery, journal failures, malformed/duplicate/deep JSON,
Unicode paths and preservation of existing files.

Execution logs and GoogleTest XML accompany the handoff under `verification/`.
The initial Wine package-identity crash led to an explicit unsupported-environment
guard; it was not reclassified as successful Windows metadata validation. Native
assertions remain enabled when the tests run on Windows.

## Native evidence still required

Follow [the native procedure](NATIVE_VALIDATION.md) as a standard user on Windows 11
with two available render endpoints. Record exact build/UBR, application icons,
policy acceptance/readback, active destinations before and after fixture stream
recreation, restoration, process-start notification coverage and process cleanup.
An available private interface or a rendering GUI would not satisfy that gate.

Native MSVC compilation, packaged application metadata, corrected Windows icon quality,
private ABI behavior, real playback endpoint reassignment and WMI permissions have
not been verified here. Restart/reboot persistence, real Discord/Steam/game behavior,
exclusive/ASIO paths and numerical CPU/memory measurements remain unverified.

## Remaining implementation and integration

This milestone does not implement Routing Board storage, the approved working/saved
checkpoint model, persistent application UUID matching/relocation, lifetime-wide
icon caching, foreground session notifications, continuous background policy
maintenance, profile selection hooks, the wxWidgets socket columns/drawer or host
integration. The JSON library files beside the architecture assessment remain a
proposal, not an installed parser. The native adapters currently serve the finite
standalone probes; they are not a GUI-owned polling service.

The next gate is the native feasibility run. Review its evidence before committing
to the private adapter or broad GUI work, then continue the approved incremental
model/persistence, discovery, GUI and lifecycle milestones. Final application
integration remains owned by the main AudioHole agent and needs its separate
approval under the original brief.

## Version 0.1.1 desktop launcher

The standalone package now includes `AudioHole-PlugBoard.exe` at its root. A normal
double-click opens an actual Windows GUI, starts one read-only inspection and shows
applications, named playback outputs, active destinations and session details.
Buttons expose support, process-start, reversible silent-fixture and recovery
checks, automatic evidence saving, cancellation and readable results. The existing
CLI and GUI call one shared runner; native operations remain on an owned worker.
Normal closure requests cancellation and keeps the event loop responsive until
cleanup completes. A reported restoration failure keeps the result window open.

The launcher uses the host's existing pinned wxWidgets 3.2.6 version and unchanged
AudioHole icon. The default appearance follows Windows, with Light, Dark and
high-contrast support. Device choices ignore mouse-wheel selection changes.
Developer smoke mode injects labeled sample data and dispatches actual control
events; it never executes the native routing adapter. A user-provided native
read-only inspection has now been reviewed; real routing and restoration evidence
remain pending. The launcher is a usability improvement
for this feasibility milestone, not completion of the mockup's socket-board UI.

## Version 0.1.2 icon correction

The native inspection exposed extended DOS executable paths being passed directly
to Shell. Icon lookup now uses a safe presentation copy and a verified-executable
resource fallback, preserving canonical application identity and all routing
semantics. The comments and regression tests cover normalization-sensitive names,
other namespaces and literal Unicode/percent/comma filenames. Corrected icons on
the user's actual installed applications still need a new native inspection.
