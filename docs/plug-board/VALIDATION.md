# Integration validation — November 17, 2026

Starting host revision: `6139f45038abaf4b6de0e989a0924d356c91e6cd` on `work`.
The remote `work` head was checked before publication. Imported module version:
0.2.6, from the owner-supplied archive recorded in `handover-files.json`.

## Completed checks

| Check | Result |
|---|---|
| Linux portable CMake build and CTest | 289/289 passed |
| Plug Board portable contracts | 44 passed |
| Plug Board portable library/service tests | 70 passed, including tutorial file isolation and System-role refresh |
| Tutorial catalog regeneration/integrity | 89 authored entries, 12 MSVC-safe phases |
| Supplied personality/artwork integrity | 80 verbatim responses; six packaged icon hashes passed |
| Windows x64 MinGW build | Passed, including embedded wx view and native backend |
| Windows model-only configuration | Passed without wx or Plug Board native targets |
| Windows model tests under Wine | 266 passed with the pre-existing Wine filesystem exclusions |
| Windows Plug Board contracts/library under Wine | 44 + 69 passed; platform-conditional test counts differ |
| Windows Plug Board native evidence under Wine | 23 passed; one original metadata test skipped for Wine's aborting `GetApplicationUserModelId` stub |
| Gui.Smoke under Wine | Passed: embedded create/rename/endpoint/app controls, personality event pools, unchanged-refresh silence, Light/Dark theme, tray/system-menu shortcut and hidden discovery |
| Gui.TestModeSmoke under Wine | Passed, including empty-board/device/application isolation |
| Gui.TutorialSmoke under Wine | Passed: existing course, two additional sockets, arbitrary names, real endpoint controls, two fake app moves, fictional playback observations and existing Settings continuation |
| Gui.EngineSmoke under Wine | Passed: software-only engine/Apply regression checks; no hardware opened |
| Gui.RestartSmoke under Wine | Parent and child passed; shutdown future and restart remain compatible |
| Runtime component installation | Exactly 750 allowlisted files; no missing/unexpected files, standalone launcher, fixture binaries or compiler-runtime DLLs |
| Native import inspection | Only Windows system libraries imported |
| Whitespace and affected C++ formatting | Passed |

All GUI modes run an in-memory simulated Plug Board backend. The tutorial
additionally verifies both fake app assignments through actual view context
menus, verifies their fictional observed outputs, and loads all six supplied
application icons. Desktop screenshots of the dark embedded board and light
tutorial board were reviewed. Actual Windows policy mutation is not inferred
from these supplemental Wine runs.

The Windows model command excludes `ConfigurationBundle.*` and
`PersonalityFiles.MissingOptionalFeatureAndCapabilitiesNeverBorrowFromOtherPackages`
for previously documented Wine filesystem behavior. These remain enabled in
native Windows CI. No CI exclusions were added for this integration.

## Integration-specific corrections found during validation

- Updated navigation expectations and scan assertions for five tabs and nine
  tutorial endpoints, preserving the original substantive GUI checks.
- Tutorial construction must not request the host's persistent configuration
  root. It uses memory-only checked storage and no local state/cache roots.
- The original shortcut-icon test read `AudioHole-PlugBoard.exe`. The embedded
  build now explicitly supplies `AudioHole.exe` as its read-only resource
  fixture and builds the host first. It still tests shortcut precedence and
  ambiguity using real isolated `.lnk` files, without launching the host.
- The tutorial projects successful fake assignments into its own fictional
  session observations. Production observations and the native policy reducer
  are unchanged.

## Remaining native validation

MSVC Debug/Release, native Windows metadata/policy behavior and the exact ZIP
launch checks run in GitHub Actions. Existing native-module hardware evidence
is retained in the original handover; this integration does not claim a new
hardware routing measurement or a new Windows-version support guarantee.
Per the owner's latest instruction, publication does not wait for GitHub's
compilation to finish.

The earlier Automatic channel compatibility/labeling request remains a separate
pending task. The audio engine, profile/device JSON schemas, unrelated layout
and avatar playback behavior were not changed by this integration.
