> Current artwork revision: 97 IDLE padding frames removed; 699 operating PNGs remain.
> See [padding removal and validation](../IDLE_PADDING_REMOVAL.md). Historical delivery counts below describe PR6.

# AudioHole avatar engine and standalone viewer

This is the native C++20 animation module for the formally accepted Norm,
Horu (`uwu`) and Vanilla library. The standalone wxWidgets viewer links the same
`audiohole_avatar` and `audiohole_avatar_wx` libraries proposed for AudioHole.
It runs without an audio engine, scanner, personality service, server or network.

All 796 operating PNGs remain byte-identical to the accepted artwork.
[Acceptance hashes](../provenance/accepted-assets.json) are included. The full
generation checkpoint `0b41b8e` is available separately on `avatar/asset-pipeline`
and the `avatar-generation-accepted-2026-11-02` tag.

## Build and launch

Required: C++20 compiler, CMake 3.25+, wxWidgets 3.2, nlohmann/json 3.11.3+.
The inspected Windows GUI uses wxWidgets **3.2.6** and JSON **3.11.3**. This module
reuses those targets when embedded. The Linux validation host uses wxWidgets
3.2.8, JSON 3.11.3 and GCC 14.2. No dependencies are downloaded by the module.

On Debian/Ubuntu with development packages installed:

```sh
cmake -S assets/personalities/runtime -B assets/personalities/runtime/build -DCMAKE_BUILD_TYPE=Release
cmake --build assets/personalities/runtime/build -j
ctest --test-dir assets/personalities/runtime/build --output-on-failure
assets/personalities/runtime/build/audiohole_avatar_viewer --root assets/personalities
```

Install the distribution's `cmake`, C++ compiler, `libwxgtk3.2-dev` and
`nlohmann-json3-dev` packages if needed. Python is optional for packaging and
validation utilities; it is not part of playback or the viewer.

On Windows, use an x64 Visual Studio 2022 developer shell and installed matching
wxWidgets/JSON development packages:

```powershell
cmake -S assets/personalities/runtime -B assets/personalities/runtime/build-win `
  -A x64 -DwxWidgets_ROOT_DIR=C:/sdk/wxWidgets-3.2.6 `
  -DwxWidgets_LIB_DIR=C:/sdk/wxWidgets-3.2.6/lib/vc_x64_lib `
  -Dnlohmann_json_DIR=C:/sdk/json/share/cmake/nlohmann_json
cmake --build assets/personalities/runtime/build-win --config Release
ctest --test-dir assets/personalities/runtime/build-win -C Release --output-on-failure
./assets/personalities/runtime/build-win/Release/audiohole_avatar_viewer.exe --root ./assets/personalities
```

Match the MSVC runtime setting to the wxWidgets build. The existing AudioHole
parent uses the static CRT and supplies wx targets itself; embedding avoids
selecting incompatible libraries. Windows uses system WIC, not wxPNG/libpng,
so the parent GUI's disabled PNG feature is supported. The viewer requests
asInvoker and per-monitor-v2 DPI awareness. Windows build/render/DPI verification
is a remaining platform check; Linux validation does not establish it.

Portable layouts can place the executable beside `registry.json`, under
`runtime/bin/`, or beside an `assets/personalities/` folder. `--root` overrides
discovery. Keep the three pack directories, registry and manifests together.

## Viewer controls

Choose Vanilla, Norm or Horu; choose a variant and state; use Play, Pause,
Restart, Step, speed, static inspection and reduced motion. Regular operating
art was not generated: selecting it exercises the required hide/diagnostic path.
The image can fit the window or use exact 128×192, 144×216, 256×384 or 512×768
bounds. Checkerboard pixels belong to the viewer background, never the artwork.

Operational controls simulate the actual eight `audiohole::State` values and
`Snapshot::scanning`. Healthy Running stays ambient IDLE. Send existing
presentation-message events to inspect priorities, cooldowns and interruptions.
The metadata panel shows the current baseline, revision, frame, exposure,
source/fallback, priority, admission decision and diagnostics. A missing-root
checkbox exercises a real failed load without modifying accepted files.

**Inspect state** is an explicit art-inspection path that bypasses event
admission/cooldown. Operational simulations use the normal policy. AudioHole
integration must use baseline/reaction requests, not manual inspection.

## Components and host contract

| Component | Responsibility |
|---|---|
| `Repository` | Read-only registry/manifest parsing; owned-path validation; PNG/header/hash/alpha validation; bounded decode/cache |
| `Player` | Clock-injected exposure timing, finite reactions, latest baseline, priority, deduplication, cooldown, pause/speed/step/reduced motion |
| `Session` | One loader thread, coalesced cancellable jobs, stale-result barriers, host-thread polling and lifecycle |
| `Panel` | Native wx alpha rendering and one cached scaled bitmap; no files or personality decisions during paint |
| Viewer | Controls and explicit simulations; links the above production libraries |
| Proposed presenter | Read-only application-state/message adapter for primary-agent review |

Public headers are in `include/audiohole/avatar/`.
[API, lifecycle and resource details](docs/API.md) ·
[GUI inspection and integration handoff](../integration/HANDOFF.md) ·
[Executed checks](validation/REPORT.md).

Generation scripts, source/reference images, historical previews and review
archives are excluded from this curated branch. They remain in the separate
[full handoff PR](https://github.com/RLCypher/audiohole/pull/4). Playback and this
viewer do not depend on those generation/review tools.

## Desktop pet foundation

Playback and decoded straight-alpha pixels do not depend on a top-level window.
A future host can reuse `Session`, `Player` and the decoder with a different
surface, or reuse the wx panel. Borderless/transparent windows, always-on-top,
dragging, click-through, monitor placement, persistence, tray and live AudioHole
connectivity belong to that host. None is implemented or required here. The
viewer has no hard dependency on a future connection or autonomous personality
service.

## Scope

No main AudioHole source/build files were modified. The proposed adapter uses
the inspected application's real state/message declarations for compilation;
it does not instantiate or call the controller. Integration is prepared, not
applied. The supplied code/examples require review and application by the
primary AudioHole development agent.
