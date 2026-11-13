# AudioHole engine 0.6 — GUI agent handoff

## Readiness and scope

Ready for GUI integration and continued real-device testing as a separate C++20
module. This is a validation prototype, not production feature completion. No GUI
code was modified by the engine work. Integrate the existing module behind the
application/controller boundary; keep device scanning, storage and presentation
owned by their existing components. Do not copy routing logic into widgets.

The source is under `engine/`, based on parent commit
`fda20900e8954993e267e2ca1d6d7be8a19840dc`. The engine work is uncommitted in this
workspace; the published source archive is the transferable snapshot. Original
0.6 source and Windows archives remain immutable. The separate handoff bundle
adds current reference documents; they supersede older build-time status notes,
not the tested implementation or profile contract.

Read [PROFILE_V3_HANDOFF.md](PROFILE_V3_HANDOFF.md) for the current serialized and
status contract, [README.md](README.md) for implementation limits and build
details, and [VALIDATION.md](VALIDATION.md) for verification evidence. The parent
`PROFILE_BUILDER.md` bundled in the source is a historical baseline; use the
current engine handoff for the owner's v3/v7 update.

## Integrating the module

The library entry point is `engine/CMakeLists.txt`; public headers are under
`engine/include/audiohole/engine/`. In a parent CMake project with C and C++
enabled, the integration pattern is:

```cmake
# Exclude the separate console host and its scanner/store adapter from the GUI.
set(AUDIOHOLE_ENGINE_BUILD_VALIDATION OFF CACHE BOOL "Build standalone engine host")
add_subdirectory(engine)
target_link_libraries(your_gui_target PRIVATE audiohole::engine)
```

Replace `your_gui_target` with the actual host/controller target. Set the option
in a fresh integration configuration or explicitly override an existing ON cache
entry. Do not turn off the parent GUI's tests to integrate this library. Parent
compiler/CRT policy must remain compatible across linked C++ components.

The core streaming target links `audiohole::engine_contract` and owns its native
workers. JSON parsing is in the contract library; scanner/store adaptation in
`validation/main.cpp` belongs to that console HOST, not the engine core. Miniaudio
is optionally compiled in using the existing pinned dependency. No new library,
virtual driver or administrator requirement is needed for this handoff.

## Host responsibilities and API sequence

1. Retain the original individual profile JSON for editing/persistence. Pass its
   UTF-8 contents to `parse_profile()` from `profile.hpp`, or build the equivalent
   typed `Profile`. `prepare_profile()` can validate draft routing intent without
   opening devices; `Runtime::start()` also prepares and checks admission.
2. Produce an immutable `BindingSnapshot` from the authoritative scanner/local
   registry. Each `LocalBinding` includes stable logical ID, opaque Windows
   endpoint ID, capture/render direction and availability/ambiguity state.
   Never guess from friendly names or fall back to a Windows default endpoint.
   The console host's `bindings()` function is a read-only adapter example.
3. Construct `Runtime` from `runtime.hpp` on ONE application-owned control thread.
   Call `backends()`, `start()`, `snapshot()` and `stop()` on that same thread.
   Prefer a dedicated control thread with queued GUI commands and copied results;
   do not make window widgets own audio-worker resources or shutdown joins.
4. Check the returned `Result<uint64_t>` for Start admission errors. Admission is
   not confirmation that audio is flowing. Poll/copy `RuntimeSnapshot` on the
   owner thread and marshal it to the GUI at a modest UI rate, outside callbacks.
   Use `operation` and `revision` to reject stale queued UI updates.
5. Present each directional `PathSnapshot` as well as aggregate state, including
   actual negotiated source/destination formats/modes, access receipts, frame
   progress and issues. Successful Shared fallback is explanatory history, not a
   fatal issue. A healthy sibling may continue while the overall state is Degraded.
6. Stop requests cleanup asynchronously. Wait for Stopped/disarmed before another
   Start, profile change or binding replacement. Keep the application-owned
   Runtime alive according to the existing controller/tray lifecycle; closing a
   view must not implicitly destroy a live engine. Destruction joins workers and
   may wait for a driver API; never pretend it has a hard cancellation deadline.

The successful latest hardware run selected native WASAPI with
`Runtime(LatencyPreset::Low)`. The default constructor selects Baseline. This
validation preset is a host option, not a profile field. Keep the chosen preset
explicit in tests/diagnostics. Miniaudio does not change tuning with that preset.

## Profile and status requirements to preserve

- Individual `audiohole-profile` schema **3**; aggregate transfer schema **7** is
  GUI import/export, not a routing input. Old v1/v2 individual loading is read-only.
- Hole `routing` selects automatic/wasapi/miniaudio/experimental. Automatic picks
  an available native adapter first, then an available Miniaudio adapter; it does
  not retry failed native routing through another backend or select Experimental.
- Each direction owns `advanced_tuning` and `exclusive_mode_preferred` separately.
  Advanced-disabled manual fields remain saved but inactive; preference remains
  active. Effective preferences inherit endpoint → common → Automatic.
- Forced access modes and other explicit requirements are strict. An unforced
  Exclusive preference permits stopped acquisition fallback plus ONE route
  cleanup/reopen before first transfer if startup fails or the 2-s validation
  watchdog expires. Strict Exclusive never silently becomes Shared. A later
  failure after observed transfer does not trigger mode migration.
- Startup retry can change automatic client formats and reset that route's
  counters. Show the new negotiated configuration and its retained failure
  receipt. Capture silence is valid frame progress; zero transfer is different.
- Preserve source JSON, unknown fields, filenames and notes through the existing
  persistence owner. Do not serialize from the engine's reduced typed projection.
- `preferred_clock_device` is retained metadata with
  `clock_preference_applied: false`. No clock-master feature is implemented.
- Counters, allocated buffers, progress age and correction ppm are diagnostics,
  not calibrated latency, audible-output proof or measured physical clock drift.

## Evidence and unresolved work

All four portable test programs pass normally and under ASan/UBSan. Windows x64
Release, including the real adapters and standalone host, cross-compiles. Native
Windows test execution and integrated GUI testing remain receiving-agent work.
The executable is unsigned; publisher trust is not established by this handoff.

[The latest hardware review](LIVE_RUN_REVIEW_2026-11-12_V06_WIRELESS.md) shows:

- Exclusive startup stalls on Corsair mic → RCP2 Main playback, then recovers to
  Shared by the 2,239-ms snapshot. The other direction continues through recovery.
- About 18 minutes of logged duplex routing; after fallback, zero FIFO underruns,
  overruns, continuing capture discontinuities or timestamp errors in both paths.
- The short capture-wait mechanism was never invoked in that Low run. Baseline
  behavior still needs a comparable test; older baseline runs had underruns.
- RF-range excursions and continued playback after the combined 16-MiB log cap
  are user observations. USB endpoint removal/reconnection and eventual Stop are
  not verified by that attachment. Earlier builds have separate clean-Stop evidence.

The user reports VirtA also misbehaves under other conditions and deliberately
replaced it with RCP2 Main. Use Main as the present working reference; investigate
VirtA separately. Main also reproduces Exclusive startup failure, so do not
attribute the entire Exclusive problem to VirtA or declare its driver faulty.

Remaining limits: automatic device reconnection, transactional live profile
switching, live configuration changes and destination mixing are not implemented.
Repeated capture use requires compatible Shared clients; multiple routes into
one destination are rejected. The converter is float32/linear adaptive SRC, not
a production-quality or bit-perfect guarantee. Overall latency targets are not
established. See README for Miniaudio's restricted PCM representations.

These limits do not prevent integrating Start/Stop, profile selection while
stopped, actual status/telemetry, diagnostics and real hardware testing. They do
prevent presenting the prototype as a complete production routing supervisor.

## First receiving-agent checks

Build/link the module with the GUI without the console host. Verify parse/admission
errors and unavailable/ambiguous bindings, actual Running confirmation, fallback
presentation, forced-mode failure, Stop during startup, and stale-update rejection.
Run the known working hardware profile in Low, then repeat that same profile in
Baseline. Preserve the native GUI smoke checks. Test profile replacement only
after Stop completes; leave unsupported live switching/recovery explicit.

Do not introduce synthetic devices into normal GUI execution. The isolated test
fixtures are for software contracts; live hardware remains the validation target.
