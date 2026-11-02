# Executed avatar validation — 2026-11-02

The engine and independent viewer build and run on the Linux validation host.
They share the same `audiohole_avatar` / `audiohole_avatar_wx` libraries proposed
for AudioHole. Main-application integration has **not** been applied or tested.

## Environment and results

Debian 13 x86-64, GCC 14.2, CMake 3.31.6, wxWidgets 3.2.8 GTK, nlohmann/json
3.11.3. Development tools/headers were extracted locally, without changing system
packages. The inspected application instead targets Windows x64 and wx 3.2.6.

| Executed check | Result | Evidence |
|---|---|---|
| Native C++ policy, fault, lifecycle, isolation and real-library tests | 29 passed, 0 failed | [Native report](native-tests.json), [CTest](ctest.log) |
| Native codec versus independent Pillow decode | All 796 RGBA buffers match byte-for-byte | [Pixel comparison](native-pixels.log) |
| Existing Python asset/pipeline/playback regression tests | 57 passed | [Python results](python-tests.log) |
| AddressSanitizer + UndefinedBehaviorSanitizer | All 29 tests passed; no reported address/UB violations, leak detection disabled | [Sanitizer results](asan-ubsan-tests.log) |
| Real wx GUI, independent viewer under Xvfb | 18 checks passed | [GUI report](viewer-smoke.json), [actual window](viewer.png) |
| Existing offline IDLE browser previews | 12 checks passed | [Browser results](browser-idle.log) |
| Existing offline full-library browser previews | 6 checks passed | [Browser results](browser-full.log) |
| Formal acceptance/frozen artwork | All 796 operating PNG hashes unchanged | [Integrity record](integrity.json) |
| Read-only upstream inspection | All 17 captured Git blobs unchanged | [Inspection inventory](../../integration/inspection/manifest.json) |
| Asset audit | 52 statics, 49 sequences, 744 frames; no errors | [Audit](asset-audit.log) |

Native checks exercise all eight actual routing states, healthy Running staying
ambient across 1,000 baseline observations, current-baseline return after finite
reactions, failure preemption, recovered-event suppression, stale revisions,
TTL/deduplication/cooldowns, interruptibility, rapid changes, cancellation,
personality isolation, reduced motion, pause/speed/step, bad paths/symlinks,
corrupt frames, approval gates, owned-frame freezing, memory limits and shutdown.
Synthetic policy/fault fixtures are distinguished from actual PNG codec tests.

The GUI check uses a real native window and production asset loader/player. It
selects all three personalities, exercises talking, pause/step and reduced motion,
simulates healthy routing/scanning/failure, rejects cosmetic success over failure,
loads an actually missing root, hides unavailable regular art, recovers, verifies
exact 128×192 and 512×768 bounds, and checks controls fit the screenshot viewport.
The screenshot was also visually reviewed for framing, transparency and controls.
It establishes standalone-viewer behavior, not behavior inside AudioHole's header.

## Performance observations and limits

The final serial GUI run retained **22.5 MiB** of decoded RGBA data and one
**1,971,120-byte** scaled bitmap. Its maximum measured `Panel::present` duration
was **10.99 ms** on this software-display host. Unchanged frames reuse the bitmap;
file reads, SHA checks and PNG decoding run on the owned loader worker. The GUI
still performs bounded RGBA conversion and scaling when a new frame is displayed.
This small scripted run is not a long-duration or target-Windows performance test.

The full-library Release test measured a maximum **1,018.61 ms** to validate/load
one state's static plus animation during concurrent validation work. It is
background load latency, not an audio-callback or paint measurement. Cold changes
can initially hide/freeze while loading. No zero-latency transition claim is made.
Decoded pixels have a 128 MiB retained budget and 64 MiB per-load limit; the panel
retains one bitmap, capped at 4× native dimensions (24 MiB). Tests exercise weak
cache release and rejection at the budgets. These limits exclude temporary
decoder buffers, JSON/input bytes, GUI toolkit overhead and caller-owned metadata.

LeakSanitizer's initial run failed because this managed environment does not
support its ptrace requirements. The successful ASan/UBSan rerun used
`ASAN_OPTIONS=detect_leaks=0`. **No LeakSanitizer pass is claimed.** Ownership tests
are useful evidence but do not replace a platform leak profiler.

## Reproduction

Build and run CTest using the [runtime instructions](../README.md). The C++ suite
writes `build/validation/native-tests.json`. With Pillow installed:

```sh
python assets/personalities/runtime/tests/verify_native_pixels.py \
  --root assets/personalities \
  --report assets/personalities/runtime/build/validation/native-tests.json
python assets/personalities/runtime/tests/gui_smoke.py \
  --viewer assets/personalities/runtime/build/audiohole_avatar_viewer \
  --root assets/personalities --output /tmp/audiohole-avatar-smoke
```

The GUI utility uses an existing display or Xvfb; Windows uses its desktop.
Sanitizer builds use compiler/linker `-fsanitize=address,undefined` with
`-fno-omit-frame-pointer`; this host requires leak detection disabled as above.
Local GUI/browser validation needed permission for local display/IPC sockets;
the viewer itself makes no network requests and requires no server.

## Remaining validation

Windows MSVC/WIC compilation, WIC byte comparisons, per-monitor DPI, deployment,
and actual AudioHole header integration remain unverified. Follow the
[Windows checklist](../docs/WINDOWS_VALIDATION.md) before accepting those claims.
The inspected backend is a `SimulatedController`; no audio routing/hardware,
real-time performance or real device behavior was tested or changed here.

Approved generation limitations remain: regular operating art is unavailable,
some animations use held drawings and deliberate pose cuts, and source events
lack timestamps/general probing/apply notifications. Runtime policy does not
invent those notifications, smooth or regenerate accepted drawings, or borrow
another personality's art. Horu remains the display name; `uwu` remains the ID.
