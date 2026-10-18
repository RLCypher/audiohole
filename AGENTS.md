# AudioHole — Agent Instructions

**Document:** `AGENTS.md`  
**Applies to:** AudioHole repository and all subdirectories  
**Status:** Reconciled for v0.1 development

---

## 1. Project Mission

AudioHole is an open-source Windows application that connects arbitrary audio endpoints as though they were connected by physical audio cables.

Connections may be:

- **Simplex:** One directional audio path.
- **Duplex:** Two associated directional paths forming one logical connection.

The application must provide reliable, low-latency audio routing without requiring virtual audio drivers, administrator privileges, or proprietary routing infrastructure.

AudioHole is intended to remain useful across Windows reinstalls, hardware upgrades, device reenumeration, and migration to another computer.

**The central requirement is that configuration remains portable, understandable, and manually editable.**

Implementation choices must serve this objective.

---

## 2. Required Reading

Before implementing a feature, read the relevant specifications.

The repository documentation is the authoritative design reference.

| Document | Responsibility |
|---|---|
| `SPEC.md` | Product requirements and overall behavior |
| `ARCHITECTURE.md` | Component boundaries and runtime architecture |
| `DEPENDENCIES.md` | Dependency requirements and evaluation |
| `DEVICE_PROBING.md` | Device discovery, scanning, and capability verification |
| `CONFIG_FORMAT.md` | Configuration structure, portability, and serialization |
| `PROFILES.md` | Profiles, holes, activation, recovery, and switching |
| `API.md` | Internal and optional external control interfaces |
| `GUI.md` | Graphical configuration and diagnostic interface |
| `TRAY.md` | Background operation, tray controls, and notifications |
| `DECISIONS.md` | Accepted, provisional, open, and deferred decisions |
| `ROADMAP.md` | Development milestones and validation gates |

Do not attempt to infer a feature's complete behavior from this file alone.

Read the relevant detailed specification before modifying its implementation.

---

## 3. Instruction Precedence

When documents appear to conflict, use the following procedure:

1. Identify the conflicting requirements.
2. Check `DECISIONS.md` for an accepted or superseding decision.
3. Check the relevant detailed specification.
4. Determine whether the conflict is an editorial inconsistency or a genuine design conflict.
5. Resolve minor editorial inconsistencies without changing behavior.
6. Escalate genuine design conflicts before implementing a behavior-changing choice.

`AGENTS.md` governs implementation conduct.

It does not silently override accepted product requirements.

`DECISIONS.md` records deliberate design decisions.

Detailed specifications define the behavior of their respective subsystems.

The roadmap establishes implementation order, not permission to ignore requirements.

---

## 4. Decision Authority

Every significant technical decision must respect its status in `DECISIONS.md`.

### 4.1 Accepted

Implement according to the accepted decision.

Do not silently replace it with a more convenient alternative.

### 4.2 Provisional

Evaluate the preferred approach.

Do not treat it as permanently selected without validation.

### 4.3 Open

Investigate the alternatives.

For decisions with substantial architectural consequences:

- Identify the alternatives.
- Explain relevant tradeoffs.
- Recommend a validation method.
- Build a focused prototype when authorized.
- Record findings.
- Obtain approval before committing to a permanent approach.

### 4.4 Deferred

Do not implement deferred features without explicit authorization.

### 4.5 Routine Implementation Decisions

Agents may independently make ordinary, reversible implementation choices that:

- Do not contradict accepted requirements.
- Do not introduce substantial dependencies.
- Do not create persistent compatibility obligations.
- Do not redefine configuration schemas.
- Do not change externally visible behavior.

Avoid unnecessary approval requests for routine coding decisions.

---

## 5. Development Workflow

Work incrementally.

Prefer small, verifiable changes over broad speculative rewrites.

### 5.1 Before Editing

1. Identify the relevant milestone.
2. Read the affected specifications.
3. Inspect the existing implementation.
4. Identify applicable decisions.
5. Determine the expected behavior.
6. Identify tests that should pass.

### 5.2 During Implementation

- Maintain clear component boundaries.
- Keep changes focused.
- Avoid unrelated refactoring.
- Preserve existing configuration compatibility.
- Add diagnostics for meaningful failures.
- Avoid introducing unnecessary dependencies.
- Do not implement unrelated future features.

### 5.3 After Implementation

1. Build the affected targets.
2. Run relevant tests.
3. Verify acceptance criteria.
4. Document limitations.
5. Identify unresolved issues.
6. Update relevant documentation when behavior changes.
7. Report what was actually verified.

Do not claim a feature is complete merely because the code compiles.

---

## 6. Language and Build System

AudioHole v0.1 uses:

- C++.
- CMake.
- Windows 10/11 x64.

Use modern C++ practices appropriate to the selected compiler baseline.

Prefer:

- RAII for resource management.
- Explicit ownership.
- Clear interfaces.
- Bounded resource use.
- Standard library facilities where suitable.
- Strong error handling.
- Predictable cleanup.

Avoid unnecessary global mutable state.

Do not introduce a second build system without an accepted architectural reason.

---

## 7. Architecture Boundaries

Maintain the separation established in `ARCHITECTURE.md`.

Primary responsibilities include:

- Application controller.
- Routing supervisor.
- Audio engine.
- Device discovery and registry.
- Device probing.
- Profile management.
- Configuration persistence.
- Diagnostics.
- GUI.
- System tray.
- Optional external-control adapters.

### 7.1 Audio Engine Independence

The audio engine must not depend on:

- GUI widgets.
- Tray menus.
- Windows toast notifications.
- Profile editor controls.
- Filesystem serialization.
- Scan-console rendering.

### 7.2 Interface Independence

The GUI and tray must not implement separate routing engines.

They issue commands to the authoritative application controller.

### 7.3 Device Discovery Independence

Device enumeration and probing must not be embedded directly in profile editor widgets.

### 7.4 Structured Communication

Prefer structured commands, results, status snapshots, and events over ad hoc communication between subsystems.

---

## 8. Real-Time Audio Safety

Real-time audio processing must remain predictable and bounded.

### 8.1 Prohibited Callback Operations

Do not perform the following directly inside real-time audio callbacks:

- Filesystem operations.
- JSON parsing or serialization.
- GUI updates.
- Tray updates.
- Toast notifications.
- Blocking synchronization.
- Network requests.
- Unbounded logging.
- Unbounded memory allocation.
- Potentially blocking device enumeration.
- Configuration migration.

### 8.2 Buffering

Use bounded buffering and explicit underrun/overrun handling.

Allocate resources outside real-time processing where practical.

### 8.3 Independent Clocks

Do not assume capture and playback devices share a clock.

Implement and validate drift compensation according to the selected audio architecture.

### 8.4 Latency

AudioHole targets attributable processing latency of 10 ms or less where supported.

Five milliseconds is a stretch objective.

These figures do not describe the entire end-to-end latency of external hardware and software.

Never sacrifice basic stability merely to advertise a smaller buffer.

### 8.5 Measurements

Report measured values as measured.

Report estimates as estimates.

Do not invent latency, CPU, stability, or device-capability results.

---

## 9. Configuration Transparency

All user-facing configuration must remain human-readable and manually editable.

Follow `CONFIG_FORMAT.md`.

### 9.1 Canonical Format

Use UTF-8 JSON for structured persistent configuration.

Follow the established:

- Directory layout.
- Naming conventions.
- Field names.
- Schema versions.
- Serialization rules.
- Migration rules.

Do not introduce an incompatible parallel format.

### 9.2 No Opaque State

Do not store essential user configuration exclusively in:

- Binary databases.
- Registry-only values.
- GUI framework settings.
- Undocumented caches.
- Machine-specific serialized objects.

Machine-local caches may exist where specified.

They must not replace portable canonical configuration.

### 9.3 Human-Readable Organization

Files should have understandable names and predictable locations.

Users must be able to inspect, back up, edit, and transfer their configurations.

### 9.4 Atomic Writes

Persistent configuration writes must be protected against partial or interrupted updates.

Follow the established atomic-write and backup strategy.

### 9.5 External Edits

Do not silently overwrite externally modified configuration.

Detect conflicts and handle them according to the specification.

### 9.6 Schema Changes

Do not silently introduce new persistent fields or reinterpret existing fields.

Schema changes require:

- Documented rationale.
- Compatibility analysis.
- Migration behavior where necessary.
- Relevant tests.
- Specification updates.

---

## 10. Device Identity and Portability

Device identity is a foundational requirement.

### 10.1 Stable Logical IDs

Profiles must reference stable logical device identifiers.

Do not use transient Windows endpoint IDs as the sole portable identity.

### 10.2 Device Fingerprints

Use available durable attributes according to `CONFIG_FORMAT.md` and `DEVICE_PROBING.md`.

Potential evidence includes:

- Vendor and product identifiers.
- Serial numbers.
- Hardware identifiers.
- Interface information.
- Endpoint direction.
- Manufacturer.
- Other relevant properties.

### 10.3 Machine-Local Bindings

Transient Windows endpoint identifiers belong in machine-local binding data.

### 10.4 Ambiguous Matches

Never silently bind a saved logical device to an ambiguous physical endpoint.

Require confirmation where the matching policy cannot establish sufficient confidence.

### 10.5 Missing Devices

Do not delete saved device records merely because a device is disconnected.

### 10.6 Migration

Design device matching to tolerate:

- USB port changes.
- Driver updates.
- Windows reinstalls.
- Hardware replacement.
- Configuration transfer to another PC.

Do not claim guaranteed matching when the hardware provides insufficient identifying information.

---

## 11. Profiles and Holes

Follow `PROFILES.md`.

### 11.1 Hole Definition

A hole is one logical connection.

A simplex hole contains one directional path.

A duplex hole contains two associated directional paths.

A duplex hole counts as one hole.

### 11.2 Directional Independence

Each duplex direction may have independent:

- Endpoints.
- Format settings.
- Buffer settings.
- Runtime status.
- Failure conditions.

### 11.3 Active-Hole Limits

Normal operating range: one to four active holes.

Extended range: five to eight active holes, with the defined advisory.

More than eight active holes are unsupported in v0.1.

### 11.4 Mirrored Destinations

Mirrored output destinations are represented by separate holes.

Do not silently introduce implicit multi-destination routing.

### 11.5 Notes

Preserve user-editable notes on device and profile records.

Support hole notes as established in the GUI and profile model.

Notes must not affect routing behavior.

---

### 11.6 Directional Audio Configuration

Each direction owns its effective audio settings. Optional shared hole-level `audio_defaults` may provide inherited values, but explicit directional overrides take precedence. Do not implement duplex format, buffer, or operating mode as one inseparable shared setting. Preserve inheritance and overrides in JSON, GUI editing, and migration (D-053).

### 11.7 Hole Quick Toggles

Individual hole enable/disable actions through tray or external API are deferred beyond v0.1. Hole-status inspection remains supported. Do not implement quick toggles without explicit authorization (D-038).

---

## 12. Routing State Machine

Use the canonical states:

- Stopped.
- Ready.
- Starting.
- Running.
- Degraded.
- Recovering.
- Failed.
- Stopping.

### 12.1 Ready

Ready means a valid profile is armed and waiting for required resources.

Ready is not inherently an error.

### 12.2 Running

Running must reflect confirmed operating routing paths.

A requested activation is not sufficient evidence.

### 12.3 Degraded

Degraded indicates partial loss of requested routing functionality.

### 12.4 Failed

Failed indicates a routing error preventing required operation.

The profile may remain armed for recovery.

### 12.5 Stop

Explicit Stop disarms routing and cancels pending recovery and activation.

### 12.6 State Ownership

The routing supervisor owns authoritative routing state.

GUI, tray, and API consumers must not maintain contradictory independent state machines.

---

## 13. Recovery and Failover

Automatic recovery is required.

### 13.1 Device Loss

Detect endpoint disappearance and stream interruption.

### 13.2 Device Return

Attempt recovery when a matching device becomes available.

### 13.3 Failover

Use only configured fallback candidates.

Do not substitute arbitrary endpoints.

### 13.4 Recovery Reporting

Report meaningful state transitions and failures through structured events.

### 13.5 Notification Deduplication

Do not generate repeated user notifications for every retry.

### 13.6 Explicit Stop

Stop Routing must cancel recovery.

### 13.7 Startup Delay Independence

Do not reapply the startup activation delay during ordinary device recovery.

---

## 14. Transactional Profile Switching

Profile switching must follow the established transactional behavior.

### 14.1 Preserve Working Routing

Do not unnecessarily replace a functioning profile with silence when the destination profile cannot be activated.

### 14.2 Validate Before Commit

Validate and prepare destination resources where possible.

### 14.3 Resource Conflicts

Account for exclusive-mode conflicts and other conditions that may prevent simultaneous preparation.

### 14.4 Rollback

Preserve or restore the previous working configuration where technically possible.

### 14.5 Explicit Ready Activation

Allow the user to deliberately arm an unavailable destination profile.

Do not make this the silent default after a failed switch.

### 14.6 Staged Edits

Routing-affecting edits to an active profile must be staged until Apply.

---

## 15. Device Scanning and Probing

Follow `DEVICE_PROBING.md`.

### 15.1 Easy Scan

Easy Scan is nonintrusive.

It gathers Windows-reported information without deliberately opening experimental streams.

### 15.2 Advanced Scan

Advanced Scan requires explicit user consent before potentially disruptive tests.

### 15.3 Evidence Classification

Distinguish:

- Reported capabilities.
- Experimentally verified capabilities.
- Unsupported configurations.
- Untested configurations.
- Failed tests.
- Inconclusive results.

### 15.4 Error Reporting

Preserve meaningful Windows error information, including HRESULTs where applicable.

### 15.5 History

Store scan history with timestamps and relevant test context.

### 15.6 Cancellation

Support cancellation without corrupting device records.

### 15.7 No Unsupported Claims

Do not describe a configuration as verified merely because Windows reports support or initialization succeeds.

---

## 16. Format and Buffer Recommendations

Recommendations must be evidence-based and explainable.

### 16.1 Audio Formats

Consider:

- Verified compatibility.
- Reported capabilities.
- Required conversion.
- Operating mode.
- Latency implications.
- Known limitations.

Do not automatically prefer the highest sample rate or bit depth.

### 16.2 Buffers

Balance latency against stability.

Do not unconditionally select the second or third smallest buffer.

Use actual constraints and test evidence.

### 16.3 Conservative Defaults

When evidence is incomplete, use an appropriate conservative fallback.

Label it Default rather than falsely claiming verification.

### 16.4 User Overrides

Permit supported manual overrides.

Preserve overrides where valid.

### 16.5 Explanations

Provide machine-readable recommendation reasons suitable for presentation by the GUI.

The GUI must display a concise explanation beneath the relevant control.

Technical details should remain available through tooltips.

### 16.6 Warning Information

Provide structured warning information for configurations likely to produce unfavorable results.

Do not embed all recommendation logic inside GUI widgets.

---

## 17. GUI Requirements

Follow `GUI.md`.

### 17.1 Primary Tabs

- Profiles.
- Devices.
- Scanning.
- Settings.

### 17.2 Add Hole Workflow

The ordinary workflow should be:

1. Add Hole.
2. Choose simplex or duplex.
3. Select source.
4. Select destination.
5. Accept recommended format and buffer settings.

Duplex configuration repeats the directional setup as required.

### 17.3 Technical Explanations

Technical controls should have tooltips.

Recommended settings should also have visible explanations.

### 17.4 Warnings

Use contextual caution indicators.

Explain the specific risk.

### 17.5 Notes

Expose device, profile, and hole notes.

### 17.6 Scanning Console

Provide an integrated verbose scan console based on structured events.

### 17.7 First-Run Introduction

Provide an optional guided introduction.

It must be skippable and rerunnable without resetting configuration.

### 17.8 Active Routing

Editing the GUI must not unexpectedly interrupt running audio.

### 17.9 Window Closure

Closing the GUI must not stop routing.

---

## 18. Tray Requirements

Follow `TRAY.md`.

### 18.1 Persistent Operation

The tray remains available while the GUI is closed.

### 18.2 Status

Display the actual overall routing state.

Use noncolor distinctions where practical.

### 18.3 Controls

Provide:

- Active profile information.
- Profile switching.
- Hole status.
- Start Routing.
- Stop Routing.
- Open AudioHole.
- Exit AudioHole.

### 18.4 Stop Versus Exit

Stop Routing leaves the application running.

Exit AudioHole stops routing and terminates the application.

### 18.5 Notifications

Support:

- All Events.
- Warnings and Errors.
- Errors Only.

Default to Warnings and Errors.

### 18.6 Silent Informational Notifications

Ready-to-Running informational notifications should be silent when enabled.

### 18.7 Recovery Notifications

Meaningful failures should be reported without notification spam.

### 18.8 Explorer Restart

Restore the tray icon after Windows Explorer restarts where supported.

---

## 19. Startup and Delayed Activation

AudioHole supports:

- Manual activation.
- Immediate activation.
- Delayed activation.

### 19.1 Windows Startup

Starting with Windows is optional and disabled by default.

### 19.2 Startup Profile

Support last-active or explicitly selected startup profiles.

### 19.3 Delayed Activation

The startup delay begins when AudioHole launches.

It does not begin at Windows boot time.

### 19.4 Countdown Ownership

The application controller owns the countdown.

GUI and tray display the same pending activation state.

### 19.5 Manual Override

Start Routing Now cancels the countdown and requests immediate activation.

### 19.6 Cancellation

Cancel Startup or Stop Routing disarms the pending activation.

### 19.7 Missing Hardware

If the timer expires while required hardware is unavailable, a valid profile enters Ready.

### 19.8 No Duplicate Activation

A canceled or bypassed timer must not subsequently trigger another activation.

### 19.9 Recovery Independence

The startup delay applies once per application launch.

It must not be repeated for ordinary reconnection or failover.

---

### 19.10 Profile Selection During Countdown

Changing the selected profile during delayed startup preserves the controller-owned deadline and updates the pending target. GUI and tray do not own independent timers. Obsolete callbacks must not activate a previously selected profile. Start Routing Now bypasses the timer; Stop Routing and Cancel Startup disarm it. The countdown is transient pending state while routing is Stopped, not a ninth canonical routing state (D-054, D-055).

---

## 20. Diagnostics and Logging

Diagnostics must be structured, human-readable, and useful.

### 20.1 Structured Events

Prefer structured events containing relevant context over unstructured strings alone.

### 20.2 Scan Diagnostics

Record device discovery and probing behavior.

### 20.3 Routing Diagnostics

Record:

- Stream initialization.
- Stream failures.
- Device loss.
- Recovery attempts.
- Failover transitions.
- Relevant buffer events.
- Available latency measurements.
- State transitions.

### 20.4 Real-Time Safety

Audio callbacks must not perform blocking logging operations.

### 20.5 Export

Support human-readable diagnostic export.

### 20.6 Privacy

Avoid collecting unnecessary personal information.

Do not expose unrelated private data in diagnostic exports.

### 20.7 Honesty

Never fabricate hardware results, measurements, or successful tests.

---

## 21. Optional External Control

Follow `API.md`.

### 21.1 Shared Controller

External control adapters must use the same application controller as the GUI and tray.

### 21.2 No Direct Audio Manipulation

External requests must not manipulate audio buffers or stream internals directly.

### 21.3 Transport Selection

Windows named pipes, CLI wrappers, and localhost adapters remain subject to the decisions register.

Do not permanently select an external transport without the required decision process.

### 21.4 Security

Externally accessible control must use appropriate local permissions and be disabled by default where specified.

### 21.5 Command Semantics

Start, Stop, Activate Profile, and status queries must behave consistently across interfaces.

### 21.6 Optional Scope

Do not allow optional external-control features to delay essential routing functionality unnecessarily.

---

## 22. Dependencies

Follow `DEPENDENCIES.md` and `DECISIONS.md`.

### 22.1 No Silent Dependency Selection

Do not permanently commit to an unresolved major dependency without validation and approval.

### 22.2 Minimize Dependencies

Prefer dependencies that materially reduce implementation risk or maintenance burden.

### 22.3 Licensing

Verify license compatibility with the project's selected open-source license.

### 22.4 Version Control

Pin or otherwise control dependency versions.

### 22.5 Reproducibility

Builds should not depend on undocumented local machine state.

### 22.6 Candidate Technologies

Current candidates include:

- Native WASAPI.
- miniaudio.
- Hybrid audio backend.
- Qt.
- wxWidgets.
- Dear ImGui.
- nlohmann/json.

Candidate status does not imply acceptance.

---

## 23. Testing Requirements

Testing must accompany implementation.

### 23.1 Unit Tests

Use unit tests for:

- Configuration parsing.
- Schema validation.
- Profile validation.
- Device matching.
- Recommendation logic.
- State transitions.
- Command handling.

### 23.2 Integration Tests

Use integration tests for:

- Controller and routing supervisor.
- Configuration persistence.
- Device registry.
- Profile activation.
- Recovery behavior.
- GUI/tray state synchronization.

### 23.3 Hardware Tests

Use real hardware to validate:

- Capture and playback.
- Duplex operation.
- Clock drift.
- Low-latency performance.
- Disconnection and reconnection.
- Sleep and wake.
- Driver behavior.

### 23.4 Hardware Availability

If required hardware is unavailable, document the limitation.

Do not substitute mocked success for real hardware verification.

### 23.5 Acceptance Criteria

Use the relevant specification's acceptance tests.

### 23.6 Regression Prevention

Add regression tests for discovered defects where practical.

---

## 24. Prototype Rules

Prototypes should resolve specific uncertainties.

### 24.1 Focus

Each prototype should have a defined question and success criteria.

### 24.2 Isolation

Keep experimental code distinguishable from production components.

### 24.3 Results

Record:

- What was tested.
- Hardware and environment.
- Observed behavior.
- Measurements.
- Failures.
- Limitations.
- Conclusions.

### 24.4 Promotion

Do not automatically promote prototype code into production.

Review it against the accepted architecture and coding requirements first.

### 24.5 Decision Updates

Use validated prototype findings to inform `DECISIONS.md`.

---

## 25. Documentation Maintenance

Documentation is part of the implementation.

### 25.1 Behavior Changes

If implementation requires a change to established behavior, obtain approval and update the relevant specification.

### 25.2 Schema Changes

Update `CONFIG_FORMAT.md` and migration tests.

### 25.3 API Changes

Update `API.md`.

### 25.4 GUI Changes

Update `GUI.md`.

### 25.5 Tray Changes

Update `TRAY.md`.

### 25.6 Technical Decisions

Update `DECISIONS.md` when a decision is accepted, revised, or superseded.

### 25.7 Milestones

Update `ROADMAP.md` as implementation milestones are completed or reorganized.

### 25.8 No Fictional Completion

Do not mark unimplemented features as complete.

Do not claim tests passed unless they were actually executed.

---

## 26. Scope Control

AudioHole v0.1 is not intended to become:

- A full digital audio workstation.
- A general-purpose mixing console.
- A network audio transport.
- A virtual audio driver.
- A cross-platform audio framework.
- An unrestricted audio graph editor.

Do not expand scope without explicit authorization.

The priority is reliable routing between existing Windows audio endpoints.

---

## 27. Agent Communication

When reporting implementation work, include:

### Completed

What was implemented.

### Verified

What was built or tested and the actual results.

### Limitations

What remains incomplete or unverified.

### Decisions Needed

Any unresolved architectural choices requiring approval.

### Next Step

The next concrete implementation or validation action.

Avoid vague completion claims.

Do not describe speculative behavior as implemented functionality.

---

## 28. Stop-and-Ask Conditions

Pause and request clarification when a task would require:

1. Contradicting an accepted decision.
2. Choosing a major unresolved architectural dependency.
3. Changing the canonical configuration schema incompatibly.
4. Introducing mandatory administrator privileges.
5. Introducing a mandatory virtual audio driver.
6. Silently substituting ambiguous devices.
7. Removing or weakening configuration portability.
8. Changing the meaning of a simplex or duplex hole.
9. Weakening real-time audio safety.
10. Changing transactional profile-switching behavior.
11. Introducing an undocumented externally accessible control interface.
12. Implementing a deferred feature.
13. Discarding user configuration or diagnostic history unexpectedly.
14. Replacing working functionality with an unvalidated rewrite.

For ordinary reversible implementation details, proceed without unnecessary interruption.

---

## 29. Definition of Done

A feature is complete only when:

1. It implements the relevant accepted requirements.
2. It builds successfully.
3. Applicable automated tests pass.
4. Relevant hardware tests pass or limitations are documented.
5. Errors and edge cases are handled.
6. Diagnostics are sufficient for troubleshooting.
7. Configuration remains compatible and transparent.
8. Documentation reflects actual behavior.
9. No unrelated regressions are known.
10. Remaining limitations are explicitly reported.

A working demonstration is not automatically a finished feature.

---

## 30. Final Directive

**Build AudioHole as a reliable, transparent, portable audio-routing application.**

Prioritize:

- Correctness over convenience.
- Stability over aggressive latency claims.
- Measured evidence over assumptions.
- Human-readable configuration over opaque state.
- Durable device identity over transient Windows identifiers.
- Explicit user control over surprising automation.
- Recoverable failures over silent breakage.
- Small verified milestones over speculative complexity.

The implementation should remain understandable to both its maintainers and its users.

**If a design decision is uncertain, investigate it. If a requirement is established, preserve it. If a behavior is unverified, say so.**

## Personality and User-Facing Copy

Read `PERSONALITY.md` before writing user-facing copy. The application has one `interface.personality_mode` setting with three mutually exclusive values: `default` (Normal — Default Audio Engineer), `vanilla` (Vanilla — Strictly informational), and `uwu` (UwU — I know what you are). Do not implement a separate Vanilla boolean.

Use centralized, intentionally authored message variants, with neutral fallback; never rewrite user input, endpoint names, technical identifiers, paths, or structured diagnostics. Personality never overrides safety, transparency, accessibility, or functional requirements. Changing modes must not affect routing. The intentionally inert DFA (Does Fuck-All) button appears in Settings → Advanced in Normal and UwU, and is hidden in Vanilla. It must not invoke controller commands or change persistent state.

---

## Temporary Signal Activity Monitoring Guardrail

For v0.1, implement only **temporary source-side signal-presence observation** in setup/troubleshooting. This is not a calibrated level meter, mixer, or audio recorder. The separately specified user-triggered Test Beep is not part of passive signal observation. The indicator must be off by default and cease monitoring when its GUI panel closes. Use existing routed samples, with real-time-safe computation and throttled UI updates. Do not open duplicate capture streams, alter audio routing, or store samples. Silence is not disconnection; detected source samples are not proof of destination playback. See `GUI.md`, `ARCHITECTURE.md`, and `SPEC.md`.


---

## Setup Test Beep Guardrail (v0.1)

Implement a clearly labeled **Test Beep** action for an explicitly selected **destination playback endpoint** in hole setup/troubleshooting. The purpose is only to answer: *Did the beep come out of the intended device?* Do not substitute the beep into an unrelated microphone/capture direction or broadcast to every output. A duplex hole must allow the user to select the destination direction being tested. If the selected endpoint cannot safely render a test signal, explain why and do not silently switch endpoints.

The beep must be a brief, quiet, bounded signal (default approximately 1 kHz, -30 dBFS, one second) with short fades, an explicit click to start, a visible active indicator, and immediate cancellation on Stop, profile switch, device removal, or shutdown. Never persist an armed test. Use the existing playback pipeline when feasible; when routing is stopped, a temporary playback session may be used if supported. Resolve conflicts with exclusive-mode endpoints explicitly. Never disturb live routing, raise device volume, alter configuration, or claim a successful beep proves the full route works. Test-beep presentation follows `PERSONALITY.md`, but warnings and endpoint names remain precise.

## Personality coverage directive

Implement `PERSONALITY.md` §16 as normative v0.1 UI copy coverage, not merely an optional tone example. Use a centralized, authored three-mode presentation catalog with the specified common events and tests. UwU is intentionally strongly flirtatious and innuendo-heavy in noncritical UI copy, with frequent deliberate R/L→W styling and kaomoji; preserve technical strings and safety clarity. Do not apply runtime mutation to arbitrary text.

---

## Specification reconciliation guardrail

Follow the owner-authorized reconciliation in `DECISIONS.md`. D-019's eight public states are canonical; resolving is internal to Starting. Capacity counts enabled holes in the armed plan, including waiting/recovering holes, and does not guarantee hardware performance.

Native event-driven WASAPI and wxWidgets are leading candidates, not finalized selections. Toolchain/library approvals are provisional; no implementation or prototype is authorized by the documentation reconciliation itself. Public transport remains deferred until core routing is proven.

Preserve unmodified samples whenever technically feasible without sacrificing reliable continuous operation. Destination pacing, bounded FIFOs, and adaptive synchronization are the baseline. O-005 keeps direct-transfer eligibility, instability thresholds, and response policy Open; do not invent automatic resampling transitions, direct-only preferences, or bit-perfect guarantees.

O-001 through O-006 remain unresolved owner decisions. Recommendations about fallback health, switch preparation, close/tray preferences, production schemas, direct transfer, and storage roots are not accepted requirements. Preserve prior behavior while flagging material choices. Stop takes precedence over pending asynchronous routing, scan, and startup work through generation-based stale-result rejection. Respect all existing real-time, configuration, personality, activity-observation, and Test Beep guardrails.
