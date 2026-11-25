# Backend architecture comparison

Evidence is pinned source and Linux processing probes, with no Windows benchmark. **No backend selected.** AudioHole decisions D-030/D-034 remain open. [Findings and evidence](FINDINGS.md), [source references](SOURCE_REFERENCES.md), [experimental decision gates](EXPERIMENTS.md).

## Scope of the three strategies

- **A — miniaudio I/O:** use `ma_context`/`ma_device` for capture/playback. Prefer evaluating separate source/render devices and AudioHole's own destination queue/controller alongside stock duplex. Native identity/probing can still be required; “primary backend” does not make the application supervisor library-owned.
- **B — direct I/O plus selected components:** own MMDevice/WASAPI lifecycle and use standalone miniaudio resampler/converter/channel/ring utilities where validated. This assignment's B is **not identical** to `DECISIONS.md` D-030's hybrid candidate (native discovery/probing plus miniaudio streaming), nor to an indefinitely maintained dual-I/O-backend design in parts of the project documentation. Keep these alternatives explicit in a future decision review.
- **C — direct WASAPI:** own lifecycle, buffers, scheduling, conversion and drift actuator without depending on miniaudio. Independent implementation can study algorithms without copying source; a narrowed format/layout policy still must satisfy arbitrary compatible endpoint routing.

## Evidence-weighted comparison

| Criterion | A: miniaudio I/O | B: native I/O + processing components | C: native WASAPI without miniaudio |
| --- | --- | --- | --- |
| Endpoint enumeration | Active-only basic list and limited format info; native extension needed for complete AudioHole records [D1–D2] | Full chosen native query/probe policy | Same native control as B |
| Precise format selection | Shared mix layout; optional Windows rate conversion; exclusive property-selected format [W2] | Explicit IsFormatSupported and Initialize candidates; selected components handle route conversion | Same I/O control; own conversion implementation |
| Low-latency shared | Client3 already present, restricted eligibility and period rounding/fallback [W2] | Choose legal queried period, explicit fallback and exact error reporting | Same potential I/O capabilities as B |
| Capture/render path | Generic blocking scratch copies; fixed adapter configurable; separate devices feasible [G1–G3] | Can process mapped spans directly where lifetimes permit, plus necessary inter-clock queue | Same ability, own complete pipeline |
| Independent clocks | Stock duplex does not implement continuous controller; AudioHole must supply [W4, G1] | Must supply queue/estimator/controller; can select components as actuator after validation | Same control burden; own actuator and DSP quality work |
| SRC/format quality | Built-in linear plus LPF or custom; generic ppm helper and phase defect need action [R1–R4] | Same defects if using those components; native I/O does not remove them | Must implement/test comparable quality or choose independently authorized processing dependency |
| Packed s24 shuffle | Defective direct component path reproduced [A3, E1] | Same defect if selected; prefer validated alternative | Own implementation can avoid it, but requires tests |
| Buffer budget | Three-period default requests, fixed adapter, endpoint buffer details inspectable but policy mediated | Full application budget, no fixed adapter unless deliberately added | Same control as B |
| Timestamp feedback | Not provided by callback path; native extension/internal access needed [W4] | Capture packet timestamps, IAudioClock/QPC available to chosen design | Same |
| Scheduling | Context priority + opt-in Games/Pro Audio; default worker highest Win32 priority but no MMCSS task [T1] | Own event workers and MMCSS registration/status | Same; own cleanup/error paths |
| Recovery | Built-in default migration is limited and reinit contains documented leak risk [D3] | Deferred supervisor lifecycle, explicit cleanup and identity matching | Same control, all implementation burden |
| HRESULT diagnostics | General result mapping plus uneven raw log detail [W2, W4] | Keep structured native HRESULT and API stage | Same |
| Source sharing / reverse direction | Application topology/ownership still needed; device object is not AudioHole logical hole | Explicit shared capture and per-destination consumers | Same |
| Memory | Per-device contexts/state/workers/converters/adapters; exact bytes configuration-dependent | Can omit device framework, keep selected component state plus custom buffers/workers | Small specialized footprint is possible, not established |
| Maintenance | Less original I/O code, upstream pin/update review, possible extension cost | Two responsibility layers, integration contracts, component bug/version tracking | Most custom platform code and compatibility responsibility |
| Licensing | Core public-domain/MIT-0 option; inventory extras | Same selected-component obligations; isolation doesn't change origin | Independent code/project licensing; copied adaptations would retain applicable provenance |

## Feasibility of component isolation

**Verified build evidence:** the research C executable includes one upstream implementation translation unit with `MA_NO_DEVICE_IO`, `MA_NO_DECODING`, `MA_NO_ENCODING`, `MA_NO_ENGINE`, `MA_NO_RESOURCE_MANAGER`, `MA_NO_NODE_GRAPH`. It initializes converters, linear/generic resamplers, a duplex ring helper and channel conversion without opening a device. This establishes processing-only compilation on the available Linux x64 compiler, not MSVC/Windows validation. Headers expose heap-size/preallocated APIs and allocation callbacks. [E1, R1–R4, B1]

For a future B prototype, wrap selected C objects in C++ RAII, build implementation once, keep compile options consistent across translation units, pin revision, and initialize state outside real-time processing. Do not extract arbitrary private functions into production: their assumptions/helpers/configuration are wider than their visible body. Prefer public component APIs if validated. Keep exact format/frame units and custom allocation ownership in interfaces. This research introduces no production dependency.

A component wrapper is simpler than maintaining two complete backends only if selected functions actually reduce tested code. Conversely, full miniaudio I/O can be simpler if native extensions and recovery work remain small. The source alone cannot quantify those integration costs.

## Decision gates and measurable criteria

Require identical endpoint pairs, formats, Windows mode, negotiated buffers, MMCSS, queue/controller, workload and build optimization for comparisons. Report actual conversion locations (Windows/miniaudio/application), not only requested rate. [E2–E9 in EXPERIMENTS](EXPERIMENTS.md)

Acceptance evidence should include per-direction correctness, zero unexplained steady-state losses in the declared duration/load, bounded occupancy under measured independent drift, no wrong-device substitution, Stop canceling recovery, exact initialization/error evidence, and repeatable attributable latency/CPU/working-set results. The 10 ms attributable objective is a test criterion under declared supported conditions; failure should explain which term dominates.

Possible outcomes:

- **A remains viable** if separate-device routing/controller meets latency/quality/recovery gates with acceptable native extension scope. Direct access is not uniquely capable of Client3/MMCSS.
- **B becomes attractive** if direct timing/format/lifecycle control is material and validated standalone processing removes enough custom DSP work. The current generic ratio helper and phase/s24 defects prevent assuming “drop-in, already solved”.
- **C becomes attractive** if a small transparent pipeline can satisfy required conversion/drift/compatibility quality with lower total maintenance cost. Reduced code or fewer dependencies alone do not establish correctness or speed.

All three are **Investigate Further** at architecture level. “Adopt/Adapt/Reject” in the finding register classifies specific techniques, not a final architecture.

## Current owner-approved comparison direction

Native event-driven WASAPI is already the leading candidate in the publication baseline, while selection remains open. Keep that preference distinct from a demonstrated performance win. B/C can directly investigate RATEADJUST plus IAudioClockAdjustment for shared rendering; A requires a backend extension because a defined constant is its only source occurrence. The adjustment setter cannot run on the real-time processing thread; neither Client3 compatibility nor equivalent low periods are established. Compare unchanged transfer, application-managed established SRC, and Windows-managed adjustment within one engine, as required by current D-034/O-005. No custom production DSP is recommended before that comparison.
