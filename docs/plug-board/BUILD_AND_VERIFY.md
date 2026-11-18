# Build and verify the handed-over source

The approved runtime is the supplied Windows x64 Release executable. It uses
static wx/compiler runtimes and requires only the Windows system DLLs recorded
in `Windows/RUNTIME-IMPORTS.json`. Source compilation requires CMake 3.25+, C++20
and a suitable 64-bit compiler; Windows builds also need SDK/RC tools. Libraries
and SDK/toolchains are not vendored in this handover.

`Build/dependency-pins.json` records exact CMake download URLs and SHA-256 pins.
Matching local sources can be supplied with `FETCHCONTENT_SOURCE_DIR_*`; otherwise
CMake fetches them. Dependencies: wxWidgets 3.2.6, JSON 3.11.3, test-only
GoogleTest 1.16.0. Material Symbols are bundled native vector data with checked-in
original SVGs and Apache-2.0 notices. EarTrumpet code is not a dependency.

## Portable build (Linux)

From the handover root:

```sh
bash Build/build-portable.sh /absolute/path/to/clean-build
```

The helper configures Debug with GUI/developer tools off and tests on, builds
with two parallel jobs, then runs CTest. `PLUG_BOARD_CMAKE`, `PLUG_BOARD_CTEST`
and `PLUG_BOARD_BUILD_JOBS` can select tools/job count. Additional arguments after
the build directory are CMake options, for example:

```sh
bash Build/build-portable.sh /absolute/path/to/clean-build \
  -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=/path/to/json-3.11.3 \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest-1.16.0
```

A successful current build runs 112 tests across the two portable CTest suites.
The handover helper is checked against a fresh build of the packaged source;
logs are under `Evidence/`. No Windows/Shell success is inferred from these tests.

## Windows native build

Run from a Visual Studio 2022 x64 developer environment with CMake available:

```powershell
cmake -S Source/modules/plug_board -B _build/native -G "Visual Studio 17 2022" -A x64 `
  -DPLUG_BOARD_BUILD_TESTS=ON -DPLUG_BOARD_BUILD_GUI=ON `
  -DPLUG_BOARD_BUILD_DEVELOPER_TOOLS=OFF
cmake --build _build/native --config Release --parallel 2
ctest --test-dir _build/native -C Release --output-on-failure
cmake --install _build/native --config Release --prefix _build/native-package
```

The supplied executable was built with MinGW-w64 GCC 14-posix and matching static
wx 3.2.6, not MSVC. The commands above describe the source-supported MSVC path;
that generator/toolchain has not been executed for this handover. For MinGW/Ninja,
use a native x64 compiler environment, its matching generator and
`-DCMAKE_BUILD_TYPE=Release`, omitting the Visual Studio architecture arguments.
Do not combine incompatible prebuilt wx/compiler/runtime configurations.

Installation contains the one application executable and documentation/licenses;
it does not install test/probe/fixture executables. The CTest build has four
current suites: Contracts, Library, BoardSmoke and WindowsEvidence. Native
Windows normally runs the process-metadata case skipped under Wine. The POSIX
symlink test is intentionally not a Windows case.

For an ASan/UBSan portable build, use a supporting compiler and
`-fsanitize=address,undefined -fno-omit-frame-pointer` in compile/link flags. Run
with leak/error detection enabled; the release evidence includes the 112-test
sanitized run. No sanitizer result for Windows COM/Shell code is claimed.

## Reusable view integration

Use [INTEGRATION.md](INTEGRATION.md). The host owns its frame/tray, scanner,
profiles and actual visibility. Compile the source libraries, not the standalone
executable. The wx view is enabled by `PLUG_BOARD_BUILD_GUI`; using
`add_subdirectory(... EXCLUDE_FROM_ALL)` lets the host build only its dependencies.

## Package integrity

`ORIGINAL-RELEASES.json` preserves the original release ZIP hashes. The original
Windows/source manifests still validate their respective unchanged directories.
`MANIFEST.json` and `SHA256SUMS.txt` cover the handover payload; the SHA file covers
all payload files including the manifest, excluding itself. The manifest lists
all files except itself and the SHA file. An adjacent archive checksum outside
the ZIP verifies the final ZIP as a whole.
