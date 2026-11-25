# Recommendations for subsequent engineering evaluation

Publication note: [CURRENT_BASELINE.md](CURRENT_BASELINE.md) records the newer remote `work` revision and supersedes historical AudioHole status/policy statements below. PipeWire source findings retain their pinned upstream baseline.

These are research classifications, not accepted changes to AudioHole's decisions register. Evidence identifiers resolve in SOURCE_REFERENCES.md. No production dependency or backend is selected.

| Finding / technique | Classification | Reason, cost, and next validation |
|---|---|---|
| Treat independent clocks explicitly, including equal nominal formats | **Adopt** | Already required by baseline; source confirms a nonunity actuator may still be needed. Low conceptual cost; avoids gradual queue failure. Determine actual backend clock domain evidence. [S02–S03] |
| One controller/ASRC state per independent destination/direction | **Adopt** principle | Compatible with duplex independence and mirrors. Small bounded state; actual sample processing scales with channels and taps. Stream sharing must preserve separate cursors and avoid double compensation. [S02–S06] |
| Exact frame-count and delay interfaces | **Adopt** | Avoids fixed-block assumptions and missing filter latency. Modest backend API complexity; expose units and timestamp quality. [S03–S05] |
| Bounded preallocation, acquire/release ownership, compact diagnostics | **Adopt** | Supports existing real-time safety requirements. Predictable memory/copies; caller still owns capacity/overflow checks and lock-free atomic validation. [S06–S07, S12] |
| DLL with occupancy feedback | **Adapt** | O(1) controller, suitable research candidate; gain/cadence/actuator sign and recovery need Windows validation. Compare a simpler bounded PI before deciding. No measured superiority here. [S01–S02] |
| Destination-paced processing | **Adapt** | Fits output readiness and independent capture; queue introduces delay and cross-thread cost. Test single-worker versus split-worker designs under actual event cadence. Current `work` accepts destination pacing; worker layout and tuning remain experimental. See [baseline update](CURRENT_BASELINE.md). [S02, S09–S10; W04–W07] |
| Timestamp normalization | **Adapt** | Can reduce packet-phase noise and expose drift/stalls, but depends on unit/flag quality. Shared capture backlog cannot be replaced by padding. [S02; W01–W04] |
| Separate discontinuity reset/re-prime from drift correction | **Adapt** | Prevents slow-loop saturation after loss. Some sample discontinuity can be unavoidable; specify audible behavior and cancellation. Do not copy ALSA blocking suspend recovery or assume playback rewind exists on WASAPI. [S02] |
| Combined nominal conversion plus adaptive ratio | **Adapt** | Potentially saves a second FIR and its delay/CPU. Needs a suitable ASRC with exact counts, phase continuity, correction precision, and licensing review. [S05–S06] |
| ALSA adaptive bandwidth heuristic | **Investigate Further** | Extra state and undocumented coefficients; observed model slows after a rate step. Fixed gain is simpler and must be the baseline comparator. [S02; EXPERIMENTS.md] |
| Native rate quantization and float phase | **Investigate Further** | Actual ~20.8 ppm steps at 48 kHz deserve spectral/limit-cycle tests. Better fractional precision may improve behavior, but no audible defect or superior replacement is established. [S06] |
| OS `SetSampleRate` as actuator | **Investigate Further** | Shared-mode and non-real-time thread restrictions; may duplicate conversion. Compare to application ASRC with explicit timing, allowed ranges, and latency. [W09] |
| Power-of-two quantum policy | **Reject** as a Windows rule | Windows periods have fundamental-multiple constraints; enforcing powers of two can reject supported useful periods. Keep only if a DSP implementation separately benefits and API constraints allow it. [S10, S13; W05] |
| General server graph / remote-node activation infrastructure | **Reject** for v0.1 scope | Additional lifecycle/IPC/policy and maintenance cost; no demonstrated benefit for 1–8 holes. Retain dependency/deadline accounting concepts. [S09–S12, S17] |
| Linux timerfd/SCHED_FIFO/RTKit as dependencies | **Reject** | Use Windows event/MMCSS facilities; numeric priority translation is not meaningful. [S02, S14; W07–W08] |
| PipeWire/ALSA source incorporation as part of this assignment | **Reject** in current scope | Assignment explicitly disallows production copying/dependencies; licensing differs across components. Independently evaluate principles first. Any source adaptation is a separate approved task. [S18] |

## Simpler alternatives worth comparing

**Experimental comparisons, no ranking established:**

- **Fixed ratio plus large queue:** simplest but finite capacity always fails eventually under persistent rate mismatch. Use only as a negative control, not a solution to independent-clock drift.
- **Occasional frame insertion/removal:** minimal DSP cost but discontinuities/artifacts are possible; no evidence here that it satisfies AudioHole audio quality. Keep as a recovery policy experiment, not the preferred healthy tracking strategy.
- **Bounded PI occupancy control:** fewer states/tuning terms than the observed DLL plus bandwidth heuristic. Must validate noisy packet observations, correction saturation, delayed actuation, and integrator windup.
- **Fixed-bandwidth DLL:** small and directly reconstructable, with fewer moving parameters than adaptive bandwidth; compare acquisition, jitter rejection and drift-step response.
- **Timestamp feed-forward plus occupancy feedback:** may track changing rate sooner but adds estimator and timestamp validation complexity. Appropriate only if evidence improves latency/stability sufficiently.
- **Backend-owned conversion/compensation:** potentially least integration effort; require telemetry, frame-count behavior, quality, and recovery evidence before introducing redundant ASRC/controllers.

## Priority engineering questions

1. **D-030 / milestone 1.1:** determine backend telemetry and event behavior on representative Windows endpoints; compare native/miniaudio/hybrid with the same instrumentation.
2. **D-034 / milestone 1.3:** compare one stateful ASRC and fixed DLL against simpler PI and backend compensation in a bounded route harness. Include equal nominal rates and small mismatches.
3. **D-035:** derive queue targets from measured event/packet jitter and actual periods while respecting attributable-latency accounting. Capacity and target are separate knobs.
4. Validate duplex and mirrored directions, endpoint replacement, explicit Stop, suspend/resume, and per-route fault isolation before any production promotion.
5. Complete per-component licensing compatibility once AudioHole's own license is selected. The current baseline README leaves that license open.

The next concrete task should be an isolated **Windows timing/queue observability prototype**, followed by the controller comparisons specified in EXPERIMENTS.md. It can inform existing decisions without replacing the design or committing to an upstream dependency.
