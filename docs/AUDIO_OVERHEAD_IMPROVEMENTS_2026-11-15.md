# AudioHole processing and queue overhead improvements

Owner-authorized scope: reduce AudioHole's processing overhead and avoidable
queue delay without introducing new configuration choices. Baseline is `721a0b9`.
This extends the existing validation prototype; D-034/O-005 remain open.

## Processing changes

- Select the PCM conversion kernel once per capture/render callback rather than
  testing the sample format inside every sample conversion.
- Compile and validate channel indexes at construction. The pipeline uses those
  immutable indexes directly instead of repeatedly calling the public mapping
  kernel's span/overlap validator for every single frame. The public checked
  kernel remains available to its other callers.
- Specialize identity mapping and mono-to-stereo duplication. Explicit maps,
  unmapped-channel silence, PCM clipping, nonfinite input sanitization and
  interpolation arithmetic retain their existing behavior. Matching nominal
  rates still use the existing drift correction; no clock/SRC bypass was added.
- Write capture data into at most two contiguous ring spans. Render advances a
  cached ring position instead of repeatedly applying modulo per channel/frame.

Capture and playback retain SPSC ownership of the float queue. Format, mapping
and storage are fixed before streaming. Capture publishes head with release;
playback acquires head before reading, then publishes tail after consumption.
Callbacks add no allocation, logging, locks, resizing, filesystem or GUI calls.
The mapping index view borrows its immutable mapping's lifetime. Interpolation
phase, starvation silence, counters and the bounded underrun-context mailbox
retain their prior contracts.

## Automatic queue recovery

The standard PI pipeline now maintains four one-second peak buckets in each
endpoint's own audio-frame domain. Packet/window updates are bounded and owned
by that endpoint's writer; capture publishes only its recent peak atomically.
No wall-clock queries or observer mutation run on an audio callback.

Large packets/blocks increase the guard immediately. An isolated spike expires
after approximately three to four seconds of subsequent audio in its endpoint
domain. The guard still includes a recent capture packet, a render block converted
to source frames, interpolation lookahead, and the existing negotiated-period
floor. Lifetime minima/maxima remain available as diagnostic evidence.

A lower target lets the existing bounded drift controller gradually drain excess
occupancy. It never drops queued audio, resets sample phase, enlarges a render
write, changes processing intervals, or resizes capacity to reach the target.
The controller's existing filter/integral reset on a target change remains.
Actual occupancy can take substantially longer than four seconds to settle.

An underrun after any reduction latches the conservative lifetime guard for the
rest of that session. Further growth remains possible if larger batches appear;
further recovery below that lifetime guard requires a fresh session. Explicit
`queue_target_ms` values never participate in recovery or backoff.

The alternate experimental DLL controller retains its original lifetime guard:
stress testing found an underrun after a reduction there. No DLL gains, ratio
limits or compensation algorithm were changed to conceal that finding. The
processing optimizations apply to both controllers. Standard WASAPI and
Miniaudio routing use the standard pipeline and receive queue recovery.

Diagnostics now show the **current** controller target and actual routing FIFO
occupancy in milliseconds, plus automatic reduction/backoff counts. The original
negotiated target remains the fallback for adapters without current observations.
The standalone host exports the two new optional counters. No saved profile or
application schema changed; allocation remains capacity, independent of occupancy.

## Host and interface work

- After confirmed Stopped cleanup, RoutingService waits for a command instead
  of polling/copying unchanged snapshots every 50 ms. Start/Stop, binding changes
  and shutdown wake it. Active routing and cleanup retain their polling cadence.
- Stopped routes no longer generate periodic duplicate engine-status log text.
  State-transition diagnostics and active-route logging remain available.
- Performance text updates at most five times per second for counter-only changes.
  State, assignment or issue changes refresh immediately when visible. Hidden,
  minimized and off-tab performance views defer text construction and catch up
  when shown; the controller and audio workers continue running normally.
- Log filtering is cached by event sequence/count, avatar-log revision, search
  text and severity. New events, clear, filters and avatar diagnostics invalidate
  the cache. No event or commentary generation is tied to view refresh frequency.

## Evidence and reproduction

The opt-in CPU benchmark is not installed with the application and has no timing
pass/fail threshold:

```sh
cmake -S engine -B build/pipeline-bench -DCMAKE_BUILD_TYPE=Release \
  -DAUDIOHOLE_ENGINE_BUILD_VALIDATION=OFF \
  -DAUDIOHOLE_ENGINE_BUILD_BENCHMARKS=ON
cmake --build build/pipeline-bench --target audiohole_pipeline_benchmark
build/pipeline-bench/audiohole_pipeline_benchmark
build/pipeline-bench/audiohole_pipeline_benchmark --verify
build/pipeline-bench/audiohole_pipeline_benchmark --queue-recovery
```

The old pipeline source/header were preserved before editing and built against
the same benchmark harness with C++20, `-O3 -DNDEBUG`, the same compiler and
contract sources. All 156 deterministic output fingerprints matched: every PCM
format pair, rate conversion, identity/mono/explicit mapping, irregular block
sizes, wrapping, silence, nonfinite input and forced underruns. These are
same-toolchain comparisons, not a cross-platform bit-perfect guarantee.

CPU trials used seven interleaved baseline/optimized runs pinned to the same
allowed Linux CPU. Values include capture push plus render, divided by output
frames; allocation, setup and warmup are outside the timed region. This measures
software kernel cost rather than total application CPU, scheduling or acoustic
latency. Raw samples: [benchmark results](performance/pipeline-2026-11-15.json).

| Case | Before, ns/frame | After, ns/frame | Reduction |
|---|---:|---:|---:|
| Float stereo | 15.54 | 9.51 | 38.8% |
| Float mono to stereo | 15.13 | 8.96 | 40.8% |
| PCM16 stereo | 19.80 | 10.39 | 47.6% |
| Explicit channel map | 19.32 | 9.83 | 49.1% |
| Rate conversion | 15.76 | 9.10 | 42.2% |
| Packed PCM24 to PCM24-in-32 | 20.93 | 11.22 | 46.4% |

A deterministic 48-kHz run starts with one 100-ms capture batch, then regular
10-ms capture/render callbacks. At 179 audio seconds, the old FIFO target remains
5,282 frames (110.04 ms) and contains 4,793 frames (99.85 ms). The new target is
962 frames (20.04 ms), with 482 frames queued (10.04 ms), and zero underruns.
This is a software queue experiment; it excludes playback/device queues. The
[before](performance/pipeline-queue-baseline.csv) and
[after](performance/pipeline-queue-optimized.csv) traces show gradual settling.

Processing regressions cover sustained batched cadence, 44.1/48-kHz rate domains,
three minutes of recovery, recurrent bursts, starvation backoff, explicit targets,
and preservation of the experimental DLL guard. Existing conversion/concurrency,
access, startup and lifecycle tests remain enabled. The service test verifies
stable idle publications and wakeup on Stop; existing integrations verify Start
and cleanup. EngineSmoke verifies hidden telemetry stays unchanged while frame
progress continues, then refreshes on return. Existing log-clear/filter and GUI
tests continue to exercise view invalidation.

Cross/Wine execution is supplemental to native Windows CI. Native scheduling,
driver behavior and physical-device stability are not inferred from these trials.

Local validation: all **282** portable CTest cases passed; AddressSanitizer and
UndefinedBehaviorSanitizer reported no errors across **102,499** processing/
lifecycle checks. The Windows Release cross-build passed. All four engine
executables and **262** model tests passed under Wine; the model run retains the
existing `ConfigurationBundle.*` and
`PersonalityFiles.MissingOptionalFeatureAndCapabilitiesNeverBorrowFromOtherPackages`
exclusions for MinGW/Wine path handling, with those tests still enabled in native
Windows CI. All five GUI smoke modes passed at 96 DPI, including the restart child;
EngineSmoke was repeated after the final experimental-guard change.

GitHub publication could not be verified because the environment's GitHub
credentials were rejected. No remote CI result is claimed.
