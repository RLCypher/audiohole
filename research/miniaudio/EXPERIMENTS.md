# Experiments: executed evidence and proposed Windows validation

Revision: [README](README.md). Evidence label is explicit per experiment. Every artifact stays in `research/miniaudio/`. No production code or architecture was changed.

## E1 — executed processing-only source-behavior probe

**Status: executed, software behavior only.** Original code: [processing_probe.c](experiments/processing_probe.c). Output: [processing_probe_results.txt](experiments/processing_probe_results.txt). Environment/build details: [environment.json](experiments/environment.json).

- Objective/hypothesis: verify actual ratio grids and phase arithmetic; determine static/dynamic converter behavior; confirm duplex helper units; exercise packed-s24 shuffle output extent.
- Environment: provided Linux x64 cloud executor, GCC 14.2.0, C99, unmodified miniaudio 0.11.25 at the pinned commit. Device I/O, decoding, encoding, engine, resource manager and node graph disabled. No audio device is opened.
- Method: initialize public objects, call public setters/processing methods, inspect their exposed state and counts. For phase, generate 10000 outputs through the public API before rate change; no private state injection. For s24, provide 12 guarded bytes for a logical six-byte output, avoiding an actual storage overrun while observing contract violation.
- Instrumentation: C assertions, printed requested/effective rational rates, phase counts, ring capacity/occupancy and byte canaries. Assertions deliberately expect the defects at the pinned revision: this is a reproducible witness, not a test declaring them correct behavior.
- Outputs/units: dimensionless rate ratios; ppm implied by difference from 1; phase numerator units with declared denominator; frames consumed/generated; ring frames; result codes; output/guard bytes.
- Criteria and actual results: generic +100 ppm rounded to 0 and −100 ppm to −1000; direct linear matched about ±100 ppm; phase expected 10000 but became 1410; equal static converter bypassed SRC and rejected later set-rate (−3), dynamic converter allowed it; ring capacities/prime counts 2400/960 and 2400/882; s24 shuffle changed logical data and guard offset 8. All witness assertions completed.
- Confounders: f32 parameter representation/truncation, compiler/architecture integer model, compile-time component options. Exact printed values are retained, not rounded into fictitious precision.
- Limits: no WASAPI execution, Windows COM lifecycle, MSVC build, sound quality, latency, CPU benchmark, physical drift or long-run stability evidence. AddressSanitizer and UndefinedBehaviorSanitizer completed after disabling LeakSanitizer: the initial leak-check attempt failed because this executor uses ptrace. Leak detection is unavailable here. A clean address/undefined-behavior run with the oversized guard buffer does not contradict a logical output-extent write violation.

Reproduce from this directory:

```sh
bash experiments/run_probes.sh
```

Optional memory/undefined-behavior instrumentation of this probe:

```sh
bash experiments/run_probes.sh --sanitize
```

Before first reproduction, obtain the local-only upstream reference checkout (not committed):

```sh
git clone --no-checkout https://github.com/mackron/miniaudio.git upstream/miniaudio
git -C upstream/miniaudio checkout --detach 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d
```

Run those commands only for this dedicated upstream directory; they do not switch AudioHole branches. The script verifies the pinned checkout before building. Normal and sanitizer outputs are kept separately. Any future repair should change witness expectations to correctness assertions and compare against the archived baseline; do not silently rewrite evidence.

## Shared protocol for proposed Windows experiments

All E2–E10 below are **Experimental / not run**. Use Windows 10 and 11 x64 with recorded build, compiler/options, CPU/power plan, driver versions, endpoint IDs/fingerprints, OS mix format, endpoint mode and actual initialized format/period/capacity. Exercise an integrated codec, USB duplex interface, two independent USB devices and virtual endpoints where available. Wireless transport is a separate cohort, not part of the same latency budget. Avoid claiming same enclosure proves one clock.

Isolated harnesses belong here. Keep opt-in physical signal levels quiet and bounded. Use owned test hardware; no intrusive scans on unrelated active routes. Baseline all three I/O strategies with the same queue/controller/SRC policy and workload; additionally test stock miniaudio duplex as a diagnostic baseline. Randomize order and repeat trials. Record silence/startup separately from steady state, retain failed configurations/HRESULTs, and distinguish API-reported, initialized, started and signal-verified results.

Numeric limits below are proposed research gates unless already accepted by AudioHole. They do not redefine D-030/D-034 or promise supported hardware behavior.

## E2 — native versus miniaudio attributable latency

1. **Objective/hypothesis:** determine whether adapter/copy/period differences materially affect latency; equivalent actual native settings may erase a presumed abstraction penalty.
2. **Environment:** common Windows protocol; wired interface with loopback measurement arrangement and baseline/reference path; A, B, C builds with matched actual periods/mode/SRC/MMCSS.
3. **Method:** route impulses/MLS or repeated bounded markers through each path, measure reference and routed paths using the same acquisition chain, cross-correlate, randomize trials; test default and variable-block miniaudio settings separately.
4. **Instrumentation:** shared-clock reference capture where available, external loopback/correlation tool, QPC queue/callback tracing, ETW scheduling, format/period initialization records.
5. **Outputs:** total loop latency ms, attributable paired difference ms with uncertainty, p50/p95/p99/max across markers, software occupancy frames/ms, lost frames and marker outliers.
6. **Success criteria:** repeatable signal verification and explainable latency budget; evaluate ≤10 ms attributable objective only for declared operating conditions. Require uncertainty small enough to distinguish the configurations being compared; no arbitrary universal tolerance.
7. **Confounders:** ADC/DAC latency, APOs, Windows SRC, baseline path, clock alignment, start transient, buffers rounded differently, power/load.
8. **Limits:** baseline subtraction assumes stable common transport; total physical loopback delay alone does not establish AudioHole-attributable delay or wireless suitability.

## E3 — CPU, allocation and scaling comparison

1. **Objective/hypothesis:** quantify per-frame dispatch/copy/conversion cost, optional adapter cost and multi-route worker overhead.
2. **Environment:** matched E2 Windows setup, 1/4/8 holes, simplex and duplex, stereo and required multichannel cases; release builds, same DSP/controller.
3. **Method:** run identical deterministic input for fixed durations, warm up, record idle baseline; separately vary representation/rate/channel mismatch; test steady state and recovery bursts.
4. **Instrumentation:** ETW CPU/context-switch traces, QPC blocks, OS thread CPU time, allocation counters, private bytes/working set, processing allocation hooks.
5. **Outputs:** CPU % with core normalization, ns/frame/channel, callback p99/max µs, switches/s, heap allocations/s, bytes and queue occupancy.
6. **Success criteria:** no steady-state heap allocation or blocking application callback operations in configured built-in paths; no unexplained deadline misses; select overhead differences only when repeatable above noise. No fixed CPU budget established by research.
7. **Confounders:** compiler/SIMD/alignment, CPU frequency, thread count/topology, clock controller updates, logging, debugger, power policy, format differences.
8. **Limits:** a component microbenchmark does not predict whole-route latency or driver scheduling reliability.

## E4 — independent-clock long-duration routing

1. **Objective/hypothesis:** stock fixed-ratio duplex eventually exhibits drift loss on sufficiently mismatched clocks; destination-paced compensation bounds occupancy without routine drop/duplication.
2. **Environment:** two independent physical devices, each direction separately and together; identical nominal 48 kHz plus different 44.1/48 kHz cases; known same-interface pair as comparison only after verifying clock relation.
3. **Method:** measure actual relative rate from valid position/time slopes; run at least 8 hours per representative setup; log occupancy and SRC ratio. Include deterministic simulated ±50/100/300 ppm tests as separately labeled software evidence.
4. **Instrumentation:** native packet/clock/QPC timestamps with validity/generation, occupancy histories, discontinuity/lost-frame counters, capture/render sequence markers, non-real-time trace consumer.
5. **Outputs:** ppm, occupancy min/max/p99 frames/ms, underruns/overruns/discards, compensation ratio/slew, recovery events and signal artifacts.
6. **Success criteria:** no monotonic occupancy runaway or unexplained steady-state losses in declared valid-clock runs; queue remains within the predeclared safety envelope and correction matches observed drift within measurement uncertainty.
7. **Confounders:** thermal drift, scheduler bursts, timestamps from different units/clock domains, USB power state, virtual-device clock behavior and output silence.
8. **Limits:** 8 hours is not proof of indefinite stability; nominal-rate equality or a flat short trace does not prove shared clocking.

## E5 — resampling quality, rate updates and numerics

1. **Objective/hypothesis:** establish whether linear+LPF meets quality at near-unity and large ratios; investigate phase wrap and unity-crossing artifacts before adaptive use.
2. **Environment:** processing-only deterministic signals in this workspace, then Windows callback environment; f32/s16, LPF orders 0/1/2/4/8, 44.1↔48 and 48↔96 kHz plus ppm modulation; validated high-quality reference implementation separately authorized if needed.
3. **Method:** impulse/swept sine/multitone/DC and full-scale stress; analyze magnitude/phase, alias tones and residual; split random block sizes, step/slew rate through unity, test finite extreme configs/zero rate/overflow boundaries. Preserve baseline defects; test any repairs on a separate research copy only if authorized.
4. **Instrumentation:** FFT/correlation, state/count traces, ASan/UBSan where available, allocation hooks and CPU timer.
5. **Outputs:** gain/alias/residual dB, THD+N where method valid, impulse/group delay frames/ms by frequency, phase/count error, ratio granularity ppm, ns/frame, NaN/Inf incidence.
6. **Success criteria:** no memory contract violations, bounded valid state, consistent counts across block partitions and continuous phase within chosen numeric tolerance. Quality thresholds must be agreed before selecting a production SRC; no invented target here.
7. **Confounders:** reference filter delay/alignment, window leakage, input bandwidth, scalar/SIMD differences, integer quantization, exact-unity LPF bypass.
8. **Limits:** spectral software tests are not listening/hardware tests; library latency helper is not an exact IIR group delay.

## E6 — starvation, overrun and stop responsiveness

1. **Objective/hypothesis:** classify stock duplex recovery versus bounded nonblocking application policies and assess whether a source stall can starve render.
2. **Environment:** isolated Windows route under controlled CPU/worker delays; shared/exclusive, explicit and default devices, one route plus unaffected control route.
3. **Method:** inject bounded test delays outside production, pause capture delivery, saturate queue, then resume; request Stop mid-wait and mid-recovery. Trace source discontinuity and each submitted frame.
4. **Instrumentation:** packet flags/timestamps, callback deadlines, queue telemetry, Stop request/completion QPC, Windows error/stage logging.
5. **Outputs:** underruns/overruns, packets/frames discarded, silence duration ms, recovery duration ms, stop latency ms, unrelated route glitches.
6. **Success criteria:** no indefinite wait, no deadlock, no callback enumeration/blocking policy work, correct explicit Stop disarm, loss accounted rather than hidden. Predeclare acceptable stop/recovery budget before test.
7. **Confounders:** injected delay location, driver buffer behavior, debug logging, Windows event coalescing and legacy versus Client3 period.
8. **Limits:** fault injection shows response to the selected fault, not every hardware invalidation pattern.

## E7 — endpoint identity and recovery stress

1. **Objective/hypothesis:** evaluate explicit-endpoint rebind and default migration separately; detect lifecycle leaks and wrong-device substitution.
2. **Environment:** Windows USB/virtual endpoints, saved logical fingerprint/binding fixtures in research only; repeated disconnect/reconnect, default-role changes, disable/enable, sleep/wake, service interruption and exclusive contention where testable.
3. **Method:** perform a scripted/manual sequence for ≥100 cycles, record native IDs/fingerprints and state transitions; exercise Stop during retry and missing/ambiguous bindings; compare default auto-routing with supervisor-owned explicit bindings.
4. **Instrumentation:** native notifications, HRESULT/API stage, handle/COM-resource counters where possible, private bytes trends, thread counts, supervisor generations/commands.
5. **Outputs:** recovery success/failure counts, time ms, bytes/handles/threads per cycle, selected endpoint identity, retries after Stop, duplicate notifications.
6. **Success criteria:** no arbitrary endpoint substitution or stale restart after Stop; configuration preserved; no sustained unexplained resource growth; failure states explicit and reproducible.
7. **Confounders:** driver caches, process working-set retention, services restarting independently, role versus flow notifications, device ID changes.
8. **Limits:** private-byte growth alone does not identify a COM leak; source already documents an abandoned-reference risk, runtime attribution needs stronger instrumentation.

## E8 — conversion, channel mapping and bypass cost

1. **Objective/hypothesis:** quantify when compatible native/client formats avoid work; validate output contracts for all selected paths, especially packed s24 shuffle.
2. **Environment:** offline processing and matched Windows streams; u8/s16/packed-s24/s32/f32, aligned/unaligned buffers, mono/stereo/required multichannel maps.
3. **Method:** bit patterns, impulses per channel, full-scale/quiet data, scalar/vector and odd tail lengths, randomized maps/block sizes; compare exact bypass, format-only, channel-only, SRC-only and composite paths. Keep s24 defect witness separate from correctness suite.
4. **Instrumentation:** canary boundaries/sanitizers, reference arithmetic, execution-path/format flags, CPU timer/allocation counters.
5. **Outputs:** mismatched samples/bytes, peak/rms error LSB or dBFS, cross-channel leakage, clipped samples, guard writes, ns/sample and allocated bytes.
6. **Success criteria:** exact passthrough bytes, correct map semantics, no output-extent writes, documented quantization/range policy and no unintended creative DSP; improvements accepted only if repeatable.
7. **Confounders:** dithering randomness/global state, SIMD alignment/tail, valid bits versus container, custom-weight/mono path precedence, Windows SRC outside harness.
8. **Limits:** matching format does not remove adaptive SRC or prove sample-rate clock equality.

## E9 — hybrid processing-only integration

1. **Objective/hypothesis:** assess B's public-component integration burden and whether native timestamp/format control justifies it.
2. **Environment:** Windows MSVC/clang-cl x64 isolated CMake harness, direct event-driven WASAPI, selected miniaudio processing APIs only, same queue/controller as A/C comparison.
3. **Method:** build one implementation TU and RAII wrappers, initialize preallocated converters/rings, exercise differing formats/rates, hotplug and errors without private-struct mutation; test required component repairs/alternatives only after review.
4. **Instrumentation:** compiler/link logs, component ownership/allocation audit, native HRESULT/timestamps, queue telemetry and E2/E3 measures.
5. **Outputs:** build results, binary size bytes, allocations/s, ownership errors, output correctness, latency/CPU, original integration code/review complexity.
6. **Success criteria:** no hidden device backend/codec dependency, clean teardown, correct frame/count contracts, same functional gates as A/C; complexity evaluated with actual code and review findings.
7. **Confounders:** duplicated native helpers, wrapper design, build macro inconsistencies, unvalidated resampler changes.
8. **Limits:** Linux E1 compilation establishes only a starting point, not Windows ABI correctness or production-readiness.

## E10 — format/period compatibility matrix

1. **Objective/hypothesis:** native candidate selection may expose usable formats/periods stock initialization cannot choose; off-grid Client3 requests may change fallback.
2. **Environment:** common Windows devices, modes, native mix rate and mismatched requested rates, actual fundamental/min/max periods; bounded authorized isolated probing.
3. **Method:** query reported constraints, test legal values and surrounding off-grid requests, initialize/start/validate signal separately, capture raw result/stage; compare noAutoConvertSRC and default Windows SRC with same intended route format.
4. **Instrumentation:** exact WAVEFORMATEXTENSIBLE fields, Client3/legacy path trace, GetBufferSize, QPC cadence, HRESULTs, E2 signal validation.
5. **Outputs:** reported/accepted/started/verified matrix, periods/capacities frames/ms, fallback count, errors and conversion locations.
6. **Success criteria:** all labels reflect actual evidence; no silent mode/endpoint substitution; negotiated latency consequences traceable to chosen path.
7. **Confounders:** engine locked by other client, exclusive access, driver changes, Windows SRC/APOs, backend format-parser limits.
8. **Limits:** initialization success is not native hardware support or long-run stability; unsupported under one environment is not universal hardware impossibility.

## E11 — Windows-managed shared-render adjustment versus application SRC

1. **Objective/hypothesis:** compare compensation location, continuity/quality and period compatibility, without presuming either actuator superior. Include unchanged transfer only where evidence supports it; no unqualified bit-perfect claim.
2. **Environment:** current Windows protocol, independent capture/render devices, shared-render legacy event initialization with RATEADJUST/GetService; compare Client3 actual supported flags/periods and application-managed established SRC; exclusive uses applicable alternatives only.
3. **Method:** fixed nominal match and controlled drift/jitter, initialization-time strategy selection versus experimental dynamic transition, rate commands off the real-time thread, one engine/per-direction preferences. Probe supported flags and record failures instead of silently changing modes.
4. **Instrumentation:** exact initialization/HRESULT/service path, device/QPC/occupancy traces, rate-command issue/effect times, callback deadlines, signal correlation/FFT, external latency reference.
5. **Outputs:** supported combinations, period/capacity frames/ms, actual adjustment frames/s/ppm, queue bounds, discontinuities, rate-transition artifacts dB, CPU/frame and attributable delay ms.
6. **Success criteria:** bounded queues/no unexplained loss under declared drift envelope, non-real-time setter, honest negotiated/fallback reporting, no silent compensation in internal strict preservation. Quality/delay thresholds and final O-005 response policy remain open until comparative evidence.
7. **Confounders:** Client3 may reject legacy flags; engine APO/SRC behavior, timestamp validity, period differences, control latency and hardware clocks. Record non-equivalence explicitly.
8. **Limits:** Microsoft API availability is not driver compatibility; accepted RATEADJUST does not prove sample identity, capture/exclusive support or performance. Not executed here.
