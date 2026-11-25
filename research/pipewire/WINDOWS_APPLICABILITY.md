# Windows applicability

Publication note: [CURRENT_BASELINE.md](CURRENT_BASELINE.md) records the newer remote `work` revision and supersedes historical AudioHole status/policy statements below. PipeWire source findings retain their pinned upstream baseline.

The controller arithmetic, phase accumulation, bounded queues, and ownership rules can operate entirely in user mode. PipeWire's ALSA handles, timerfd/eventfd, Linux scheduling policy, and session-manager infrastructure are platform-specific. This report proposes Windows equivalents without selecting D-030 or D-034.

## API contracts verified from Microsoft documentation

| API / facility | Verified contract | Consequence |
|---|---|---|
| `IAudioClock::GetPosition`, `GetFrequency` | Position units must be interpreted using that stream's frequency; `p/f` is stream-relative seconds. Frequency ignores small actual drift. GetPosition pairs position with QPC converted to 100 ns units. Exclusive-mode `S_FALSE` can indicate a lower-quality observation. [W01–W02](SOURCE_REFERENCES.md#w01) | Do not equate every raw position tick to a frame or infer real oscillator frequency from GetFrequency alone. Record HRESULT and correlated timestamp quality. |
| `IAudioCaptureClient::GetBuffer` | Packet position is in **audio frames** from stream start, paired with 100 ns QPC time for its first frame. Flags identify silence, discontinuity, and timestamp error; empty packets and exact acquire/release pairing matter. [W03](SOURCE_REFERENCES.md#w03) | Capture packet positions and IAudioClock positions have different unit contracts. Reject invalid timing for estimation without treating silence as device loss. |
| `IAudioClient::GetCurrentPadding` | Shared render padding is queued endpoint frames; free space is capacity minus padding. Shared capture padding is the **next packet length**, equivalent to GetNextPacketSize, not total available captured frames. Exclusive event-driven processing generally accesses a full endpoint buffer per event. [W04](SOURCE_REFERENCES.md#w04) | Do not substitute capture padding directly for ALSA total readable frames; track application queue backlog independently. |
| `IAudioClient3::GetSharedModeEnginePeriod`, `InitializeSharedAudioStream` | Supported periods depend on format/client properties; chosen frames must be a fundamental-period multiple within min/max. Initialization can fail due to unsupported format, conflicts, or invalid periodicity. [W05–W06](SOURCE_REFERENCES.md#w05) | Period requests are constraints to negotiate, not latency guarantees. Query actual stream configuration and preserve HRESULTs. |
| `IAudioClient::Initialize` and event callbacks | Event-driven streams require the corresponding flag and event handle setup. Exclusive event-driven buffer duration and periodicity have matching requirements, plus supported alignment/format constraints. [W07](SOURCE_REFERENCES.md#w07) | Shared/exclusive initialization and buffer-service rules cannot be collapsed into one assumption. |
| `AvSetMmThreadCharacteristicsW` | Associates a worker thread with a configured MMCSS task; it returns a handle or an error. [W08](SOURCE_REFERENCES.md#w08) | Use a suitable available multimedia task and restore/revert registration at teardown; no guarantee of zero deadline misses follows from registration. |
| `IAudioClockAdjustment::SetSampleRate` | Shared-mode only, may reject rates, and must not be called on a real-time processing thread. [W09](SOURCE_REFERENCES.md#w09) | Investigate as a separate actuator with appropriate control-thread dispatch; it is not a universal per-render-callback correction API or proof of changing physical hardware frequency. |

No Kernel Streaming or custom driver is required for these proposed mechanisms. Direct KS would require a separate compatibility/maintenance study and is not justified by these findings. Endpoint/engine/driver timing differences and shared-mode conversion still have to be measured.

## Mechanism-by-mechanism transfer

| Technique | Classification | Windows equivalent / changed assumption | Driver / overhead / compatibility |
|---|---|---|---|
| Single pacing reference for a route | **Adapt** | Destination render readiness can define output work; capture remains an independent producer. | User mode; event/queue handoff overhead must be measured. Destination pacing is a proposed policy, not an accepted baseline decision. |
| DLL delay feedback | **Adapt** | Same recurrence with a valid signed error in source-frame units and an explicit consumption-ratio contract. | No driver; O(1) control work per update. Must tune for Windows packet cadence and observation delay. |
| Timestamp normalization | **Adapt** | Correlated QPC/clock or capture-packet positions; extrapolate only valid bounded observations to a common instant. | User mode; API call cost and driver timestamp quality vary. |
| RateMatch size/delay exchange | **Adopt** underlying interface principle | Backend reports exact frames consumed/produced, history delay, timestamp domain, discontinuity generation. | No driver; fits replaceable native/miniaudio/hybrid backend interface. |
| Stateful fractional resampling | **Adapt** | One ASRC per independent boundary, nominal conversion and drift in a combined ratio. | No driver; taps/channels/ratio determine CPU and delay; candidate not selected. |
| Optional hardware pitch element | **Investigate Further** | No universal WASAPI physical-clock pitch equivalent established. OS rate adjustment is a stream-level facility with restrictions. | Do not assume exclusive mode or arbitrary drivers can expose this actuator. |
| Bounded producer/consumer ring | **Adopt** principles | C++ atomic indices and frame-aligned storage, one producer/consumer ownership, explicit overflow policy. | User mode; bounded copies and cache traffic. Mirrors require multi-reader design or separate queues. |
| timerfd/eventfd | **Adapt** | WASAPI event handles and bounded waits/control notifications; use actual endpoint periods. | User mode; Windows scheduler/driver delay is an external constraint, not removed by translating the algorithm. |
| SCHED_FIFO / RTKit | **Adapt** intent | MMCSS worker registration with measured callback deadlines. | No custom driver; not numerically equivalent priority scales. Normal-user operation remains required. |
| Discard/rewind/silence resync | **Adapt** | Application queue discard, silence/re-prime, and stream lifecycle outside callbacks. No universal WASAPI rewind of submitted render audio. | Bounded discontinuities may be audible; report inserted/dropped frames, keep unrelated directions alive. |
| Full graph/session/remote-node machinery | **Reject** for present need | A few independent in-process directional workers and queues suffice as a prototype. | Avoid dependency, IPC, shared-memory, policy, and maintenance costs unless measured need emerges. |

## Proposed ratio and estimator contracts

**Experimental:** use one unambiguous actuator:

\[
r_k = \text{source frames consumed per destination frame},\qquad
r_{nominal}=F_s/F_d.
\]

For normalized source/destination rates `α_s,α_d` relative to a common QPC reference:

\[
r_{feedforward}=\frac{F_s\alpha_s}{F_d\alpha_d}.
\]

For `IAudioClock` readings `(p_i,q_i)` with frequency `f`:

\[
\alpha\approx\frac{(p_2-p_1)/f}{(q_2-q_1)\,10^{-7}}.
\]

Capture-packet positions instead use frame counts, so their frame-rate estimate is `Δframes/(Δq×1e−7)`. These are proposed finite-difference estimators, **not algorithms established in the inspected ALSA path**. A robust/windowed fit is a comparison experiment, not an assertion that PipeWire uses one.

Subtract integer positions/timestamps before converting to floating point; avoid overflow-prone `rawQPC×10,000,000` using a safe widened or quotient/remainder conversion. Keep monotonic elapsed time distinct from UTC. Never compare raw QPC ticks to already-converted 100 ns timestamps. No slope estimate is valid across stream Reset/reopen, invalid timestamp flags, a long stall, nonmonotonic observations, or a generation change. Same nominal rate, endpoint name, physical enclosure, or logical device identifier does not establish a common timing domain. [W01–W03; baseline clock/device-identity requirements.]

At render time, let source queue `q` and target `q*` be sampled before consumption. If `q>q*`, feedback must **increase** `r` to consume more source frames; if low, reduce it within allowed bounds. A timestamp feed-forward term can reduce acquisition delay while occupancy feedback removes residual error. It can also inject timestamp noise or duplicate compensation already performed by a backend. Start with one controller/actuator owner, and compare this more complex approach to occupancy-only DLL or PI.

## Timing observation and worker design

**Experimental sequence:**

1. Prepare endpoints, event handles, negotiated formats/periods, queues, ASRC history, and worker control messages outside audio processing. Prime explicitly and publish a stream generation.
2. Capture worker reads available whole packets, preserves timing/flags, copies into its owned queue, and releases packet buffers promptly. Use a finite per-wake work bound consistent with negotiated maximum backlog; reaching it records lag and schedules recovery rather than spinning forever.
3. Destination worker wakes, determines the mode-specific writable-frame requirement, takes a consistent queue/timing snapshot, and rejects invalid/stale observations for drift updates. Packet batching causes occupancy sawtooth: normalize phase or filter it before changing rate.
4. Controller updates at a declared cadence; variable destination demands must not silently be treated as a fixed DLL period. Render uses the exact required output count and records actual ASRC consumption, residual filter state, and underruns.
5. On mild shortage, follow a defined bounded silence policy; on large discontinuity or invalidated endpoint, publish a compact event and let the supervisor reset/reopen/re-prime outside the callback. No unbounded retries, allocation, enumeration, or filesystem logging on workers.
6. Explicit Stop invalidates queued recovery/activation work and previous generations. Duplex directions and mirrored outputs retain independent timing and failure state.

Actual render/capture event semantics, loopback endpoints, Bluetooth transport behavior, sleep/wake, format negotiation and endpoint ownership must be validated on Windows. A correct algorithm cannot create samples that a stalled capture endpoint did not deliver or remove latency already present in hardware/transport.

## Backend evaluation implications

Native WASAPI, miniaudio-centric, and hybrid prototypes should expose equivalent telemetry: period/capacity, packet sizes, timestamps and quality flags, queue target/min/max, ASRC ratio and exact counts, callback lateness/execution, HRESULTs, discontinuity/drop counts, and stream generation. Check whether a backend already compensates clocks before adding another loop. If its abstraction hides necessary timing, either extend the diagnostic boundary or test a native path; this research does not prove native is faster.

Unselected additions remain experiments. No Windows hardware was available in this Linux environment; extra Windows latency, CPU cost, endpoint compatibility, permissible ratio bounds, and audible correction thresholds remain unmeasured.
