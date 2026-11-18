# AudioHole Plug Board — standalone module 0.2.6

Extract the Windows ZIP and double-click **AudioHole-PlugBoard.exe**. Exit any
older tray instance before starting this build. No terminal, installation or
administrator account is needed.

This update uses compact **Last seen** status labels: `2m ago`, `2h 41m ago`,
whole days from 24 hours, whole weeks from seven days, and `a while ago` beyond
four weeks. Last seen text remains complete. Playback device names stay on one
line and may use ellipses; hover over that status for the full actual output
name. Hovering the app name retains **Drag to assign to a new socket**.

**Probe Tools**, the Diagnostics screen/tray entry, and application session/debug
menus have been removed. The package contains no probe, fixture or test
executables. Settings no longer shows the working-edit and drag-instruction
paragraphs. Its bottom **Windows Mixer** button opens Windows' volume mixer.
Internal bounded error logs remain in the local data folder for troubleshooting;
nothing is uploaded automatically.

## Using the board

- System (Default) is always first and cannot be deleted. Add your own sockets
  with **+**, then select their playback outputs.
- Drag applications between sockets or right-click and choose **Move to**.
  Double-click/Enter opens the same menu. Select System there to move an app back.
- Unassigned active apps appear in a unique socket matching their actual output,
  falling back to System when none matches. Explicit assignments always win.
- Move an app once: its assignment persists indefinitely, including relaunches,
  long inactivity and hidden-window operation. Save is not required for this.
- Right-click a socket heading or Sockets-list entry to Rename, Delete, Recolor
  (16 choices) or select an icon (18 Material symbols, including PC and Laptop).
  Drag that list to reorder columns. Changes persist immediately.
- **Save** updates the board checkpoint. **Save As** copies it. Choosing another
  board applies its working routing intentions. Host profiles are supplied only
  by the future integration API; the standalone app does not invent profiles.
- Expand Recently Active, or enable and expand Inactive for remembered apps.
  Each socket scrolls independently; the horizontal bar reaches more columns.
- The Settings drawer overlays the board without changing window width.
  Light, Dark, Follow System and native high contrast apply to its controls,
  selections, menus and popups.

Rows show an observed **Playing** output or **Last seen** age. Windows can accept
an app preference while an existing stream remains on its prior output; no app
is forcibly restarted. Missing/ambiguous outputs remain pending rather than
silently substituting another device. No global Windows reset is exposed.

Learned taskbar icons survive app closure and Plug Board restarts in a disposable
local cache. An app must expose its taskbar icon once before it can be remembered.
Finite startup refreshes catch late window/custom icons for up to 30 seconds;
Refresh or reopening the view rechecks later changes. Exact image/AppID matching
is used; icons never identify apps for routing. AudioHole processes are excluded.

Close hides to the tray, and minimizing pauses full session discovery. Managed
application start events continue to reapply saved assignments. **Exit** waits
for owned cleanup and retains Windows application preferences.

Configuration lives under `%APPDATA%/AudioHole/AudioHolePlugBoard/plug-board`:
`library.json` contains portable intent; `local/` holds machine bindings, history,
icon cache and bounded error logs. Backups are retained; malformed libraries are
preserved. Close the app before editing/replacing a library or restoring a backup.
`--data-root <folder>` selects a separate root. `--demo` is explicitly simulated
and isolated; `--hidden` starts in the tray. No probe entry point is available.

This remains an isolated module, not yet merged into the AudioHole host. Its
original C++ has comments documenting contracts, threading, lifetime and failure
paths. EarTrumpet informed research only; its implementation was not copied.
Material Symbols are Google's Apache-2.0 assets, with original SVGs and hashes.

## Build and verification

CMake 3.25+, C++20 and a 64-bit compiler are required. Windows uses SDK libraries
with MSVC x64 or MinGW-w64. Pinned dependencies are wxWidgets 3.2.6, nlohmann/json
3.11.3 and test-only GoogleTest 1.16.0. Existing sources can be supplied through
CMake's `FETCHCONTENT_SOURCE_DIR_*` variables. Matching wx source is required for
the framework's themed generic file picker; framework sources are unchanged.

```sh
cmake -S . -B build/portable -DCMAKE_BUILD_TYPE=Debug
cmake --build build/portable --parallel 2
ctest --test-dir build/portable --output-on-failure
```

On Windows, use an x64 generator, build Release, run CTest, then install with
`cmake --install build/native --config Release --prefix package`. Installation
contains the single app executable, documentation and licenses. Tests remain in
the build tree. Legacy CLI fixture sources are an explicit developer-only CMake
option (`PLUG_BOARD_BUILD_DEVELOPER_TOOLS=ON`) and are never installed or linked
into the app. The removed probe/session GUI sources are no longer built.

Automated window/UI tests run under Wine as supplemental evidence. Windows mixer
launch is intercepted to verify the exact Settings URI without opening it during
tests. Actual mixer launch, physical DPI/high contrast, hardware behavior and
reboot persistence still need Windows validation. See [validation](docs/NATIVE_VALIDATION.md),
[configuration](docs/CONFIGURATION.md), [API](docs/API.md) and
[implementation status](docs/IMPLEMENTATION_STATUS.md).
