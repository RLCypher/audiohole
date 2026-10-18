# Building the Milestone 0 development foundation

This milestone provides a Windows x64 console development harness and a small
thread-bound COM initialization library. It does not capture, render, enumerate
audio endpoints, or implement a product CLI or GUI. D-030, D-034, and all six
owner decisions remain Open.

## Windows prerequisites

- Windows 10 22H2 or a supported Windows 11 release for the initial validation baseline.
- Visual Studio 2022 or Build Tools 2022, with **Desktop development with C++**,
  the MSVC v143 x64/x86 tools, and a Windows 10/11 SDK.
- CMake 3.25 or newer, including CTest, available on `PATH`.
- HTTPS access to `codeload.github.com` for the first test configuration.

The preset selects the Visual Studio 2022 generator and x64 target, so ordinary
PowerShell is sufficient when the installed compiler/SDK can be found by CMake.
No Ninja installation is needed for this generator. A compatible MSVC toolchain
can use a local `CMakeUserPresets.json` or explicit generator/toolset options;
the checked-in reference commands use Visual Studio 2022. ARM64, 32-bit targets,
and native Linux builds are not supported by this foundation.

## Configure, build, test, and launch

Run from the repository root in PowerShell:

```powershell
cmake --version
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
& .\build\windows-msvc\bin\Debug\audiohole_dev.exe
if ($LASTEXITCODE -ne 0) { throw "Debug harness failed" }

cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
& .\build\windows-msvc\bin\Release\audiohole_dev.exe
if ($LASTEXITCODE -ne 0) { throw "Release harness failed" }
```

Stop if any configure/build/test command fails. Successful harness execution
prints `AudioHole development foundation: COM initialized. Audio routing is not
implemented.` and exits with code 0. COM failure prints the original hexadecimal
HRESULT to standard error and exits with code 1. It opens no audio streams and
does not require an audio device.

CTest discovers four GoogleTest cases covering MTA initialization and release,
nested `S_FALSE` balancing, apartment-mode conflict without uninitializing the
existing apartment, and cleanup during exception unwinding. Each uses a fresh
thread and real Windows COM calls. A fifth CTest test launches the harness and
checks its exit status. Both presets fail if no tests are discovered.
Tests require no administrator privileges or device access.

For a clean repeat, remove only the generated preset directory and rerun the
commands above (check for local work before deleting anything). For example:

```powershell
Remove-Item -Recurse -Force .\build\windows-msvc
cmake --preset windows-msvc
```

Rebuilding after a normal edit needs only the relevant build and test commands.
CTest does not build targets. `.github/workflows/windows.yml` runs these presets
on Windows Server 2022 with Visual Studio 2022 in separate Debug/Release jobs;
it checks build and COM behavior, not consumer audio hardware compatibility.
Adding the workflow is not evidence that a remote run has completed.

## Dependencies and reproducibility

The only third-party source dependency is **GoogleTest v1.16.0**, BSD-3-Clause,
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
GoogleTest and builds only the developer harness; it does not validate tests.

C++20 is required with compiler extensions disabled. The reference build uses
MSVC's static CRT (`/MTd` in Debug, `/MT` in Release) consistently across project
and test targets, avoiding a separate VC runtime DLL deployment requirement for
this harness. Normal Windows system libraries, including `ole32`, remain
required. The MSVC executable manifest explicitly requests `asInvoker`; the
application installs no service, driver, or startup registration.

These commands pin dependency sources and build configuration, not the entire
host installation or byte-identical output. Record CMake, MSVC, Windows SDK,
OS, and configuration with each validation run. No JSON, logging, GUI, or audio
dependency is added merely because it is a future candidate. AudioHole's own
project license remains an owner decision before distribution.

## Source boundaries

- `src/platform/windows`: `audiohole_windows`, COM initialization only.
- `src/app`: `audiohole_dev`, temporary developer executable.
- `tests`: `audiohole_tests`, linked to the platform library and GoogleTest.

`ComApartment` is non-copyable/non-movable and must be constructed and destroyed
on the same thread, after that scope's COM objects have been released. It
balances both `S_OK` and `S_FALSE` with `CoUninitialize`; failed initialization
owns no count. Future WASAPI worker threads must initialize their own apartments.
No controller, audio engine, or GUI placeholder classes are introduced. Later
targets can depend on this platform library without importing application or
presentation code.

## Validation status

Native Windows/MSVC validation must be recorded separately from Linux checks.
A Windows-target cross-build does not execute Windows COM, confirm the MSVC
manifest/CRT options, or establish successful Windows launch/test behavior.
See the current Milestone 0 entry in [ROADMAP.md](ROADMAP.md) for actual results
and outstanding validation. Milestone 1 routing remains outside this milestone.
