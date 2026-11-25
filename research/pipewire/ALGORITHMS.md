# Algorithm ledger and numerical analysis

This ledger complements the full reconstructions in CLOCK_SYNC.md and RESAMPLING.md. Units, acquisition, formulas, and edge handling are part of each algorithm's contract. The pinned path uses feedback from delay/timing; locating an unrelated algorithm elsewhere would not establish its use here.

## A1 — Delay observation and normalization

- **Purpose:** obtain an observation comparable at a graph processing phase rather than mistake time-of-read differences for clock drift.
- **Verified inputs:** ALSA available frames from the PCM handle; optional hardware timestamp and associated availability; logical processing time; node rate and buffer size.
- **Steps:** timestamp extrapolation `Δ=trunc((t−t_h)F/1e9)`; reject implausible `|Δ|≥3T`; clamp valid adjustment to ±T; convert availability to playback queued delay or capture readable delay; clamp target to configured node limits. Exact equations and direction signs are in CLOCK_SYNC.md.
- **Output:** observed delay, target, read capacity, and converter delay in appropriate frame domains.
- **Dependencies:** ALSA/monotonic time for collection; arithmetic is platform-independent. WASAPI mapping requires packet-aware adaptation.
- **Limits:** discontinuities, stale timestamps, batch pointer updates, clamped observations, and availability recovery fallbacks can corrupt drift inference. Timestamp extrapolation is optional and disabled by default in the examined ALSA initialization.
- **Applicability:** **Adapt** with valid timestamp flags, stream generations, observation-phase normalization, and bounded stale-data handling. [S02](SOURCE_REFERENCES.md#s02), [W01–W04](SOURCE_REFERENCES.md#w01)

## A2 — Three-state DLL

- **Purpose:** turn signed frame error into a slowly changing dimensionless timing/rate multiplier.
- **Verified inputs/outputs:** `e` frames; `T` frames per update, `F` frames/s, bandwidth `b`; double states and output `c`. Coefficients and ordered updates are in CLOCK_SYNC.md; initialization is required before use.
- **Inference:** noise is reduced by two smoothing states, and the accumulator can retain frequency offset with zero mean phase/delay error. This is a closed-loop property, not a guarantee of the helper in isolation.
- **Limits:** standalone output has no positivity check, universal ppm clamp, anti-windup, or NaN recovery. A constant bad observation can make the integrator grow. Coefficients presuppose a nonzero period/rate and an update cadence consistent with them.
- **Performance:** constant-size state and arithmetic per update; `exp()` runs when coefficients change rather than per sample.
- **Dependencies:** math functions and floating point; no Linux API in the recurrence.
- **Applicability:** **Adapt**, validating actuator sign, update period, saturation policy, and response against a simpler PI alternative. [S01–S02](SOURCE_REFERENCES.md#s01)

## A3 — Adaptive bandwidth statistics

- **Purpose:** select tracking bandwidth according to recent error magnitude and variability.
- **Verified steps:** source mean/variance-like updates, 3-second trigger, and **0.001–0.128** clamp as given in CLOCK_SYNC.md; bandwidth is passed directly to the coefficient formula.
- **Acquisition:** same clipped error sent to the DLL; `W=F_g/Q` represents updates per nominal second. Updates occur on controller-active paths. A DLL reset alone does not explicitly zero `err_avg` and `err_var` at every reset call site.
- **Limitations:** mean recurrence differs from a standard EWMA. At constant error, its stationary mean solves `m=e/2`, which demonstrates why replacing it with a textbook average would change the algorithm. The statistic and heuristic are not established as an optimal estimator.
- **Inference:** lower bandwidth rejects noise but may retain occupancy error longer after rate changes. Tested model results agree with that tradeoff, not with a universal performance ranking.
- **Applicability:** **Investigate Further**; fixed bandwidth is a useful initial comparison with fewer moving parameters. [S02](SOURCE_REFERENCES.md#s02), [EXPERIMENTS.md](EXPERIMENTS.md)

## A4 — Rate actuation and phase-bank resampling

- **Purpose:** reconcile nominal and actual rate differences without periodic bulk frame drops during healthy tracking.
- **Verified steps:** construct multiplicative `r`, quantize effective input rate, carry bounded phase, interpolate neighboring filter-bank responses, maintain history, return exact frame counts and delay. Full formulas are in RESAMPLING.md.
- **Inputs:** negotiated nominal rates, CPU capabilities, channel count, quality, per-cycle correction, buffered history/phase, input/output capacities.
- **Output:** variable consumed/produced counts, float audio, residual history/phase, delay and size requirements.
- **Limits:** integer rate quantization, float phase and coefficients, finite filter cutoff, rate/input validation requirements, and explicit history reset. Source phase-delay comments identify unresolved one-frame behavior.
- **Dependencies:** algorithm is portable; compiler SIMD/backend integration is platform-specific. No full PipeWire runtime is intrinsically needed for equivalent independent DSP.
- **Applicability:** **Adapt** the contract and principles; source adoption requires separate approval. [S03–S06](SOURCE_REFERENCES.md#s03)

## A5 — Bounded ring transport

- **Purpose:** decouple producer/consumer timing with bounded memory and predictable copies.
- **Verified inputs/steps:** 32-bit monotonically wrapping read/write counters; producer owns write, consumer owns read. Acquire reads of peer index, release publication after data copy, relaxed reads of own index. Fill is signed modular difference; storage offset is index modulo capacity. Data-copy helpers split at the end into at most two copies.
- **Output:** signed occupancy and copied bytes. The ring structure itself has no size field; callers supply capacity and must enforce bounds.
- **Limits:** one producer and one consumer, distance less than 2^31, transfer no larger than storage, and quiescent resets. Atomic counters alone do not make a multi-consumer queue correct.
- **Dependencies:** source uses compiler `__atomic` builtins; a Windows C++ implementation can use appropriate `std::atomic` acquire/release operations after checking lock-free support for its types.
- **Applicability:** **Adopt** ownership/bounds principles and **Adapt** language/telemetry integration. [S07](SOURCE_REFERENCES.md#s07)

## A6 — Graph quantum and dependency scheduling

- **Purpose:** derive a processing cadence from active constraints and start a node only when its upstream dependencies are ready.
- **Verified inputs:** driver candidates/priorities, connectivity, active/runnable status, default/forced/locked rates and quantums, node latency/max latency, settings bounds.
- **Steps:** graph grouping → collect constraints → choose permitted rate → scale quantum constraints → translate latency fractions to frames → clamp → optionally floor to a power of two → publish target duration/rate. Per cycle, copy dependency required counts to pending; decrement on dependency completion; signal an eventfd when pending reaches zero and status transition succeeds. Check unfinished prior cycles for xruns.
- **Output:** driver cadence and node activations. This is dependency-based scheduling under the host scheduler, not evidence of an earliest-deadline-first engine.
- **Limits:** processing plus wake-up/communication cost must fit available deadline slack; asynchronous links can add a quantum. Negotiation policy is not continuous automatic buffer growth on every xrun.
- **Dependencies:** graph arithmetic and dependency model are portable; eventfd, Linux real-time priorities, and ALSA timers are not Windows APIs.
- **Applicability:** **Adopt** explicit cadence/deadline accounting; **Reject** reproducing the full distributed activation machinery for v0.1 without a demonstrated need. [S09–S13](SOURCE_REFERENCES.md#s09)

## Constants and provenance

| Constant / parameter | Actual use | Provenance established here |
|---|---|---|
| `2π bT/F`, `1−exp(−20ω)`, factors 20 and 1.5 | DLL coefficients | Formula verified; detailed derivation/tuning rationale **unknown**. |
| `.128`, helper `.016` | helper named bandwidth bounds | Hardcoded; ALSA begins at .128 and does not use helper .016 as its lower clamp. Rationale unknown. |
| `.001`, 1000, 3 seconds | ALSA bandwidth heuristic | Hardcoded; dimensional/error scaling and empirical basis not documented in inspected code. |
| 256 frames; half of `T+H` | maximum tracking error | Hardcoded plus cadence-dependent formula; rationale for 256 unknown. |
| ±T, 3T, 64 errors | htimestamp adjustment/rejection | Period-scaled gates; maximum bad-observation count is configurable. Detailed tuning rationale unknown. |
| 32 capture frames | minimum timer/resampling headroom | Source comment explicitly identifies capture jitter allowance; numeric derivation unknown. |
| one extra batch-device period | headroom and period policy | Source comment ties this to coarse pointer updates; hardware-dependent. |
| three IRQ periods | hardware buffer negotiation fallback | Comment identifies a FireWire workaround; not a universal latency optimum. |
| 10 of 1,000,000 pitch units | suppress hardware-control churn | Comment explicitly avoids granular control spam; hardware-control path only. |
| quality 4: 48, .85; qualities 0–14 | FIR length/cutoff tradeoff | Configurable quality and hardcoded table; per-entry rationale unknown. |
| A=16.97789; near-zero sinc 1e−6 | cosh window, sinc guard | Citation plus undocumented tweak for window; exact chosen numeric rationale unknown. |
| 8 taps, 64-byte alignment, ≥256 phases, max 2^18 taps | native filter layout/precision/bounds | SIMD and phase interpolation comments support intent; exact minima/max rationale otherwise unknown. |
| 48,000 Hz; 1024 default quantum, 32–2048 usual settings bounds, floor 4, limit 8192 | compiled graph defaults | Configurable defaults in settings.c; distribution/user overrides possible. Not recommended AudioHole latency settings. |

Source locations: S01–S02, S06, S13; details in SOURCE_REFERENCES.md. No tuning rationale is inferred solely because a number looks familiar from control theory.

## Numerical edge cases and long-term behavior

**Verified:** DLL arithmetic is double, samples/coefficient/phase storage are float in the native path, counters/timestamps use explicit integer widths, and `SPA_SCALE32_UP` widens multiplication to uint64 before ceil division. This avoids one class of intermediate 32-bit overflow for bounded inputs; it does not validate denominators or guarantee the result fits uint32. [S01, S03, S06, S07, S19.]

**Inferred / calculated consequences:**

- Ring wrap is routine: 2^32 bytes at 48 kHz stereo float32 (384,000 B/s) takes about **3.11 hours**. Correct modular distance and ownership make wrap harmless within the <2^31 distance invariant; replacing it with ordinary signed subtraction would be unsafe. A local wrap check executed the original helpers.
- Per-cycle next-time conversion discards a fraction of a nanosecond for positive intervals. Worst-case open-loop loss would be <375 ns/s at 128/48 kHz; the real loop feeds back observations, so that bound is not a prediction of measured drift.
- Integer available-frame observations quantize phase error; timestamp extrapolation reduces observation-phase mismatch but adds driver timestamp quality as a dependency.
- Rate changes need coefficient/phase review. In ALSA, coefficients are set on initialization and periodic bandwidth update, while a threshold change requests a resync. Do not assume every caller automatically makes coefficients valid for arbitrary variable callback sizes.
- Signed differences computed from unsigned timestamps/counters, casts, and the negative threshold-adjustment expression deserve portability review. The small header checks do not exercise the entire ALSA C translation unit or prove those edge paths safe on Windows.
- The helper does not bound `c`; downstream reciprocal and integer conversions assume a finite positive reasonable correction. An AudioHole adaptation should add explicit input validity, stale-data gating, correction bounds, and an anti-windup/reset policy, then test the combined nonlinear system.
- Accumulated integration may retain old clock estimates after a stream replacement unless reset. A new generation must invalidate previous clock observations. Do not compare stream-relative positions across Reset or device reopen.

## Local stability calculation, with its assumptions

**Experimental mathematical model:** fixed period, no scheduling jitter, no clipping/resync, ideal rate actuator, and error dynamics

\[
e_{k+1}=e_k+T(c_k-(1+\delta)).
\]

Let `A=1−a` and state be `(e,z1,z2,z3)` before each update. Rows for the updated states are:

\[
v_1=(au,A,0,0),\quad v_2=(a^2u,aA,A,0),\quad
v_3=(0,0,0,1)+v v_2,
\]
\[
v_e=(1,0,0,0)-T(v_2+v_3).
\]

The 4×4 transition matrix has rows `(v_e,v_1,v_2,v_3)`. The DLL itself has three states; closing the loop adds the queue state. At T=128/F=48 kHz and b=.128, computed poles are approximately `.94822096`, `.97134280`, and `.99822897±.00152574i`, all inside the unit circle. At b=.001 the slow pair is approximately `.99998616±.0000119638i`. See `experiments/numerics.py` and its generated JSON for reproducibility and NumPy version.

This supports local stability only for that linear fixed-bandwidth ideal model. It does not prove stability under delayed observations, adaptive gain switching, resampler quantization, saturation, arbitrary periods, device loss, or audible signal constraints. Hardware and nonlinear comparisons remain necessary.
