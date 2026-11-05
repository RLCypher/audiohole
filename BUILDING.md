# Building the AudioHole GUI evaluation

Milestone 0.2 provides a native wxWidgets GUI backed by a deterministic, in-memory
simulated controller. It never enumerates real endpoints, captures/renders audio,
or writes production configuration. D-030, D-031, D-034, and all six owner decisions
remain Open. See [MANUAL_GUI_CHECKLIST.md](MANUAL_GUI_CHECKLIST.md) for evaluation.

## Windows prerequisites

- Windows 10 22H2 or a supported Windows 11 release for the initial validation baseline.
- Visual Studio 2022 or Build Tools 2022, with **Desktop development with C++**,
  the MSVC v143 x64/x86 tools, and a Windows 10/11 SDK.
- CMake 3.25 or newer, including CTest, available on `PATH`.
- Git for Windows on `PATH` (also used to identify packaged source revisions).
- Windows PowerShell 5.1 or PowerShell 7 for the helper scripts.
- HTTPS access to `github.com`, release download destinations, and `codeload.github.com` for pinned dependencies.

The preset selects the Visual Studio 2022 generator and x64 target, so ordinary
PowerShell is sufficient when the installed compiler/SDK can be found by CMake.
No Ninja installation is needed for this generator. A compatible MSVC toolchain
can use a local `CMakeUserPresets.json` or explicit generator/toolset options;
the checked-in reference commands use Visual Studio 2022. ARM64, 32-bit targets,
and native Linux builds are not supported by this foundation.

Check prerequisites without installing software or changing system settings:

```powershell
.\scripts\Verify-Environment.ps1
```

The script checks tool versions, the VS 2022 x64 compiler installation, and
Windows SDK headers/libraries. It prints actionable missing-component guidance
and exits nonzero on failure. Install missing tools yourself using their official
installers; the scripts do not elevate, modify PATH/registry settings, or bypass
execution policy. If local policy blocks scripts, use the direct commands below
or obtain an appropriate policy from your administrator.

## Configure, build, test, and launch

Run from the repository root in PowerShell:

The shortest build/test commands are:

```powershell
.\scripts\Build.ps1 -Configuration Debug
.\scripts\Build.ps1 -Configuration Release -Launch -Package
```

These verify prerequisites, configure the existing preset, build, and run CTest
with exit-code checks. `-Launch` opens the native simulated GUI;
`-Package` stages only the CMake Runtime component and creates a revision-named
ZIP in `build/packages`. Release is the preferred configuration for manual
evaluation. Without those switches no package or additional launch is requested.
Equivalent direct commands follow:

```powershell
cmake --version
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
& .\build\windows-msvc\bin\Debug\audiohole.exe
if ($LASTEXITCODE -ne 0) { throw "Debug GUI failed" }

cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
& .\build\windows-msvc\bin\Release\audiohole.exe
if ($LASTEXITCODE -ne 0) { throw "Release GUI failed" }
```

Stop if any configure/build/test command fails. `audiohole.exe` opens the GUI;
close hides to tray, while **Exit AudioHole** stops and terminates the process.
`audiohole_dev.exe` remains a non-distributed COM developer harness.

CTest covers real Windows COM lifetime, portable simulator/presentation logic,
and native GUI smoke interactions. GUI smoke uses internal developer arguments
`--smoke-test --screenshot-dir <path>` and exits with evidence. These are not a
public product CLI. Tests require no administrator privileges or audio devices.
Both presets fail if no tests are discovered.

For a clean repeat, remove only the generated preset directory and rerun the
commands above (check for local work before deleting anything). For example:

```powershell
Remove-Item -Recurse -Force .\build\windows-msvc
cmake --preset windows-msvc
```

Rebuilding after a normal edit needs only the relevant build and test commands.
CTest does not build targets. `.github/workflows/windows.yml` runs these presets
on Windows Server 2022 with Visual Studio 2022 in separate Debug/Release jobs;
it checks builds, COM, simulator logic, GUI smoke, and packaging, not consumer audio hardware compatibility.
Successful jobs then package and audit runtime imports, extract and launch the
package in a temporary directory outside the source/build tree, and upload its
contents as a GitHub Actions ZIP artifact. Upload is skipped on build, test,
packaging, or smoke-test failure. Adding these steps is not evidence of a run.

## Download and run a CI build

1. Open the private repository's **Actions** tab and select a successful
   **Windows builds and evaluation packages** run for the intended revision.
2. Download **AudioHole-Windows-x64-release-&lt;full-tested-commit&gt;** under Artifacts.
3. Extract the downloaded ZIP and open its single AudioHole package directory.
4. Read `README.txt` and `build-info.json`, then double-click `audiohole.exe`.
   Follow the included manual checklist; every device and operation is simulated.

Debug is uploaded separately for development investigations. Both packages are
x64, use the static MSVC CRT, and require normal Windows system DLLs but no
Visual Studio, SDK, CMake, Git, or GoogleTest on the destination. No installer,
administrative rights, service, or audio driver is required. CI additionally
rejects unexpected MSVC runtime DLL imports, since a VS-equipped runner alone
could otherwise conceal a missing dependency. Manual consumer-PC checks remain
valuable; the runner is Windows Server 2022, not every supported consumer PC.

Artifacts are private-repository evaluation builds, retained for 14 days, not
formal Releases or externally published packages. The GitHub artifact name uses
the full tested SHA; the inner folder/local ZIP uses its first 12 characters.
On a pull-request run this can be GitHub's synthetic merge commit, not the tip
of `work`; metadata records the actual checkout SHA and Actions run URL. Reruns
may create separate run artifacts. Local dirty builds are marked `-dirty` and
identified as such in metadata rather than claimed to match a clean commit.

For a local package after building/testing, or to audit it on Windows:

```powershell
.\scripts\Package.ps1 -Configuration Release
.\scripts\Test-Package.ps1 -Archive C:\path\to\AudioHole-Windows-x64-Release-commit.zip
```

Packaging refuses to overwrite an existing ZIP: move it aside or choose another
`-OutputDirectory`. Only `audiohole.exe`, the manual GUI checklist, wxWidgets and nlohmann/json license
texts, `personalities/default.json`, `personalities/uwu.json`, the personality format
guide, `README.txt`, and `build-info.json` are included. Tests, source trees, symbols, SDKs, and GoogleTest are excluded.
The native smoke-test script requires clean metadata and a VS installation for
the import audit; end users need neither to launch the executable. GitHub creates
the downloadable ZIP from the already-tested package contents, avoiding a
second ZIP to extract.

`tests/scripts/Test-DistributionTools.ps1` exercises packaging metadata, archive
contents, overwrite protection, unexpected-runtime rejection, and cleanup using
inert temporary fixtures. CI runs it in addition to the application/COM tests
and actual extracted-package launch; fixture tests do not establish Windows
application behavior.

## Dependencies and reproducibility

The test-only third-party source dependency is **GoogleTest v1.16.0**, BSD-3-Clause,
used by tests only:

- Tag commit: `6910c9d9165801d8827d628cb72eb7ea9dd538c5`.
- Archive SHA-256: `78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399`.
- Source: <https://github.com/google/googletest/tree/v1.16.0>.
- Notice: [third_party/licenses/googletest-LICENSE.txt](third_party/licenses/googletest-LICENSE.txt).

CMake FetchContent verifies the pinned archive hash and extracts sources under
the generated build directory. TLS and hash verification must remain enabled.
GoogleMock, dependency installation targets, and shared GoogleTest libraries are
not enabled. The application does not link GoogleTest.

For an offline build, configure once with network access, or provide an unpacked
copy of that exact verified source using CMake's
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=C:/path/to/googletest-1.16.0` override.
When using a local source override, verify the archive hash before extraction;
FetchContent does not verify overridden source trees. A fresh uncached build
with tests enabled otherwise needs the download. `-DBUILD_TESTING=OFF` omits
GoogleTest but builds the GUI and developer harness; it does not validate tests.

C++20 is required with compiler extensions disabled. The reference build uses
MSVC's static CRT (`/MTd` in Debug, `/MT` in Release) consistently across project
and test targets, avoiding a separate VC runtime DLL deployment requirement for
the packaged GUI. Normal Windows system libraries, including `ole32`, remain
required. The MSVC executable manifest explicitly requests `asInvoker`; the
application installs no service, driver, or startup registration.

These commands pin dependency sources and build configuration, not the entire
host installation or byte-identical output. Record CMake, MSVC, Windows SDK,
OS, and configuration with each validation run. No logging, DSP, or audio backend dependency is integrated. nlohmann/json is used only for approved personality-file validation. AudioHole's own
project license remains an owner decision before distribution.

## Source boundaries

- `src/platform/windows`: `audiohole_windows`, COM initialization only.
- `src/app`: `audiohole_dev`, temporary developer executable.
- `tests`: `audiohole_tests`, linked to the platform library and GoogleTest.

`ComApartment` is non-copyable/non-movable and must be constructed and destroyed
on the same thread, after that scope's COM objects have been released. It
balances both `S_OK` and `S_FALSE` with `CoUninitialize`; failed initialization
owns no count. Future WASAPI worker threads must initialize their own apartments.
- `src/model`: GUI-independent controller interface, typed session model, deterministic fixtures.
- `src/presentation`: authored technical/commentary catalog and reproducible selection.
- `src/gui`: native views, tray, and process lifecycle. wx owns widgets; Frame owns the controller.

wxWidgets **3.2.6** is integrated provisionally for GUI evaluation (D-031 remains Open),
statically linked with its wxWindows Library Licence 3.1 exception. Source archive
SHA-256: `939e5b77ddc5b6092d1d7d29491fe67010a2433cf9b9c0d841ee4d04acb9dce7`.
The license and referenced LGPL text are retained under `third_party/licenses/`
and included in runtime packages. Image codecs, XML, regex, network/web/media,
and optional rich editor libraries are disabled; no extra bundled libraries are needed.
For offline source, verify that archive before extracting and set
`-DFETCHCONTENT_SOURCE_DIR_WXWIDGETS=C:/path/to/wxWidgets-3.2.6`.

Portable model-only developer checks (not a Linux product/GUI) can run with:

```sh
cmake -S . -B build/model -DAUDIOHOLE_MODEL_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/model --parallel 2
ctest --test-dir build/model --output-on-failure
```

## Validation status

Native Windows/MSVC validation must be recorded separately from Linux checks.
A Windows-target cross-build does not execute Windows COM, confirm the MSVC
manifest/CRT options, or establish successful Windows launch/test behavior.
See the current Milestone 0 entry in [ROADMAP.md](ROADMAP.md) for actual results
and outstanding validation. Milestone 1 routing remains outside this milestone.

Implementation changes must also pass the human readability and contract review in [ENGINEERING.md](ENGINEERING.md); build and test success alone does not establish milestone acceptance.

## Approved presentation testing iteration

Norm/UwU are staged beside the built executable in `personalities/` on every
build and included in runtime packages. Vanilla is compiled in and remains usable
if that directory is removed. See [personalities/README.md](personalities/README.md)
for editing/custom catalogs, validation limits, and explicit reload behavior.
The JSON parser is nlohmann/json 3.11.3 (MIT), pinned in `cmake/Json.cmake`; its
notice is shipped with the GUI. An offline build may set
`-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=C:/path/to/json-3.11.3` after verifying the
archive SHA-256 recorded in DEPENDENCIES.md. As with other overrides, CMake does not
verify an already unpacked tree. No DLL or interpreter is added by this parser.

Follow System is the default. Settings also offers Light and Dark;
Windows high contrast overrides the palette. Windows-owned dialogs and menus keep
system styling. This evaluation retains session-only presentation preferences.
Native smoke tests cover file availability, DFA slider inertness/visibility, scan
callouts, theme changes preserving controller revision, and selected navigation.
Screenshots and the manual checklist are still required for visual acceptance.


## Test-mode scanner integration

The runtime now also installs the private `audiohole-probe.exe` helper and the
miniz 3.1.0 license. Keep the helper beside `audiohole.exe`; launch the GUI normally,
then select Test Mode for real Windows scans. The helper has no public scanning
command. The strict runtime allowlist and import audit cover both executables.
`Scanner.Contracts` is test-only, as are the injected scanner integration tests;
their data never becomes runtime inventory.

The scanner reuses nlohmann/json 3.11.3. ZIP export adds pinned miniz C sources (see
DEPENDENCIES.md). Offline builds may use
`-DFETCHCONTENT_SOURCE_DIR_MINIZ=C:/path/to/miniz-3.1.0` after verifying its recorded
archive hash. Model-only checks also build scanner contracts and background-service
integration tests, but do not execute Windows hardware APIs. Read
[SCANNER_TEST_MODE.md](SCANNER_TEST_MODE.md) before interpreting Deep evidence.
