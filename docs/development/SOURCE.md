# Corresponding source

AudioHole code is GPL-3.0-only. The matching source is a separate **Source**
download on the same CI run as each binary artifact. The portable binary ZIP does
not contain source code or a source archive. The separate source archive includes
the checked-out source, build scripts,
license notices and source copies of the pinned wxWidgets, nlohmann/json, miniz,
miniaudio and GoogleTest dependencies. No signing keys or credentials are needed.

Extract the source archive, install the Windows toolchain described in BUILDING.md,
then configure the existing Windows preset. To build without fetching dependencies,
pass CMake cache paths to the source copies under dependencies:

```powershell
cmake --preset windows-msvc `
  -DFETCHCONTENT_SOURCE_DIR_WXWIDGETS="$PWD/dependencies/wxwidgets" `
  -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="$PWD/dependencies/nlohmann_json" `
  -DFETCHCONTENT_SOURCE_DIR_MINIZ="$PWD/dependencies/miniz" `
  -DFETCHCONTENT_SOURCE_DIR_MINIAUDIO="$PWD/dependencies/miniaudio" `
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/dependencies/googletest"
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
```

An archive also contains source-build-info.json identifying its commit and dirty
status. Uncommitted source packages use the actual working tree rather than claiming
to be a clean commit. Official CI requires a clean checkout. Rebuilding the application from this source distribution does not require Git history.

To build the optional installer install Inno Setup 7.1.1, create the runtime
ZIP with scripts/Package.ps1, then use scripts/Build-Installer.ps1. The installer
script, release notes and JSON reader are included in the source archive.
