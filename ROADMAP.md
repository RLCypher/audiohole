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

The roadmap is organized by dependencies and validation gates rather than calendar deadlines.

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

## 13. Implementation Sequence Summary

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

1. Reconcile `AGENTS.md` against the finalized specifications.
2. Establish the repository and build environment.
3. Evaluate the audio backend candidates.
4. Prototype simplex and duplex routing.
5. Validate drift compensation and device identity.
6. Record the resulting technical decisions.
7. Begin the production architecture.

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
