# AudioHole v0.1 — Development Roadmap

**Document:** `ROADMAP.md`  
**Status:** Proposed implementation roadmap  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `DEPENDENCIES.md`, `DEVICE_PROBING.md`, `CONFIG_FORMAT.md`, `PROFILES.md`, `API.md`, `GUI.md`, `TRAY.md`, `DECISIONS.md`, `AGENTS.md`

---

## 1. Purpose

This document defines the implementation sequence for AudioHole v0.1.

AudioHole should be developed through small, testable milestones, each producing a working result or resolving a specific technical uncertainty.

The roadmap prioritizes:

1. Architectural correctness.
2. Reliable audio routing.
3. Portable device identity.
4. Predictable recovery behavior.
5. Transparent configuration.
6. Usable graphical configuration.
7. Persistent background operation.
8. Diagnostic visibility.
9. Performance validation.
10. Release readiness.

The roadmap is organized by dependencies and validation gates rather than calendar deadlines. The reconciled working sequence below governs ordering; the existing numbered phases remain the deliverable inventory and retain their history. Documentation approval does not authorize implementation or prototypes.

## Reconciled Working Sequence

This sequence incorporates owner guidance without dropping existing v0.1 requirements. Acceptance criteria below are outcomes to demonstrate, not claims of completed tests. Exact soak durations, drift bounds, quality tolerances, and performance thresholds must be documented with the prototype plan; review suggestions such as eight-hour runs or ±200 ppm were not approved as fixed requirements.

| Order | Scope and existing milestone mapping | Buildable/testable result and acceptance gate |
|---|---|---|
| 1 | Reproducible foundation (0.1–0.2); minimum controller, status, diagnostics, and test harness from 2.1/8.1/8.2 | Clean documented Windows x64 build; nonzero tests execute; ordinary-user application shell launches; provisional dependency versions/licenses recorded before integration. |
| 2 | One reliable simplex path (narrow 1.1/3.1) | Explicit capture-to-render routing transfers known input to the selected output; repeated Start/Stop cleans up; requested/effective format, buffering, and errors are recorded. A developer harness is not a required public CLI. |
| 3 | Independent-clock synchronization and latency (1.3, relevant 3.2/9.1) | Bounded FIFO behavior under simulated and real independent clocks; distinguish direct transfer, compensation, and conversion; document fidelity, drift/jitter evidence, instability, and latency boundaries. Compare unchanged transfer, application-managed adaptive resampling, and Windows-managed shared-render rate adjustment. No final O-005 response policy is implied by a prototype. |
| 4 | Backend comparison and exclusive behavior (1.1, D-030) | Equivalent native WASAPI/miniaudio conditions compared for latency, CPU, negotiation, compatibility, exclusive conflicts, cleanup, and recovery observations; include supported rate-adjustment initialization paths from the expanded experiments below. Record limitations; selection remains gated on representative evidence, including later duplex. |
| 5 | Recovery and device identity (1.4, minimum 2.2/2.3, 4.1/4.2, Easy Scan subset) | Explainable portable matching; ambiguous matches require confirmation; disconnect/reconnect and Stop-during-open/retry tests reject stale results; Ready and armed recovery remain correct. Do not invent unresolved persistence schemas. |
| 6 | Duplex operation (1.2/3.3) | Both independently configured directions transfer known signals; partial failure preserves the healthy direction under the existing default; duplex counts once; resources cleanly stop. Complete representative D-030 evidence before production commitment. |
| 7 | Multiple holes and mirroring (3.4) | Compatible capture sharing with independent destination queues; failure/slow consumers do not unnecessarily stall unrelated paths; 1–4 and 5–8 policy/advisory and ninth-hole rejection tested. Measure actual achievable hardware performance separately. |
| 8 | Complete profiles, persistence, transitions, and failover (2.2/2.4, 4.3–4.5) | Approved schemas preserve inheritance, notes, IDs, and edits; invalid/conflicting files remain intact; switches preserve working routes where possible; only configured fallback candidates activate. Resolve O-001/O-002/O-004 before implementing their material policies. |
| 9 | GUI/tray and personality (6.1–6.6, 7.1–7.2) | D-031 prototype meets documented selectors, accessibility/DPI, diagnostics, and personality requirements before final framework selection; four tabs, Apply/Revert, shared status, close-to-tray, inert DFA, and identical mode behavior validated. O-003 remains a gate for nondefault close/tray semantics. |
| 10 | Complete setup tools and evidence (5.1–5.3, setup acceptance below) | Easy Scan stays nonintrusive; Advanced Scan consent, stage evidence, cancellation, and partial results work; provisional process isolation evaluated; activity opens no extra capture; Test Beep is bounded, exact-target, cancelable, and non-disruptive. |
| 11 | Startup and notifications (7.3–7.4) | Shared monotonic deadline, retarget/bypass/cancel races, and stale callbacks tested; missing hardware leads to Ready; no duplicate activation; notifications obey preferences and deduplication. |
| 12 | Release validation (9.1–10.3) | Representative hardware endurance, latency, fidelity, resource use, sleep/resume, audio-service interruption, portable restoration, and identity ambiguity documented; no known critical data-loss or routing-state defect. Packaging/licensing decisions resolved before distribution. |

The internal command/status boundary, minimal canonical state machine, generation-based cancellation, real-time telemetry, and safe configuration handling begin early and expand with each milestone. Their later numbered sections describe completion scope, not permission to postpone essential foundations. Full Advanced Scan and the complete GUI must not delay the initial simplex proof.

Public transport implementation (8.3) is deferred until core routing is proven and separately authorized; named pipes remain a candidate. O-001 through O-006 in `DECISIONS.md` remain owner decisions, not implementation defaults. The approved Windows 10 22H2 / supported Windows 11 baseline describes intended tests, not universal endpoint capability or completed validation.

### Expanded synchronization experiments

**Status:** Provisional evaluation plan. D-034/O-005 and O-001 through O-006 remain Open. No algorithm, production schema, new public mode, or quantitative quality/converter-delay limit is selected. The initial simplex prototype needs no further product decision: after separate authorization, use explicit compatible endpoints, a narrow format scope, bounded buffers, and honest diagnostics. Full synchronization experiments and evidence-dependent tuning must not delay that first path.

Use one engine with independently resolved directional preferences. Automatic is the proposed prototype default, permitting proactive compensation based on evidence; it need not wait for an actual underrun. Strict preservation is internal/experimental, with incompatible formats rejected, no intentional resampling/frame modification/insertion/duplication/dropping, explicit reporting and restart/rebuffer, and no silent compensation fallback. Do not expose a third public correction policy.

| Experiment | Comparison and evidence to collect |
|---|---|
| Transfer strategy | Compare unchanged transfer where supported, application-managed adaptive resampling, and Windows-managed shared-render rate adjustment (`AUDCLNT_STREAMFLAGS_RATEADJUST`, `IAudioClockAdjustment::SetSampleRate`). Hold hardware, channel/format settings, load, and buffering/periods equivalent where APIs permit; record unavoidable differences and unsupported combinations instead of declaring false equivalence. Neither compensation location is presumed superior or sample-preserving. |
| WASAPI compatibility | Test event-driven shared `IAudioClient::Initialize`, supported rate-adjustment flags/services, `IAudioClient3::InitializeSharedAudioStream`, and low-period configurations. Record accepted flags, actual periods/formats, exact failures, and fallback paths. Do not assume low-period initialization accepts RATEADJUST or all legacy initialization flags. Windows-managed adjustment is a shared-render candidate, not exclusive/capture support; compare exclusive routes using applicable strategies. Adjustment calls remain outside real-time processing. |
| Clock evidence and stability | Observe position-versus-correlated-time progression and FIFO trends; do not treat constant `IAudioClock::GetFrequency` as a measured drift estimate. Exercise matching nominal rates at 48/96 kHz, both signs of representative relative 10/50/100/200 ppm mismatch, drift changes, timestamp errors, jitter, and stalls. Include approximately 400 ppm when modeling opposite ±200 ppm endpoint errors. Record scenario bounds/durations; these are experimental inputs, not accepted hardware tolerances or tuning thresholds. Compare independent USB devices with same-device or documented shared-clock arrangements without inferring common clocks from hubs or nominal rates. |
| Strategy lifetime | Compare initialization-time selection of a stable processing path against dynamic engagement/disengagement. Record transition artifacts, FIFO/latency changes, sample loss, filter/history effects, and repeated switching. Eligibility, confidence, hysteresis, and ratio limits remain evidence-dependent choices. |
| Fidelity and discrete correction | Compare speech, music, transients, tones, silence, and multichannel alignment using repeatable known signals. Measure converter errors/artifacts and discrete insertion/duplication/drop consequences against smooth adaptive compensation. Verify unchanged PCM values and sequence within a stated digital boundary where supported. Keep corrections experimental; record emergency buffer discard/reinitialization as recovery rather than routine compensation. |
| Latency and resources | Record minimum achievable and typical operating latency, variation over time, FIFO occupancy/delay, converter delay, end-to-end measured latency including external hardware where measurable, CPU, and memory. State boundaries and distinguish measurements from estimates. Keep the conditional 10 ms attributable objective; do not invent arbitrary converter-delay or signal-error cutoffs before comparative evidence. |
| Recovery and directional behavior | Exercise stalls, capture discontinuities, device loss/recreation, and Stop during pending operations. Preserve stale-result rejection and healthy-direction isolation. For later duplex/mirroring, compare independent drift/policies and destination queues. Strict experiments must report inability to preserve and require explicit restart/rebuffer; Automatic emergency-recovery permissions still require review. |
| Observability | Independently expose selected preference, actual processing, health, and endpoint mode through internal diagnostic observations. Record mismatch/confidence, occupancy/trend, effective ratio, transitions/reasons, lost/corrected frames, discontinuities, recovery, and latency. No sample loss may be concealed and no JSON field or public command is finalized by this experiment plan. |

Evidence gates are reproducible configurations and commands, actual test execution/duration, complete strategy/mode results including unsupported or inconclusive outcomes, bounded-buffer and cleanup observations, scoped sample-sequence comparisons, and measured quality/latency/resource reports. Record artifact and instability cases, not only averages or successful initialization. Propose objective acceptance thresholds from these comparative results for review before final commitment; do not mistake an evaluation run for proof of universal stability or end-to-end bit-perfect audio.

Defer estimator windows, direct-transfer eligibility thresholds, ratio limits, FIFO targets, transition mechanics, converter choice, and quantitative quality/delay limits until evidence exists. Final D-034/O-005 commitments, public strict-preservation exposure, Automatic emergency-recovery permissions, and production persistence/API contracts still require owner review. Other open decisions remain unchanged and do not block independent, separately authorized foundation/simplex work.

---

## 2. Development Principles

### 2.1 Build Vertically

Prefer small end-to-end implementations over large collections of disconnected components.

A working capture-to-playback path is more valuable than a complete collection of interfaces that cannot yet route audio.

### 2.2 Validate Risk Early

The highest-risk technical areas should be investigated before they become architectural dependencies.

These include:

- Audio backend behavior.
- Low-latency operation.
- Independent device clocks.
- Drift compensation.
- Duplex routing.
- Device identity persistence.
- Stream interruption and recovery.

### 2.3 Keep Configuration Transparent

Human-readable configuration is a foundational requirement, not a feature to retrofit later.

### 2.4 Separate Prototype and Production Code

Experiments may use simplified implementations.

Production components must follow the accepted architecture and configuration contracts.

### 2.5 Preserve Working Milestones

Each completed milestone should leave the repository in a buildable, testable condition.

### 2.6 Avoid Premature Complexity

Do not implement deferred features merely because a dependency makes them convenient.

---

# Phase 0 — Repository and Development Foundation

## Milestone 0.1 — Repository Structure

**Goal:** Establish a maintainable project layout.

### Deliverables

- CMake project.
- Initial C++ source structure.
- Documentation directory.
- Third-party dependency strategy.
- Test directory.
- Prototype directory.
- Basic build instructions.
- License selection.
- Initial README.

### Suggested Structure

```text id="2g5umr"
AudioHole/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── AGENTS.md
├── docs/
│   ├── SPEC.md
│   ├── ARCHITECTURE.md
│   ├── DEPENDENCIES.md
│   ├── DEVICE_PROBING.md
│   ├── CONFIG_FORMAT.md
│   ├── PROFILES.md
│   ├── API.md
│   ├── GUI.md
│   ├── TRAY.md
│   ├── DECISIONS.md
│   └── ROADMAP.md
├── src/
│   ├── app/
│   ├── audio/
│   ├── devices/
│   ├── profiles/
│   ├── config/
│   ├── diagnostics/
│   ├── gui/
│   ├── tray/
│   └── control/
├── tests/
├── prototypes/
└── third_party/
```

The exact layout may change during implementation, but separation of responsibilities should remain.

### Acceptance Criteria

- Clean CMake configuration.
- Successful x64 build.
- Basic executable launches.
- Unit-test target runs.
- Repository documentation is discoverable.

---

## Milestone 0.2 — Toolchain and Dependency Evaluation

**Goal:** Establish a reproducible development environment.

### Deliverables

- Compiler baseline.
- CMake version requirement.
- Dependency acquisition strategy.
- Build presets.
- Debug and Release configurations.
- Initial dependency/license inventory.

### Evaluation

Review candidate dependencies against `DEPENDENCIES.md` and `DECISIONS.md`.

### Acceptance Criteria

- Fresh checkout can be built using documented steps.
- No unexplained manual dependency installation.
- Dependency versions are controlled.
- License obligations are documented.

---

# Phase 1 — Technical Risk Prototypes

## Milestone 1.1 — Audio Backend Evaluation

**Goal:** Select a viable audio backend architecture.

### Candidates

- Native WASAPI.
- miniaudio.
- Hybrid implementation.

### Prototype Requirements

Demonstrate:

1. Enumerating Windows audio endpoints.
2. Opening a capture stream.
3. Opening a playback stream.
4. Routing capture to playback.
5. Starting and stopping cleanly.
6. Reporting initialization failures.
7. Handling endpoint removal.
8. Supporting independent endpoint clocks.

### Evaluation Criteria

- Reliability.
- Latency.
- CPU usage.
- Diagnostics.
- Shared-mode support.
- Exclusive-mode support.
- Recovery behavior.
- Implementation complexity.

### Acceptance Criteria

- A documented comparison exists.
- At least one approach routes audio reliably.
- Known limitations are recorded.
- Backend choice is entered into `DECISIONS.md`.

**Validation Gate:** Do not build the production audio engine around an unvalidated backend.

---

## Milestone 1.2 — Duplex Routing Prototype

**Goal:** Validate simultaneous bidirectional routing.

### Deliverables

- Two independent directional audio paths.
- One logical duplex connection.
- Independent stream lifecycle management.
- Basic directional status reporting.

### Acceptance Criteria

- Both directions operate simultaneously.
- Failure in one direction is detectable independently.
- Stopping one test session releases resources cleanly.
- No unintended feedback loop is introduced by the test configuration.

---

## Milestone 1.3 — Clock Drift and Buffering Prototype

**Goal:** Demonstrate stable routing between devices with independent clocks.

### Deliverables

- Bounded audio buffers.
- Buffer fill-level monitoring.
- Drift-compensation strategy.
- Underrun/overrun detection.
- Basic latency instrumentation.

### Acceptance Criteria

- Sustained routing does not exhibit uncontrolled buffer growth.
- Clock differences are handled without repeated catastrophic underruns.
- Audio processing remains bounded.
- Test results include latency and stability observations.

### Validation Gate

The drift-compensation approach must be documented before becoming a production dependency.

---

## Milestone 1.4 — Device Identity Prototype

**Goal:** Validate durable logical device matching.

### Deliverables

- Windows endpoint enumeration.
- Device fingerprint extraction.
- Logical device records.
- Machine-local binding cache.
- Match-confidence reporting.
- Ambiguity detection.

### Test Cases

- USB port changes.
- Device disconnect/reconnect.
- Driver update where practical.
- Identical device models.
- Missing serial numbers.
- Configuration transferred to another installation.

### Acceptance Criteria

- Stable logical IDs survive ordinary endpoint reenumeration.
- Ambiguous devices are not silently misidentified.
- Missing devices remain represented.
- Matching decisions are explainable.

---

# Phase 2 — Core Architecture

## Milestone 2.1 — Application Controller

**Goal:** Establish one authoritative command and state interface.

### Deliverables

- Application controller.
- Routing supervisor interface.
- Structured command model.
- Structured status model.
- Event publication.
- Asynchronous operation handling.

### Acceptance Criteria

- Commands are serialized or coordinated safely.
- GUI/tray/API adapters can consume the same state.
- No interface independently owns routing state.
- Commands are safe during transitional states.

---

## Milestone 2.2 — Configuration System

**Goal:** Implement transparent, portable configuration.

### Deliverables

- UTF-8 JSON parsing.
- Schema validation.
- Deterministic serialization.
- Atomic writes.
- Backup/recovery behavior.
- Settings storage.
- Profile storage.
- Device record storage.
- Machine-local binding storage.
- Configuration migration framework.

### Acceptance Criteria

- Files remain human-readable.
- Manual edits are supported.
- Invalid files produce understandable errors.
- Interrupted writes do not silently corrupt configuration.
- Portable and machine-local data remain separated.

---

**Additional acceptance criteria for the preceding milestone:**

- Shared audio defaults and directional overrides survive save/load; legacy hole-level audio migrates without silent loss.
- Startup-window preference migration preserves its original meaning.

---

## Milestone 2.3 — Device Registry

**Goal:** Implement persistent device discovery and identity management.

### Deliverables

- Logical device registry.
- Endpoint binding resolution.
- Device fingerprints.
- Display names.
- Editable notes.
- Availability tracking.
- Device-change notifications.
- Ambiguous-match workflow.

### Acceptance Criteria

- Saved devices survive disconnection.
- Device renaming preserves identity.
- Device notes survive rescanning.
- Profile references remain stable.
- Ambiguous bindings require confirmation.

---

## Milestone 2.4 — Profile Model

**Goal:** Implement the canonical profile and hole representation.

### Deliverables

- Profile creation and loading.
- Simplex holes.
- Duplex holes.
- Per-direction settings.
- Enabled/disabled holes.
- Profile notes.
- Hole notes.
- Profile validation.
- Active-hole limit enforcement.

### Acceptance Criteria

- Duplex holes count as one logical hole.
- Profiles serialize to readable JSON.
- Invalid references are reported.
- Disabled holes do not consume active routing capacity.
- Profile changes can be staged.

---

# Phase 3 — Production Audio Engine

**Additional acceptance criteria for the preceding milestone:**

- Duplex directions have independent effective audio settings; hole notes survive duplication and migration.

---

## Milestone 3.1 — Simplex Routing

**Goal:** Deliver the first production-quality audio path.

### Deliverables

- Capture stream.
- Playback stream.
- Bounded buffering.
- Audio format negotiation.
- Start/Stop lifecycle.
- Structured diagnostics.

### Acceptance Criteria

- A simplex hole routes audio reliably.
- Stream failures are reported.
- Resources are released cleanly.
- No filesystem or GUI work occurs on real-time callbacks.

---

## Milestone 3.2 — Format Conversion

**Goal:** Support endpoints with differing native formats.

### Deliverables

- Sample-format conversion.
- Sample-rate conversion.
- Channel handling.
- Conversion diagnostics.
- Conversion-related latency reporting.

### Acceptance Criteria

- Supported format differences are handled correctly.
- Unsupported combinations fail clearly.
- Conversion does not cause unbounded buffering.
- Audio quality and CPU use are measured.

---

## Milestone 3.3 — Duplex Routing

**Goal:** Support production duplex holes.

### Deliverables

- Two associated directional paths.
- Independent format and buffer settings.
- Directional status.
- Coordinated activation and shutdown.

### Acceptance Criteria

- Both directions route simultaneously.
- One failed direction is reported independently.
- The logical hole remains one user-facing object.
- Shutdown releases all associated streams.

---

## Milestone 3.4 — Multiple Holes

**Goal:** Support the v0.1 routing scale.

### Deliverables

- Concurrent hole management.
- Resource accounting.
- Active-hole limits.
- Multi-hole diagnostics.

### Acceptance Criteria

- One to four holes operate within the intended normal range.
- Five to eight holes produce the defined advisory.
- More than eight active holes are rejected.
- A duplex hole counts once.
- Resource conflicts are reported.

---

# Phase 4 — Recovery and Profile Management

## Milestone 4.1 — Routing State Machine

**Goal:** Implement canonical routing states.

### Deliverables

- Stopped.
- Ready.
- Starting.
- Running.
- Degraded.
- Recovering.
- Failed.
- Stopping.

### Acceptance Criteria

- State transitions are deterministic.
- Ready is not misclassified as failure.
- Partial failures are distinguishable from complete failures.
- Commands during transitions are handled safely.

---

## Milestone 4.2 — Device Loss and Recovery

**Goal:** Recover from ordinary endpoint interruptions.

### Deliverables

- Device-loss detection.
- Automatic retry.
- Rebinding.
- Recovery diagnostics.
- Stop/disarm behavior.

### Acceptance Criteria

- A disconnected device is detected.
- A returning device can be rebound.
- Recovery does not require reopening the GUI.
- Explicit Stop cancels retries.
- Recovery does not repeatedly apply the startup delay.

---

## Milestone 4.3 — Failover

**Goal:** Support configured fallback endpoints.

### Deliverables

- Fallback candidate handling.
- Endpoint substitution rules.
- Actual-endpoint reporting.
- Recovery to preferred endpoint according to policy.

### Acceptance Criteria

- Failover occurs only to configured candidates.
- Ambiguous devices are not silently substituted.
- The actual active endpoint is reported.
- Failover does not create uncontrolled switching loops.

---

## Milestone 4.4 — Transactional Profile Switching

**Goal:** Prevent failed switches from unnecessarily destroying working routing.

### Deliverables

- Destination validation.
- Resource preparation.
- Commit/rollback behavior.
- Explicit Ready activation.
- Failure explanations.

### Acceptance Criteria

- Unavailable destination profiles do not silently replace working profiles.
- Compatible streams are preserved where practical.
- Failed switches report their cause.
- Users may deliberately arm an unavailable profile.

---

## Milestone 4.5 — Staged Configuration Changes

**Goal:** Allow profile editing without unexpected audio interruption.

### Deliverables

- Staged routing edits.
- Apply/Revert.
- Conflict detection.
- Minimal-disruption updates.

### Acceptance Criteria

- Editing a profile does not immediately alter running streams.
- Applying changes uses the controller.
- Failed application preserves previous working behavior where possible.
- External configuration edits are not silently overwritten.

---

# Phase 5 — Discovery and Probing

## Milestone 5.1 — Easy Scan

**Goal:** Provide nonintrusive device discovery.

### Deliverables

- Endpoint enumeration.
- Windows-reported capabilities.
- Driver metadata.
- Device fingerprints.
- Device record updates.
- Structured scan events.

### Acceptance Criteria

- Easy Scan does not deliberately open disruptive test streams.
- Results distinguish reported information from verified evidence.
- Device notes and logical identities are preserved.
- Missing devices remain in the registry.

---

## Milestone 5.2 — Advanced Scan

**Goal:** Verify device capabilities experimentally.

### Deliverables

- Explicit consent.
- Selected-device probing.
- Shared-mode tests.
- Exclusive-mode tests where supported.
- Format tests.
- Buffer tests.
- Cancellation.
- Partial-result preservation.
- HRESULT reporting.

### Acceptance Criteria

- Advanced Scan warns before disruptive operations.
- Results distinguish supported, unsupported, failed, and inconclusive tests.
- Cancellation leaves consistent records.
- Results include timestamps and test context.
- Scan history remains human-readable.

---

## Milestone 5.3 — Recommendation Engine

**Goal:** Automatically select sensible format and buffer settings.

### Deliverables

- Format recommendation logic.
- Buffer recommendation logic.
- Evidence-based ranking.
- Conservative fallback behavior.
- Human-readable explanations.
- Warning generation.

### Acceptance Criteria

- Recommendations reflect actual available evidence.
- Unverified settings are not described as verified.
- Conversion requirements are identified.
- Aggressive buffers receive appropriate warnings.
- User overrides are preserved where valid.

---

# Phase 6 — Graphical Interface

## Milestone 6.1 — GUI Framework Integration

**Goal:** Establish the desktop interface.

### Deliverables

- Selected GUI framework.
- Main window.
- Profiles tab.
- Devices tab.
- Scanning tab.
- Settings tab.
- Persistent routing status area.

### Acceptance Criteria

- GUI launches reliably.
- Tabs operate independently.
- GUI consumes controller state.
- Closing the GUI does not stop routing.

---

## Milestone 6.2 — Profile Editor

**Goal:** Implement the primary Add Hole workflow.

### Deliverables

- Profile creation and selection.
- Profile notes.
- Add Hole.
- Simplex/duplex selection.
- Source/destination dropdowns.
- Format dropdown.
- Buffer dropdown.
- Hole notes.
- Apply/Revert.

### Acceptance Criteria

- Ordinary simplex creation requires only endpoint selection.
- Recommended settings populate automatically.
- Duplex directions remain independently configurable.
- Missing devices remain identifiable.
- Active routing is not modified before Apply.

---

## Milestone 6.3 — Recommendation Presentation

**Goal:** Make technical settings understandable.

### Deliverables

- Default/Recommended/User Selected labels.
- Visible recommendation explanations.
- Technical tooltips.
- Caution indicators.
- Evidence labels.

### Acceptance Criteria

- Users can see why a setting was recommended without hovering.
- Technical details are available through tooltips.
- Warnings explain specific risks.
- Recommendation changes update explanations.
- No unsupported verification claims appear.

---

## Milestone 6.4 — Device Management

**Goal:** Expose the device registry through the GUI.

### Deliverables

- Recording/playback device lists.
- Device details.
- Renaming.
- Notes.
- Fingerprints.
- Driver information.
- Availability.
- Scan history.

### Acceptance Criteria

- Device names and notes are editable.
- Rescans preserve user metadata.
- Missing devices remain visible.
- Ambiguous bindings are explainable.

---

## Milestone 6.5 — Scanning Interface

**Goal:** Provide integrated device probing and diagnostics.

### Deliverables

- Easy Scan controls.
- Advanced Scan controls.
- Consent dialog.
- Progress display.
- Integrated verbose console.
- Filtering.
- Copy/export.
- Scan history.

### Acceptance Criteria

- Scanning remains independent of profile editing.
- Console output reflects structured events.
- Cancellation works.
- Large logs do not block the GUI or audio engine.

---

## Milestone 6.6 — First-Run Introduction

**Goal:** Guide new users through initial configuration.

### Deliverables

- Welcome.
- Easy Scan prompt.
- Device review.
- Profile creation.
- Add Hole walkthrough.
- Recommendation explanation.
- Activation.
- Finish.

### Acceptance Criteria

- Introduction can be skipped.
- Introduction can be rerun.
- Rerunning does not reset configuration.
- Interrupted setup does not corrupt files.

---

# Phase 7 — Tray and Startup

## Milestone 7.1 — Persistent Tray

**Goal:** Keep AudioHole accessible while the GUI is closed.

### Deliverables

- Tray icon.
- State-specific appearance.
- Tooltip.
- Context menu.
- Open GUI.
- Start/Stop.
- Exit.

### Acceptance Criteria

- Closing the GUI preserves routing.
- Tray reflects actual routing state.
- Stop disarms routing.
- Exit terminates the application.
- Tray survives ordinary Explorer restarts where supported.

---

## Milestone 7.2 — Profile and Hole Menus

**Goal:** Provide useful everyday control without reopening the GUI.

### Deliverables

- Profiles submenu.
- Active profile indicator.
- Transactional profile switching.
- Holes submenu.
- Duplex directional status.

### Acceptance Criteria

- Profile switching follows established rules.
- Failed switches preserve working routing where possible.
- Duplex holes count once.
- Hole status remains accurate.

---

**Additional acceptance criteria for the preceding milestone:**

- The Holes submenu shows status without individual enable/disable actions; duplex directional states remain visible.

---

## Milestone 7.3 — Notifications

**Goal:** Report meaningful routing events.

### Deliverables

- Native Windows toast integration.
- Notification preferences.
- Failure notifications.
- Recovery notifications.
- Deduplication.
- Silent informational notifications.

### Acceptance Criteria

- Default level is Warnings and Errors.
- Ready-to-Running informational toasts are silent.
- Partial and complete failures generate eligible notifications.
- Recovery loops do not spam the user.
- Notification failure does not affect routing.

---

## Milestone 7.4 — Configurable Startup

**Goal:** Support reliable automatic and delayed activation.

### Deliverables

- Optional Windows startup.
- Startup profile selection.
- Manual/Immediate/Delayed activation.
- Configurable delay.
- Shared countdown state.
- GUI and tray countdown presentation.
- Start Routing Now.
- Cancel Startup.

### Acceptance Criteria

- Delay begins when AudioHole launches.
- Countdown does not block the application.
- Manual Start bypasses the timer.
- Stop cancels pending activation.
- Missing hardware results in Ready after expiration.
- Recovery does not repeat the startup delay.
- Countdown cannot trigger duplicate activation.

---

# Phase 8 — Diagnostics and External Control

**Additional acceptance criteria for the preceding milestone:**

- Selecting a different profile during a countdown preserves the deadline and retargets activation.
- Canceled and obsolete timer callbacks cannot activate routing.
- GUI and tray share controller countdown status; pending timers are not restored across application launches.

---

## Milestone 8.1 — Runtime Diagnostics

**Goal:** Make routing behavior inspectable.

### Deliverables

- Structured routing events.
- Hole-level status.
- Directional status.
- Stream errors.
- Recovery history.
- Buffer statistics.
- Available latency measurements.
- Diagnostic export.

### Acceptance Criteria

- Diagnostics remain accessible while routing.
- Audio callbacks are not blocked by logging.
- Exported data is human-readable.
- Unknown measurements are not fabricated.

---

## Milestone 8.2 — Internal Command Interface

**Goal:** Provide a consistent control surface for GUI, tray, and future integrations.

### Deliverables

- Command definitions.
- Status queries.
- Event subscriptions.
- Error responses.
- Asynchronous completion handling.

### Acceptance Criteria

- Commands use the authoritative controller.
- Repeated commands are safe.
- Transitional states are handled.
- No interface bypasses the routing supervisor.

---

**Additional acceptance criteria for the preceding milestone:**

- The internal controller exposes pending activation separately from canonical routing state.
- Profile selection, timer expiration, manual activation, and cancellation are safely coordinated.

---

## Milestone 8.3 — Optional External Control

**Goal:** Evaluate lightweight integration with tools such as OBS and Stream Deck.

**Status:** Conditional on the corresponding `DECISIONS.md` selection.

### Candidate Deliverables

- Windows named-pipe transport.
- CLI wrapper.
- Optional localhost HTTP or WebSocket adapter.
- Permission controls.
- Connection diagnostics.

### Acceptance Criteria

- External control is disabled by default where applicable.
- Commands cannot directly manipulate audio callbacks.
- Status matches GUI and tray.
- Unauthorized access is rejected.
- Disabled adapters impose negligible overhead.

---

# Phase 9 — Validation and Optimization

**Additional acceptance criteria for the preceding milestone:**

- Individual hole enable/disable external commands remain deferred beyond v0.1.
- Optional external transports may expose hole-status inspection without mutating hole configuration.

---

## Milestone 9.1 — Latency Measurement

**Goal:** Measure AudioHole's attributable latency.

### Deliverables

- Repeatable measurement procedure.
- Baseline test configurations.
- Recorded results.
- Shared/exclusive comparisons.
- Buffer-setting comparisons.

### Acceptance Criteria

- Measurements distinguish AudioHole latency from external device and wireless latency.
- Results are reproducible.
- The 10 ms objective is evaluated honestly.
- The 5 ms stretch objective is evaluated where practical.

---

## Milestone 9.2 — Stability Testing

**Goal:** Validate sustained operation.

### Test Scenarios

- Long-running simplex routing.
- Long-running duplex routing.
- Multiple active holes.
- Device disconnection/reconnection.
- Sleep and wake.
- Windows audio-service restart.
- Application restart.
- Driver changes where practical.
- Profile switching during operation.

### Acceptance Criteria

- No uncontrolled memory growth.
- No persistent runaway buffering.
- No unrecoverable state corruption.
- Failures are reported clearly.
- Recovery behavior matches the specification.

---

## Milestone 9.3 — Configuration Portability

**Goal:** Verify the central portability requirement.

### Test Scenarios

- Copy configuration to a clean Windows installation.
- Change USB ports.
- Reinstall device drivers.
- Replace a device with the same model.
- Connect multiple identical devices.
- Transfer configuration to another computer.

### Acceptance Criteria

- Logical device references remain stable.
- Matching devices are rebound when sufficiently identifiable.
- Ambiguous matches require confirmation.
- Unavailable devices remain in configuration.
- User notes and profile organization survive transfer.

---

## Milestone 9.4 — Resource and Performance Testing

**Goal:** Confirm AudioHole remains lightweight.

### Deliverables

- Idle CPU measurements.
- Active CPU measurements.
- Memory usage measurements.
- Multiple-hole performance.
- GUI-closed performance.
- Tray-only operation measurements.

### Acceptance Criteria

- Background operation has reasonable resource use.
- GUI and tray do not introduce audio glitches.
- Logging does not block audio callbacks.
- Performance limitations are documented.

---

# Phase 10 — Release Preparation

## Milestone 10.1 — Packaging

**Goal:** Produce a distributable Windows build.

### Candidate Deliverables

- Portable ZIP.
- Windows installer.
- Dependency packaging.
- Version information.
- Startup registration handling.
- Clean uninstall behavior.

### Acceptance Criteria

- Installation does not require unnecessary elevation.
- Audio routing requires no administrator privileges.
- Configuration remains portable.
- Uninstall behavior is documented.
- Distribution includes required licenses.

---

## Milestone 10.2 — Documentation

**Goal:** Prepare user-facing and contributor documentation.

### Deliverables

- Installation guide.
- Quick-start guide.
- Profile creation guide.
- Device scanning guide.
- Troubleshooting guide.
- Configuration format reference.
- Contributor build instructions.
- Known limitations.

### Acceptance Criteria

- A new user can create a working hole without developer assistance.
- Technical users can inspect and edit configuration.
- Diagnostic export is documented.
- Limitations are stated clearly.

---

## Milestone 10.3 — Release Candidate

**Goal:** Validate the complete v0.1 experience.

### Required Scenarios

1. Install or extract AudioHole.
2. Launch the application.
3. Complete or skip the introduction.
4. Discover devices.
5. Create a profile.
6. Add a simplex hole.
7. Add a duplex hole.
8. Accept recommended settings.
9. Activate routing.
10. Close the GUI.
11. Control routing from the tray.
12. Disconnect and reconnect a device.
13. Verify automatic recovery.
14. Switch profiles.
15. Export diagnostics.
16. Stop routing.
17. Exit AudioHole.
18. Restart AudioHole.
19. Verify saved configuration remains intact.

### Acceptance Criteria

- No critical known data-loss defects.
- No critical known routing-state inconsistencies.
- No unresolved crash in ordinary workflows.
- Core documentation matches implementation.
- Accepted decisions are reflected in the code.
- Known limitations are recorded.

---

# Phase 11 — Post-v0.1 Planning

The following should not delay the initial release unless they become necessary to satisfy an accepted requirement:

- Cross-platform support.
- Virtual audio drivers.
- Remote network audio routing.
- Complex audio mixing.
- Advanced signal processing.
- Unlimited routing scale.
- Custom tray dashboards.
- Remote web administration.
- Expanded external-control integrations.

These features should be evaluated only after the core application is reliable.

---

## 12. Milestone Completion Rules

A milestone is complete only when:

1. Its deliverables exist.
2. Its acceptance criteria pass.
3. Relevant tests are recorded.
4. Significant limitations are documented.
5. Architectural decisions are updated.
6. The repository remains buildable.
7. Relevant specifications remain consistent.

A demonstration that works once is not sufficient evidence of completion.

---

## 13. Original Phase Inventory

These phase labels preserve the original scope and references. Use the Reconciled Working Sequence above for execution order.

| Phase | Primary Outcome |
|---|---|
| 0 | Buildable repository and controlled dependencies |
| 1 | Validated audio, duplex, drift, and identity prototypes |
| 2 | Controller, configuration, devices, and profiles |
| 3 | Production simplex, duplex, and multiple-hole routing |
| 4 | Recovery, failover, and transactional switching |
| 5 | Easy Scan, Advanced Scan, and recommendations |
| 6 | Complete graphical configuration interface |
| 7 | Tray, notifications, and configurable startup |
| 8 | Diagnostics and optional external control |
| 9 | Latency, stability, portability, and performance validation |
| 10 | Packaging, documentation, and release candidate |
| 11 | Post-v0.1 development planning |

---

## 14. Immediate Next Actions

Before beginning implementation:

1. Review this documentation reconciliation and keep O-001 through O-006 explicitly Open until owner resolution.
2. Obtain implementation/prototype authorization; none is granted by this documentation update.
3. Follow the Reconciled Working Sequence: foundation, simplex, independent clocks/latency, backend/exclusive comparison, recovery/identity, then duplex.
4. Record measured evidence and limitations before final backend, GUI, DSP, or probe-isolation commitment.
5. Resolve material schema/product choices before their production implementation; continue only independent authorized work around unresolved decisions.

---

## 15. Guiding Principle

**AudioHole should become a reliable audio-routing tool through a sequence of small, verifiable implementations, not a large speculative build.**

Every milestone should answer one of three questions:

- Does the required behavior work?
- Is the implementation reliable enough to build upon?
- Can the user understand, control, and troubleshoot it?

The roadmap should remain adaptable as testing reveals real constraints, without compromising AudioHole's established design principles.
---

## v0.1 Setup Diagnostics — Signal Activity Acceptance

- Add an off-by-default **Show Signal Activity** control to the hole setup/troubleshooting GUI.
- Verify independent source activity indications for forward and reverse duplex directions.
- Verify the indicator reacts to present and absent source samples without falsely labeling silence as disconnection.
- Verify stopped/unavailable/not-monitored conditions are distinguishable from silent samples.
- Verify closing the panel stops monitoring; routing continues unaffected.
- Verify no additional capture stream, saved audio, or logging of samples.
- Verify monitoring does not materially increase audio callback latency or introduce allocations/blocking.
- Verify neutral status semantics in Default, Vanilla, and UwU personality modes.
- The separate user-triggered **Test Beep** feature is in v0.1 scope and has its own acceptance criteria below.


---

## v0.1 Setup Diagnostics — Test Beep Acceptance

- Display **Test Beep** for a specifically identified destination playback endpoint in hole setup/troubleshooting.
- For duplex holes, independently select and identify the intended forward or reverse destination.
- Verify a short, quiet, single beep (default approximately 1 kHz, -30 dBFS, one second, with fades) is sent only to the selected endpoint.
- Verify no beep plays without a user click, no repeated/overlapping beeps occur, and Stop cancels immediately.
- Verify capture-only, missing, and exclusive-busy endpoints receive no beep and produce actionable feedback; no silent fallback to a different device.
- Verify routing, volume, gain, saved profiles, and device bindings are unchanged before and after testing.
- Verify cleanup on panel closure, Stop Routing, profile switch, device disappearance, and shutdown.
- Verify beep availability and precise target/error text in Normal, Vanilla, and UwU modes.
- Verify successful playback submission is not presented as proof of audibility or successful end-to-end routing.

## Personality catalog acceptance

v0.1 acceptance includes the authored event coverage and three-mode tests in `PERSONALITY.md` §16, including distinct UwU text in main GUI, setup diagnostics, profiles, tray, Test Beep, and DFA; no technical-string mutation; and identical operational behavior across modes.


---

## Milestone 0 implementation and validation record

**Status at initial implementation:** Foundation implemented; native Windows/MSVC
acceptance was pending in this local record. The owner subsequently reported
Milestone 0.0 Windows CI passing (see Milestone 0.1 below). This historical
record does not authorize Milestone 1.

The implementation adds C++20 CMake targets for a Windows COM initialization
library, a temporary developer executable, four GoogleTest COM lifetime cases,
and a CTest harness-launch check. Visual Studio 2022 x64 Debug/Release presets,
build instructions, build-artifact ignore rules, a pinned test-only GoogleTest
source/notice, and a Windows build/test workflow accompany the code. No audio
streams, GUI/tray, device discovery/probing, profiles, persistence, controller
scaffolding, or public CLI are implemented. D-030, D-034, O-005, and other open
decisions remain Open. Project license selection remains pending before
public distribution.

Actual validation in the Linux onboarding environment:

| Check | Outcome |
|---|---|
| Windows x64 Debug and Release cross-configure/build/link | Passed using MinGW-w64 GCC 14 (Debian 14.2.0-19+27+b1), CMake 3.31.6, Ninja 1.11.1; both application and test executables produced. This is not MSVC validation. |
| Preset syntax / unsupported host | CMake parses the presets; native Linux configuration rejects the unsupported target with an actionable message. |
| Native MSVC Debug/Release builds | Not run: no MSVC/Windows SDK installation is available on this host. |
| Application launch and four COM tests / CTest launch test | Not run on Windows. A supplemental Wine 10 runtime attempt could not initialize Wine's Windows runtime; it established no application/test result. |
| Windows workflow | Added, not remotely executed during this task. It builds/tests both configurations; CTest also launches the harness. |

Validation tools and temporary cross-build outputs are outside tracked source;
no generated binaries, dependencies, or build directories are committed.
The MinGW validation tools and Wine are not application dependencies. Cross-build
MinGW runtime DLLs used only for the attempted Wine check do not describe the
reference MSVC static-CRT deployment.

Run the documented commands in [BUILDING.md](BUILDING.md) on Windows with
Visual Studio 2022 and record the compiler/SDK versions, executable exit status,
and five CTest results in both configurations before claiming native readiness.
The COM helper supplies thread-bound initialization/cleanup for future WASAPI
workers without selecting an audio backend or synchronization algorithm. Keep
subsequent planning proportional to the next authorized build/test step.

## Milestone 0.1 — Development checks and private evaluation packages

This owner-authorized infrastructure milestone builds on Milestone 0.0; it does
not implement audio routing or replace the earlier phase inventory. The owner
reported native Windows/MSVC Debug/Release builds and tests passing for the
Milestone 0.0 baseline, with PR #1 open against `main`.

Milestone 0.1 adds a read-only PowerShell prerequisite verifier, a build/test/
launch/package helper, explicit CMake Runtime installation, and separate
revision-identified Debug/Release Actions artifacts. Packages exclude tests and
third-party source. CI tests the exact package outside the source/build tree
and audits compiler-runtime imports before upload; Release is preferred for
ordinary manual evaluation. See [BUILDING.md](BUILDING.md).

Validation must distinguish local script/fixture checks from native CI builds,
package execution, and upload. GitHub API access in the current environment
returns `Forbidden`; the private Actions page is also unavailable through the
available unauthenticated web route. Git transport remains available. These
observations do not establish a Windows build or workflow failure. Confirm the
new commit's Windows CI and artifact upload before treating 0.1 as validated.

The next owner-authorized milestone is 0.2: a native Windows GUI backed by an
explicit deterministic simulator, before actual routing. This revises the
immediate execution order: 0.1 must pass Windows CI before 0.2 begins. The
simulator must not claim real device observations, finalize Open policies or
production schemas, or implement WASAPI capture/render. Keep 0.1 and 0.2 in
separate commits on `work`, preserve PR #1, and do not merge or create Releases.


Local Milestone 0.1 checks passed using PowerShell 7.4.6 and CMake 3.31.6
on Linux: script parsing, temporary-fixture ZIP content/metadata checks,
existing-archive protection, unexpected-runtime rejection and staging cleanup,
and explicit refusal of unsupported native validation. These are tooling checks,
not Windows application execution or confirmation of Actions artifact uploads.
