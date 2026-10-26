# Recommendations and unresolved decisions

Informational only; no change to AudioHole architecture, production dependencies, design files or milestone status. Evidence/commit: [README](README.md), [finding register](FINDINGS.md).

## Adopt principles

1. Record requested versus negotiated format, period, buffer capacity, conversion location and observed queue occupancy separately (F04–F06, F11). This directly supports AudioHole's evidence taxonomy and conditional latency target.
2. Use bounded, preallocated processing state and clear producer/consumer ownership (F10–F11, F16). Miniaudio's stateful converters and acquire/commit ring illustrate mechanisms; API choice remains reviewed separately.
3. Preserve logical endpoint identity outside the backend; treat native strings as opaque machine-local bindings (F14). Backend switching must not change saved logical assignments or silently replace endpoints.
4. Measure before selecting backend (F17). Equivalent Windows API facilities exist in miniaudio; direct access is not a demonstrated latency win.

## Adapt mechanisms

1. **Destination-paced independent workers:** drain capture promptly, queue per destination, render on destination demand. Keep a separate drift controller/actuator and diagnostics (F01–F03). This better isolates source stalls and supports mirrored destinations, but needs prototype evidence.
2. **Period negotiation:** query actual Client3 constraints and select an explicit legal period. Log fallback and actual capacity; compare against miniaudio's floor/clamp/guard behavior (F04–F05).
3. **Reusable conversion dispatch:** avoid format/channel operations when truly compatible; reduce channels before SRC and expand afterward where semantics permit (F09–F11). Keep adaptive SRC available even for equal nominal rates.
4. **Scheduling:** explicitly select and verify MMCSS task when desired, preserve bounded callbacks, make logs nonblocking, and record native priority separately (F12).
5. **Recovery:** enqueue endpoint changes and do teardown/rebind/open on a lifecycle worker. Explicit IDs, durable matching, cancellation and configured fallbacks belong to AudioHole (F14–F15).

## Reject specific assumptions or paths

- Stock WASAPI duplex as a complete arbitrary-clock synchronizer: nominal conversion and post-glitch packet discard are not a continuous drift controller (F01–F03).
- Generic `ma_resampler_set_rate_ratio`/converter ratio convenience API for ppm corrections at this revision (F07).
- Unmodified packed-s24 shuffle component branch: the processing probe demonstrates corrupted logical output and a write beyond its declared extent (F18).
- Whole-library device info/recovery as AudioHole's portable registry/probing/supervision model (F14–F15).
- Default async ring tuning as a universal low-latency policy, or assigning its priming to the Windows path (F16).
- Any claim that matched nominal formats remove clock drift, direct WASAPI must be faster, or 10 ms is achieved from a requested period alone.

## Investigate further before dependency selection

1. Fine-ratio and phase continuity: independently review the phase wrap, finite/range validation and ratio granularity; compare custom/alternative actuators (F07–F09).
2. SRC quality near unity and over required large ratios; coefficient transitions at equality and integer/f32/vector conversion consistency (F09, F18).
3. Native versus library I/O with equivalent actual configuration and the same route queue/controller (all strategies).
4. Long-run clock stability, loaded-system scheduling, device invalidation/reconnection and repeated teardown resource balance (F02–F03, F12–F15).
5. Windows build/ABI, license inventory and total integration complexity (F17).

Detailed objectives, instruments, units, confounders and proposed acceptance criteria are in [EXPERIMENTS](EXPERIMENTS.md). Proposed thresholds are research gates, not edits to accepted requirements.

## Opportunities where the existing library may simplify AudioHole

Client3 discovery/fallback, aligned ring acquire/commit, stateful conversion counters, coefficient construction, format conversion dispatch and multiple Windows lifecycle edge cases already exist. Studying/reusing validated public components can reduce implementation and portability work. Converters deliberately avoid operations when static settings match; an abstraction is not necessarily a mandatory DSP stage.

## Opportunities for narrower native specialization

AudioHole can avoid a general media engine, decoders, creative DSP and resource manager. Its native stream layer can preserve raw HRESULT/timestamps, choose explicit candidate formats/periods, provide structured discontinuity evidence and defer notification work according to its supervisor policy. Separate native render demand and capture drainage can avoid the stock duplex blocking coupling and fixed callback adapter. The actual value of removing scratch copies is an open measurement, not a reason to redesign by itself.

## Open questions and confidence

High confidence: traced Windows execution path, exact defaults/guards, reproduced processing defects, and missing continuous drift control on that path. Limited confidence: device-specific recovery effects, audible artifacts, memory-growth rate, hardware-clock relationships, and performance—these need Windows tests. No gains or ratios for the proposed servo have been validated.

The next authorized research step is a Windows-only isolated comparison harness under this same directory, when suitable execution/hardware exists. Selecting a production dependency, adapting source or changing D-030/D-034 requires separate review; this report does not ask for or imply that adoption.

## Reconciliation with newer `work` before publication

The reviewed publication base is `766ed88f7d7786bfb90fb7a520da34a15de010d1`. Destination pacing/bounded FIFOs/adaptive synchronization are accepted; native WASAPI leads, without final selection. Evaluate Windows-managed shared-render adjustment alongside established application-managed SRC (F19/E11); preserve one engine and independently resolved directional preferences. The proposed PI equation is research only. No production gains/thresholds, public strict-preservation policy, or final direct-transfer/response contract is selected. The repository has foundation and simulated GUI code, not a live routing engine.
