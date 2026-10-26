# AudioHole baseline and research questions

**Verified — initial supplied checkout documentation and file inventory (historical baseline).** Baseline commit `a2aaf7a8ca2b14c3ca37b59cf6d8bc255124a984` is “Initial commit: Add AudioHole v0.1 specifications”. The tracked checkout contains Markdown specifications and no CMake target or production source. README calls the project pre-alpha and labels features planned. Neither the roadmap nor a proposed architecture demonstrates implementation or hardware validation.

## Requirements reviewed

| Baseline source | Requirement relevant to research |
| --- | --- |
| `README.md`, `SPEC.md` §5 | Windows 10/11 x64 C++/CMake patchbay between existing endpoints; no new virtual driver; simplex is one directional route; duplex is two associated directional routes. |
| `ARCHITECTURE.md` §§5–6 | Backend boundary preserves negotiated settings, errors, timing; capture sharing may support mirrored destinations; each independently clocked destination needs separate compensation. |
| `ARCHITECTURE.md` §§8–11; `AGENTS.md` §8 | Minimize unnecessary conversion, bounded preallocated queues, independent clocks, drift control, event-driven operation and MMCSS evaluation, no blocking application work in callbacks. |
| `DECISIONS.md` D-017/D-018 | Conditional ≤10 ms attributable processing latency, 5 ms stretch; stability before small buffers; requested versus negotiated versus observed latency must stay separate. |
| `DECISIONS.md` D-030, D-034, D-035 | Backend, conversion/resampling implementation, and detailed buffer recommendation algorithm are **Open**. Prototype evidence is required before accepting a backend. |
| `DEPENDENCIES.md` §§2.2, 8 | Historical proof of concept used miniaudio; original source unavailable. Native, miniaudio, and hybrid are candidates. Application owns identity, recovery policy, probing evidence, profiles and diagnostics. |
| `DEVICE_PROBING.md` §§2, 7–10 | Easy Scan nonintrusive; Advanced Scan separately authorized; reported/accepted/verified/failed/inconclusive/not-tested distinctions; exact Windows errors; shared-mode acceptance does not establish native hardware support. |
| `CONFIG_FORMAT.md` §§3–4, 8–10; `AGENTS.md` §10 | Portable logical identity plus fingerprint, separate machine-local endpoint binding, explicit unknowns and ambiguous matches; manually editable JSON. |
| `PROFILES.md` §§12–14; `ARCHITECTURE.md` recovery/state sections | Independent directional status, explicit Stop cancels recovery, no arbitrary failover, preserve working profiles where possible, transactional switching. |
| `ROADMAP.md` phase 1 | Early risk prototypes: capture→render, reverse route, independent clocks, interruption/recovery, latency, CPU and diagnostics. Milestones are plans, not completion records. |

The initial supplied architecture had no explicit finalized destination-pacing policy. Before publication, the newer remote `work` revision **766ed88f7d7786bfb90fb7a520da34a15de010d1** was reviewed: `DECISIONS.md` D-034 and `ARCHITECTURE.md` reconciliation now explicitly accept **destination-paced operation, bounded FIFOs and adaptive synchronization**. These constraints govern this report. Native event-driven WASAPI is the owner-approved leading candidate; converter selection, direct-transfer eligibility and O-005 response policy remain open. Earlier draft wording based only on the supplied initial checkout was corrected before publication.

Terminology trap: one miniaudio `ma_device_type_duplex` pairs **one capture stream and one playback stream**, not AudioHole's two-direction logical hole. An AudioHole duplex hole may need four endpoint streams unless streams can safely be shared. Identical product names and nominal rates do not prove shared hardware clocking.

## Questions answered or bounded

1. Does WASAPI duplex use a playback callback and automatically regulate a cross-clock ring? No; the selected Windows path is blocking read/write, distinct from the library's asynchronous duplex helper.
2. Does shared low latency need a new native backend? Not necessarily: miniaudio already attempts `IAudioClient3`, with significant format/period eligibility conditions.
3. Are requested format, mix format and native format equivalent? No; requested rate can enable Windows SRC, and shared capabilities are incomplete evidence of hardware support.
4. Is dynamic rate support sufficient for drift compensation? No controller exists on the traced path, generic ratio precision is poor for ppm correction, and a phase-update wrap was reproduced.
5. Is native device management still useful if streaming uses miniaudio? Yes: portability, full state/property events, exhaustive probing and structured HRESULT evidence require more application logic.
6. What actually adds latency? Fixed callback buffering, queues, WASAPI buffers, and SRC history differ from memory copies and dispatch. Hardware measurements must separate them.
7. Is any strategy selected here? No. The comparison proposes decision gates for D-030 and D-034 without modifying them.

## Publication baseline reconciliation

Remote `origin` is `https://github.com/RLCypher/audiohole.git`, development branch `work`, observed head `766ed88f7d7786bfb90fb7a520da34a15de010d1` (19 commits ahead of the initial checkout). Remote root instructions, README, architecture, decisions, roadmap, dependencies, probing, engineering standard, GUI README and simulator controller interface were reviewed at that exact revision; the recursive tree is complete.

Current source includes CMake/Windows foundation/COM support, presentation and a `SimulatedController`, and the wxWidgets GUI preview. Current README and roadmap explicitly state that real capture/render, probing and synchronization are not implemented. Their recorded tests/GUI results are project reports, not tests executed by this research. This corrects the initial checkout's broader “no implementation” statement without claiming a real engine exists.

D-034/O-005 now require evaluating unchanged transfer, application-managed adaptive SRC and Windows-managed shared-render adjustment (`RATEADJUST` / `IAudioClockAdjustment::SetSampleRate`) within one engine. Automatic is provisional, with proactive compensation permitted for evaluation. Strict preservation is internal/experimental; do not silently modify/drop/insert/duplicate frames in that diagnostic mode, expose a new third public policy, choose arbitrary production thresholds, or turn the illustrative controller into a selected algorithm. Final dependency and response decisions remain open. SetSampleRate must run outside real-time processing, and Client3 flag compatibility needs evidence. The stock miniaudio backend defines RATEADJUST but does not initialize/use that actuator.

[Current decisions](https://github.com/RLCypher/audiohole/blob/766ed88f7d7786bfb90fb7a520da34a15de010d1/DECISIONS.md#L1480) · [Current architecture](https://github.com/RLCypher/audiohole/blob/766ed88f7d7786bfb90fb7a520da34a15de010d1/ARCHITECTURE.md#L1442) · [Current roadmap](https://github.com/RLCypher/audiohole/blob/766ed88f7d7786bfb90fb7a520da34a15de010d1/ROADMAP.md#L53) · [Current project status](https://github.com/RLCypher/audiohole/blob/766ed88f7d7786bfb90fb7a520da34a15de010d1/README.md#L79). No existing project file was altered.
