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
Per the owner's reinstated monitoring instruction, check native CI 20 minutes
after publication, then every five minutes while it remains unfinished. Diagnose
and correct failures, republish, and repeat until Debug and Release pass.
Use a quiet timer; stop waiting and report a blocker if API access or another
problem prevents checking.

The earlier Automatic channel compatibility/labeling request remains a separate
pending task. The audio engine, profile/device JSON schemas, unrelated layout
and avatar playback behavior were not changed by this integration.

## MSVC property-key include correction

The first MSVC Debug build of `a2f56e6` failed in Windows SDK 10.0.26100.0's
`functiondiscoverykeys_devpkey.h` while compiling `environment.cpp`. Its
`DEFINE_PROPERTYKEY` prerequisite was missing, producing the C2440/C4430/C2374
error cascade. MinGW's copy of that header includes the prerequisite itself,
which concealed the problem in the original cross-build.

Both `environment.cpp` and the GUID-definition translation unit `guids.cpp`
now include `propsys.h` and `propkeydef.h` explicitly before the device-key
catalog. The
ordered include blocks are protected from automatic sorting; `initguid.h`
continues to precede GUID/property-key definitions in `guids.cpp`.

For local reproduction, Microsoft's actual `functiondiscoverykeys_devpkey.h`
was substituted through an isolated include directory, without changing the
installed toolchain or repository dependencies. The original two source files
both failed at `DEFINE_PROPERTYKEY`; both compiled after the correction.
`environment.cpp`, `guids.cpp`, `window_icon.cpp` and `shortcut_icon.cpp` also
passed syntax checks with Microsoft's `propkeydef.h` and `propkey.h` in that
directory. These headers came from Microsoft's public
`win32metadata/generation/WinSDK/RecompiledIdlHeaders/um` snapshot; the device-key
header SHA-256 was
`bf53c0183cfac0ae543fcc97fd06165b7a59aac345e54320d1e38d3c27dcebcd`.

The full MinGW Windows build, including AudioHole and the native evidence
executable, linked successfully. The native adapter suite passed 23 tests under
Wine, retaining its existing process-metadata skip. This supplemental
reproduction does not replace the MSVC Debug/Release rebuild in GitHub Actions.

## MSVC wxString conditional correction

The Debug build of `ae1a308` passed the native backend and failed with C2445 in
`settings_drawer.cpp`: the recent-activity label combined `"Last 24 hours"`
with `wxString::Format` results in a conditional expression. MSVC found both
pointer and `wxString` conversions eligible. That literal now constructs a
`wxString` explicitly, giving every branch the same type.

The same review found and corrected mixed literal/`wxString` conditionals in
the application-row accessibility name and help text. All displayed text and
control behavior are preserved. The remaining conditional expressions in the
embedded view were reviewed for this conversion pattern.

The Windows x64 cross-build passed after these corrections. Preview,
Test Mode and Tutorial GUI smoke passed under Wine, including the actual
Plug Board drawer/row controls and complete tutorial. Formatting and whitespace
checks passed. MSVC Debug/Release confirmation remains with native CI.

## MSVC Core Audio GUID linkage correction

The next MSVC build of `85147e2` compiled the embedded module but failed to link
AudioHole with eight unresolved Core Audio GUID variables. Microsoft's MIDL
headers declare these variables with `EXTERN_C`; including `initguid.h` does
not define them. MinGW's headers use `DEFINE_GUID` instead, which concealed
the missing definitions in the cross-build.

The backend, environment inspector, destination validator and session monitor
now obtain the same GUID values from `__uuidof` on the SDK interfaces and
`MMDeviceEnumerator` coclass. This covers COM activation, interface queries
and callback `QueryInterface` comparisons. The optional silent developer
fixture uses the same approach. No GUID values are copied into application
source, and the GUID-definition unit retains its ordered includes for the
remaining `DEFINE_GUID` and property-key constants.

The full Windows cross-build linked AudioHole and the native evidence
executable successfully. An undefined-symbol inspection of the compiled
`plug_board_windows` archive confirmed that all eight external GUID references
were eliminated. The native adapter suite passed 23 tests under Wine, retaining
its original process-metadata skip. `Gui.Smoke` also passed using the newly
linked application. MSVC Debug/Release confirmation remains with native CI and
the monitoring schedule above.

## Native CTest timing-budget correction

Both MSVC configurations of `493f4c5` built and passed all five GUI smoke tests.
The [native CI run](https://github.com/RLCypher/audiohole/actions/runs/44244248832)
then passed 296 of 297 non-GUI checks in each configuration, with only
`PlugBoard.WindowsEvidence` timing out at 30 seconds.

Its `SessionMetadataChurnCannotExtendTheFiniteStartupRefreshWindow` test
deliberately observes the production 30-second startup deadline, waits until
31 seconds, then checks that metadata changes cannot restart recurring icon
refreshes. The host presets' default 30-second timeout killed this legitimate
wait. Earlier direct executable runs passed but bypassed that CTest limit.

The suite now declares a bounded 90-second CTest timeout, covering the real
deadline, remaining assertions and runner overhead. The host preset default
and the production refresh schedule remain unchanged. Validation uses CTest
with `--timeout 30` to exercise the same default/override behavior as native CI.

The original registration reproduced the same failure under Wine at 30.44
seconds, while executing that startup-window test. After reconfiguration,
verbose CTest reported a computed timeout of 90 seconds and the complete suite
passed in 36.57 seconds: 23 tests passed, retaining the original Wine metadata
skip. The Windows cross-build and whitespace checks also passed. Native
Debug/Release completion is checked after publication using the schedule above.
