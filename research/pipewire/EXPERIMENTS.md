# Experiments and validation plans

## What was actually executed

All execution remained inside `research/pipewire/`. Environment: managed Linux x86-64, kernel `6.18.55`, glibc 2.41, GCC `14.2.0`, Python 3.12.14, NumPy 2.3.5. Runtime versions are recorded in `experiments/numerics.json`. These are code/arithmetic/model checks. No Windows endpoint, ALSA hardware, audio signal, acoustic loopback, PipeWire daemon, benchmark suite, or long-running wall-clock audio session was tested.

### E0 — Provenance and workspace checks

- The assignment was created first as `AGENTS.md`, compared byte-for-byte with the uploaded original, and checked for its 18,840-byte length and SHA-256 digest recorded in README.md.
- PipeWire tag 1.4.9 was resolved through the official repository to commit `fd60e04525f3a04d90bf50085222e0cc9139b4a4`.
- Selected snapshots are compared to their Git blob hashes, including the original `blob <length>\0` header. They are original source references, not locally edited adaptations.
- `experiments/verify_research.py` checks the original assignment prefix, publication instructions, manifest, available source blobs, and authored-document local links/anchors. Use `--require-reference-snapshots` to require all 49 optional cached references. Tracked AudioHole files are checked separately with Git. Publication uses an explicit allowlist on a dedicated branch based on current remote `work`; the shared local checkout is not switched.

### E1 — Original DLL and ring-buffer header probe

**Objective:** verify the documented first-step DLL algebra, 32-bit ring wrap behavior, and closed-loop behavior under a declared simplified plant.

**Method:** `experiments/dll_probe.c` includes the unchanged upstream `dll.h`, `ringbuffer.h`, and `defs.h`. It checks a six-byte copy split over an eight-byte ring end while uint32 indices wrap, asserts fill/content correctness, and checks the DLL's first response against the derived coefficient identity. It then calls the real DLL for each synthetic queue update.

The common plant uses Q=128, F=48,000, and

\[
e_{k+1}=e_k+128(c_{effective,k}-(1+\delta_k)).
\]

`e` represents playback delay minus target or capture target minus delay. Ideal actuation sets `c_effective=c`. Two alternative **rate-only models** approximate the native rate-update quantization:

- Playback: `c_effective=48000/floor(48000/c)`.
- Capture: `c_effective=floor(48000*c)/48000`, accounting for capture's reciprocal RateMatch factor.

The harness does not run native FIR/filter history, packet delivery, integer per-cycle frame counts, timer scheduling, or ALSA recovery. The ideal/quantized plants have continuous-valued frame occupancy; finite-block resampler effects are not reproduced. Error input is clipped at ±256 frames. Adaptive scenarios reconstruct the exact mean/variance-like bandwidth heuristic with a double elapsed-time trigger just over 3 seconds; the production uint64 next-time trigger and threshold compensation are not emulated.

Jitter is measurement-only `16 sin(2πk/17)` frames. Step scenarios reverse +100 ppm to −100 ppm at 300 simulated seconds. The jump scenario injects 512 frames at 300 seconds and applies an explicitly **proposed** instantaneous reset-to-target abstraction above 160 frames; this is not a test of ALSA rewind/forward behavior.

**Reproduction:**

```sh
python3 experiments/fetch_references.py --probe-only
mkdir -p experiments/build
cc -std=gnu11 -O2 -Wall -Wextra -Werror -I upstream/pipewire/spa/include experiments/dll_probe.c -lm -o experiments/build/dll_probe
./experiments/build/dll_probe > experiments/dll_results.csv
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I upstream/pipewire/spa/include experiments/dll_probe.c -lm -o experiments/build/dll_probe_sanitized
ASAN_OPTIONS=detect_leaks=0 ./experiments/build/dll_probe_sanitized > experiments/dll_results_sanitized.csv
cmp experiments/dll_results.csv experiments/dll_results_sanitized.csv
```

**Observed results:** strict compilation and all harness assertions passed; address/undefined-behavior sanitizer run with leak detection disabled passed with identical CSV output. An initial sanitizer execution failed because LeakSanitizer is incompatible with this environment's ptrace tracing; leak checking was therefore **not performed**. There is no allocation in the probe's queue model, but that does not establish leak freedom for any full engine.

| Scenario | Simulated duration | Observed e range, frames | Final 60-second e range, frames | Observation |
|---|---:|---:|---:|---|
| Ideal fixed b=.128, +100 ppm | 21,600 s | −2.849222 to .074721 | 0 to 0 at printed precision | Bounded six-hour model trajectory; command converged to +100 ppm. |
| Ideal fixed b=.128, −100 ppm | 600 s | −.074721 to 2.849222 | 0 to 0 at printed precision | Opposite mismatch sign remained bounded. |
| Adaptive bandwidth, +100 ppm | 600 s | −8.357013 to 3.621321 | −.402986 to −.259954 | End bandwidth .001; small residual slow transient remains. |
| Quantized playback, +100 ppm | 600 s | −2.564613 to .422732 | −.040159 to .039361 | Small bounded residual oscillation in rate-only model. |
| Quantized capture, +100 ppm | 600 s | −3.157867 to .424533 | −.039467 to .039467 | Small bounded residual oscillation in rate-only model. |
| Adaptive plus measurement jitter | 600 s | −3.648837 to .133490 | −.000163 to .000148 | Synthetic high-frequency measurement noise largely filtered. |
| Fixed-bandwidth rate sign step | 600 s | −2.849222 to 5.698445 | 0 to 0 at printed precision | Bounded response to specified step. |
| Adaptive-bandwidth rate sign step | 600 s | −8.357013 to 36.679540 | 1.101710 to 1.364327 | Slower recovery and larger excursion in this model than the fixed comparator. |
| Proposed jump/reset | 600 s | −2.849222 to .074721 after reset | 0 to 0 at printed precision | One reset executed; injected jump was reset before extrema accumulation. This does not mean no discontinuity occurred. |

Raw results include tail means, final command, max command, bandwidth, reset count, and exact printed precision. A final quantized command is not the time-averaged physical ppm rate; the loop can alternate actuator bins. No result establishes inaudibility, spectral quality, actual memory/CPU cost, robustness to delayed packets, or universal controller stability. Six simulated hours completed through accelerated arithmetic, not six wall-clock hours.

### E2 — Arithmetic and fixed-loop poles

`experiments/numerics.py` reproduces coefficients, four rate-quantization examples, ring-wrap/drift arithmetic, and the 4×4 ideal closed-loop matrix in ALGORITHMS.md. `experiments/numerics.json` records numeric results and tool versions. All computed fixed-bandwidth poles for .128, .016, and .001 at 128/48 kHz were inside the unit circle. This is a local linear-model result under explicit assumptions, not a nonlinear stability proof for PipeWire or Windows.

## Proposed isolated experiments — not executed

The following are engineering plans, not reported results. Numerical test inputs and pass criteria are **proposed experiment settings**, not values claimed to be optimal or taken from PipeWire. Freeze settings, workload, and criteria before comparing candidates. Any future test involving production code or Windows device access is a separate task; intrusive device probing follows AudioHole's consent rules.

| ID | Objective and methodology | Required instrumentation / outputs | Proposed success criterion |
|---|---|---|---|
| P1 — Windows timing observability | Capture and render on two independent endpoints; equal 48 kHz nominal rates; shared and supported exclusive modes. Sample correlated clock/packet timestamps over ≥60 min and include a stable-clock control where available. Compare packet and application-queue observations. | Windows versions/driver/endpoint IDs, formats/modes/actual periods, GetFrequency units, packet flags/positions/QPC, API HRESULT and duration, render padding, source queue counts. Derive slopes from valid windows only; record uncertainty and rejected observations. | No unit/domain mismatch; every invalid observation is identified; finite bounded estimator behavior across startup/reset; measured drift estimate is consistent with observed queue trend within an explicitly reported uncertainty interval. Do not call initialization proof of playback. |
| P2 — Controller comparison | Replay identical synthetic/retrieved timing traces through fixed DLL, exact adaptive DLL heuristic, bounded PI, and optionally timestamp feed-forward. Sweep ±1, ±10, ±100, ±500 ppm; Q=48/128/480 at 48 kHz; impose batching, delayed observations, and rate steps. | Queue trajectory, correction/actual consumption, settling time (e.g. first sustained 30 s inside ±1 frame on a noiseless trace), overshoot, saturation/windup, underrun/overrun/reset count. | Under each declared supported drift/noise envelope, queues stay within physical bounds without repeated emergency resets. Comparisons use equal target occupancy and recovery policy; report failures rather than retune one candidate secretly. |
| P3 — Real ASRC precision and quality | Run full candidate converters on impulse, sine sweep/multitone, silence and long noise; ratio updates at sub-bin and large ppm values; compare native quantized approach with a higher-precision candidate and backend conversion. | Exact input/output counts, phase continuity, impulse-derived delay, spectra/sidebands/aliasing, channel agreement, block-boundary artifacts, allocation and execution timings. | No frame accounting loss or discontinuity on ordinary small ratio updates; measured filter delay agrees with reporting within declared tolerance. Freeze numeric spectral/CPU acceptance thresholds before claiming any candidate is better; model occupancy alone is insufficient. |
| P4 — Discontinuity recovery | Inject timestamp invalidation, 1/4/20 missing periods, capture overflow, render starvation, and device-rate step. Test proposed silence/discard/re-prime policies independently of drift tracking. | Missing/dropped/inserted frames, event sequence, generation tags, ratio bounds, time to resume, ASRC state reset, acoustic artifacts if safely measured. | Work remains bounded; invalid timing never integrates indefinitely; no obsolete-generation audio after reopen; explicit Stop prevents further recovery. Recovery duration meets a predetermined route-specific bound and is honestly reported. |
| P5 — Latency/CPU scheduling comparison | Native/miniaudio/hybrid where available; single worker versus split workers; fixed negotiated periods; 1, 4 and 8 holes including duplex, idle and loaded systems. Compare using the same signal and endpoint pair. | External physical loopback with synchronized instrumentation; requested/actual periods, sample age/queue depth/filter delay, callback lateness/runtime p50/p95/p99/max, CPU time, memory, xruns. | Attributable latency ≤10 ms on explicitly supported cases (5 ms stretch) with no sustained failures under the declared workload; report unsupported/failing cases and uncertainty. Do not subtract unrelated measurements to invent attributable latency. |
| P6 — Long-duration stability | ≥24 h real routing on independent devices including nominally equal and unequal rates; periodic thermal/load changes; shared capture with two independent mirrored render clocks; separately test a backend/driver clock stall. | Long-term occupancy/ratio trends, maximum callback duration, xruns/reset/discard counts, memory trend, all stream generations, state transitions. | No uncontrolled queue trend, unbounded memory growth, or recurrent catastrophic resync in healthy steady operation. A stalled destination cannot silently block unrelated destinations or corrupt a shared capture cursor. |
| P7 — Lifecycle and duplex isolation | Endpoint removal/return, sleep/wake, backend restart, exclusive conflict, and profile switching; exercise explicit Stop during each recovery stage. | Per-direction lifecycle/events/HRESULTs, resource counts, canceled command/generation records, timestamp resets, continuity of healthy reverse/unrelated paths. | Exact configured endpoint policy; no stale estimates reused across reopen; no recovery after Stop; healthy paths remain active where required; no duplicate streams or resource leaks. |
| P8 — Numerical/queue stress | Exercise ring counters around 2^32, distances near policy limits, maximum negotiated packet sizes, ratio=0/negative/NaN/Inf, failed time queries, large timestamp jumps, changing quantum/rate, and diagnostic saturation. Use thread instrumentation separately from release timing runs. | Bounds assertions, ASan/UBSan or platform equivalents, concurrency checking, rejection counters, consumed/produced frame conservation. | Defined rejection/recovery for invalid input, no out-of-bounds accesses or data races, and no audio-path wait/allocation induced by diagnostics. Sanitizers are correctness tools, not timing benchmarks. |

All future results should include tool/source revisions, hardware/driver/OS and mode, configuration, raw measurements, filtering/rejection rules, uncertainty, failures, limitations, and a distinction between reported capability, initialized stream, verified frame movement, and externally confirmed output.
