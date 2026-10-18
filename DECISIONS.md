# AudioHole v0.1 — Technical Decisions Register

**Document:** `DECISIONS.md`  
**Status:** Working decision register  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `DEPENDENCIES.md`, `DEVICE_PROBING.md`, `CONFIG_FORMAT.md`, `PROFILES.md`, `API.md`, `GUI.md`, `TRAY.md`, `ROADMAP.md`

---

## 1. Purpose

This document records the architectural, technical, and product decisions governing AudioHole v0.1.

Its purpose is to distinguish:

- Requirements that have already been established.
- Technical approaches that are provisionally preferred.
- Decisions requiring investigation or experimentation.
- Features intentionally deferred beyond the initial release.

AudioHole must not allow implementation convenience to silently redefine its requirements.

The central product objective remains:

**Connect arbitrary Windows audio devices as though they were connected by physical audio cables, including independent bidirectional connections where supported.**

AudioHole is not intended to create new virtual audio endpoints or require proprietary routing infrastructure.

---

## 2. Decision Status Definitions

Each decision should use one of the following statuses.

| Status | Meaning |
|---|---|
| **Accepted** | Established design decision; implementation should follow it |
| **Provisional** | Preferred direction, subject to validation |
| **Open** | No approach selected; investigation required |
| **Deferred** | Intentionally outside the current implementation scope |
| **Superseded** | Replaced by a later documented decision |

### 2.1 Decision Ownership

Changes to accepted decisions should be deliberate and documented.

Implementation agents may recommend changes but must not silently override accepted decisions.

### 2.2 Decision Records

Significant decisions should include:

- Identifier.
- Status.
- Decision.
- Rationale.
- Alternatives.
- Consequences.
- Validation requirements.
- Related documentation.

### 2.3 Decision History

Superseded decisions should remain in the register with a reference to their replacements.

The document should preserve why a choice was made, not merely what was chosen.

---

# Part I — Accepted Decisions

## D-001 — Supported Platform

**Status:** Accepted

**Decision:** AudioHole v0.1 targets Windows 10 and Windows 11 on x64 systems.

**Rationale:** The application is designed around Windows audio endpoint discovery and routing.

**Consequences:**

- Windows audio APIs are first-class integration targets.
- Cross-platform abstraction is not a v0.1 requirement.
- Platform-specific implementations are acceptable where justified.

---

## D-002 — Implementation Language

**Status:** Accepted

**Decision:** The primary implementation language is C++.

**Rationale:** AudioHole requires low-level Windows audio integration, predictable performance, and direct control over real-time processing.

**Consequences:**

- CMake is the intended build system.
- External dependencies should be selected for maintainability and compatibility.
- Audio processing must avoid inappropriate runtime allocation and blocking behavior.

---

## D-003 — Normal User Privileges

**Status:** Accepted

**Decision:** Ordinary routing operations must not require administrator privileges.

**Rationale:** AudioHole should behave as a conventional desktop application.

**Consequences:**

- No mandatory kernel drivers.
- No mandatory privileged background service.
- No routine elevation prompts.
- Features requiring elevation should be avoided or explicitly identified.

---

## D-004 — No Mandatory Virtual Devices

**Status:** Accepted

**Decision:** AudioHole connects existing Windows audio endpoints without requiring installation of a virtual audio driver.

**Rationale:** The primary use case is connecting arbitrary existing audio devices.

**Consequences:**

- The initial implementation operates through supported user-mode audio interfaces.
- Creating new virtual audio endpoints is outside the v0.1 objective.
- Documentation must not imply that AudioHole is a virtual audio driver.

---

## D-005 — Human-Readable Configuration

**Status:** Accepted

**Decision:** User-facing configuration, profiles, device records, and diagnostic information must remain human-readable and logically organized.

**Rationale:** Transparency, portability, troubleshooting, and manual editability are core product requirements.

**Consequences:**

- UTF-8 JSON is the canonical structured configuration format.
- Configuration must not depend on an opaque database.
- Files should have understandable names and directories.
- Manual editing must remain practical.
- The GUI and optional CLI must operate on the same underlying configuration model.

**Related:** `CONFIG_FORMAT.md`

---

## D-006 — Portable Device Identity

**Status:** Accepted

**Decision:** Device identity must not depend solely on transient Windows endpoint identifiers.

**Rationale:** Users should be able to move AudioHole configurations between installations and computers.

**Consequences:**

- Stable logical device IDs.
- Device fingerprints using durable available attributes.
- Machine-local endpoint binding cache.
- Explicit handling of ambiguous matches.
- Preservation of disconnected device records.
- No silent substitution of unrelated devices.

**Related:** `CONFIG_FORMAT.md`, `DEVICE_PROBING.md`

---

## D-007 — Hole Abstraction

**Status:** Accepted

**Decision:** A hole is one logical audio connection.

A hole may be:

- Simplex: one directional path.
- Duplex: two associated directional paths.

**Rationale:** Duplex routing should be represented as one user-facing connection rather than two unrelated objects.

**Consequences:**

- Duplex holes count as one hole.
- Each direction can have independent technical settings.
- Each direction can have independent runtime status.
- The GUI and tray must preserve this distinction.

---

## D-008 — Routing Scale

**Status:** Accepted

**Decision:** AudioHole v0.1 is designed primarily for one to four simultaneously active holes.

Five to eight active holes are permitted with a one-time advisory.

More than eight simultaneously active holes are unsupported in v0.1.

**Rationale:** The initial release should prioritize reliability and predictable behavior rather than unrestricted routing scale.

**Consequences:**

- Profiles may contain additional disabled holes.
- Mirrored destinations use separate holes.
- The routing supervisor must enforce the active-hole limit.

---

## D-009 — GUI-First Operation

**Status:** Accepted

**Decision:** The primary configuration interface is graphical.

**Rationale:** AudioHole should be approachable without command-line knowledge.

**Consequences:**

- Profile creation uses a simple Add Hole workflow.
- Endpoint selection uses readable dropdowns.
- Technical settings have explanations.
- Advanced functionality is progressively disclosed.
- Command-line control is optional rather than mandatory.

**Related:** `GUI.md`

---

## D-010 — Independent Interface Sections

**Status:** Accepted

**Decision:** The main GUI has four primary tabs:

1. Profiles.
2. Devices.
3. Scanning.
4. Settings.

**Rationale:** Device management, scanning, and application preferences should not be buried inside profile editing.

**Related:** `GUI.md`

---

## D-011 — First-Run Introduction

**Status:** Accepted

**Decision:** First-time users receive an optional guided introduction.

**Requirements:**

- Explain simplex and duplex holes.
- Offer Easy Scan.
- Guide creation of a first profile.
- Explain recommended settings.
- Permit skipping.
- Permit rerunning from Settings.
- Preserve existing configuration when rerun.

**Related:** `GUI.md`

---

## D-012 — Explainable Recommendations

**Status:** Accepted

**Decision:** Automatically selected technical settings must be clearly labeled and explained.

**Requirements:**

- Distinguish Default, Recommended, and User Selected.
- Provide visible explanations beneath recommended format and buffer controls.
- Provide technical tooltips.
- Identify evidence limitations.
- Warn about unfavorable overrides.
- Never claim experimental verification without supporting evidence.

**Related:** `GUI.md`

---

## D-013 — Nonintrusive Easy Scan

**Status:** Accepted

**Decision:** Easy Scan collects Windows-reported information without deliberately opening disruptive experimental streams.

**Rationale:** Ordinary device discovery should be safe and convenient.

**Related:** `DEVICE_PROBING.md`

---

## D-014 — Explicit Advanced Scan

**Status:** Accepted

**Decision:** Advanced Scan requires explicit user consent before potentially disruptive probing.

**Requirements:**

- Explain possible interruptions.
- Support cancellation.
- Preserve meaningful partial results.
- Record actual test outcomes.
- Distinguish reported, supported, and verified capabilities.
- Retain diagnostic history.

**Related:** `DEVICE_PROBING.md`

---

## D-015 — Integrated Scan Console

**Status:** Accepted

**Decision:** The Scanning tab includes an integrated verbose diagnostic console.

**Rationale:** Technical users should be able to inspect discovery and probing without requiring an external terminal.

**Consequences:**

- Structured diagnostic events.
- Human-readable console presentation.
- Filtering, copying, and exporting.
- Persistent scan history.
- No blocking of real-time audio processing.

---

## D-016 — Real-Time Audio Safety

**Status:** Accepted

**Decision:** The audio engine must follow real-time-safe design practices.

**Requirements:**

- Bounded processing.
- Preallocated audio buffers where practical.
- No blocking filesystem operations on audio callbacks.
- No GUI or tray operations on audio callbacks.
- No unbounded memory allocation in real-time processing.
- Explicit handling of clock drift and buffer underruns.

**Related:** `ARCHITECTURE.md`

---

## D-017 — Latency Objective

**Status:** Accepted

**Decision:** AudioHole should target an attributable processing latency of 10 ms or less where supported, with 5 ms as a stretch objective.

**Rationale:** Interactive audio, particularly VR, benefits from low latency.

**Important distinction:** This objective applies to AudioHole's contribution, not the entire end-to-end path through wireless hardware, drivers, and external applications.

**Consequences:**

- Latency should be measured rather than assumed.
- Stability must not be sacrificed merely to achieve a smaller nominal buffer.
- Unachievable targets should be reported accurately.

---

## D-018 — Automatic Buffer Recommendation

**Status:** Accepted

**Decision:** AudioHole should recommend a buffer setting that balances latency and stability.

**Rationale:** The minimum supported buffer is not necessarily the best operating choice.

**Consequences:**

- Use probe evidence where available.
- Maintain a stability margin.
- Use conservative fallbacks where evidence is incomplete.
- Permit user overrides.
- Explain the recommendation visibly.

The exact recommendation algorithm remains unresolved.

---

## D-019 — Explicit Routing States

**Status:** Accepted

**Decision:** AudioHole uses the canonical routing state model defined in `PROFILES.md`.

States include:

- Stopped.
- Ready.
- Starting.
- Running.
- Degraded.
- Recovering.
- Failed.
- Stopping.

**Rationale:** Requested routing and actual routing must remain distinguishable.

---

## D-020 — Ready Is Not Failure

**Status:** Accepted

**Decision:** Ready represents a valid armed profile waiting for required resources.

**Rationale:** Devices such as VR headsets may not exist when Windows starts.

**Consequences:**

- Automatic activation when resources become available.
- No unnecessary failure indication.
- Clear tray and GUI status.
- Stop Routing disarms the profile.

---

## D-021 — Automatic Recovery

**Status:** Accepted

**Decision:** AudioHole should automatically attempt recovery from recoverable routing interruptions.

**Requirements:**

- Preserve armed state.
- Detect endpoint loss and return.
- Retry according to defined policies.
- Support configured failover where applicable.
- Cancel recovery after explicit Stop.
- Avoid notification spam.

**Related:** `PROFILES.md`

---

## D-022 — Transactional Profile Switching

**Status:** Accepted

**Decision:** Profile switching must preserve existing working routing when a requested destination cannot be activated, where technically possible.

**Rationale:** A failed profile switch should not unnecessarily replace working audio with silence.

**Consequences:**

- Validate before commit.
- Prepare resources where possible.
- Preserve compatible paths.
- Roll back when necessary.
- Explain failures.
- Permit deliberate activation into Ready.

---

## D-023 — Staged Routing Edits

**Status:** Accepted

**Decision:** Routing-affecting changes to an active profile are staged until Apply.

**Rationale:** Editing configuration must not unexpectedly interrupt audio.

**Consequences:**

- Apply and Revert controls.
- Minimal-disruption updates where possible.
- Immediate editing of non-routing metadata where safe.
- Detection of conflicting external file edits.

---

## D-024 — Persistent System Tray

**Status:** Accepted

**Decision:** AudioHole remains accessible through the Windows notification area while its main GUI is closed.

**Requirements:**

- Status icon.
- Active profile indication.
- Start/Stop controls.
- Profile selection.
- Hole status submenu.
- Open GUI.
- Explicit Exit.

**Related:** `TRAY.md`

---

## D-025 — Stop Versus Exit

**Status:** Accepted

**Decision:** Stop Routing and Exit AudioHole are separate operations.

Stop Routing disarms routing but leaves the application available.

Exit AudioHole stops routing and terminates the application.

**Rationale:** Users must be able to stop audio without losing background access to AudioHole.

---

## D-026 — Configurable Startup

**Status:** Accepted

**Decision:** AudioHole supports optional Windows startup and configurable routing activation.

**Modes:**

- Manual.
- Immediate.
- Delayed.

**Delayed startup requirements:**

- Configurable duration.
- Countdown begins when AudioHole launches.
- Countdown visible in GUI and tray.
- Start Routing Now bypasses the timer.
- Cancel Startup disarms the timer.
- Missing devices lead to Ready after timer expiration.
- Delay applies once per application launch.
- Recovery does not repeat the startup delay.

**Related:** `GUI.md`, `TRAY.md`

---

## D-027 — Notification Preferences

**Status:** Accepted

**Decision:** AudioHole supports three notification levels:

- All Events.
- Warnings and Errors — default.
- Errors Only.

**Requirements:**

- Meaningful partial and complete failures generate eligible notifications.
- Ready-to-Running informational notifications are silent.
- Duplicate recovery notifications are suppressed.
- Windows notification preferences are respected.
- The tray remains authoritative for persistent visible status.

---

## D-028 — Editable Notes

**Status:** Accepted

**Decision:** Device records and profiles have editable Notes fields exposed through the GUI.

Hole notes are also supported in the current GUI design.

**Rationale:** Notes are useful for identifying devices, documenting routing purposes, and maintaining portable configuration.

**Consequences:**

- Notes remain human-readable.
- Rescanning must not overwrite device notes.
- Renaming must not discard notes.
- Notes must not affect routing behavior.

---

## D-029 — One Authoritative Controller

**Status:** Accepted

**Decision:** GUI, tray, and optional external-control interfaces share one application controller and routing supervisor.

**Rationale:** Multiple interfaces must not maintain contradictory routing states.

**Consequences:**

- Centralized command handling.
- Centralized status reporting.
- Event-driven updates.
- No independent GUI or tray routing engines.
- Consistent Start, Stop, Activate, and Exit semantics.

---

# Part II — Provisional Technical Decisions

## D-030 — Audio Backend Strategy

**Status:** Open

**Decision required:** Select the primary Windows audio backend architecture.

### Candidate A — Native WASAPI

Implement endpoint discovery, stream management, and audio I/O directly using Windows audio interfaces.

**Advantages:**

- Direct access to Windows-specific capabilities.
- Fine control over shared and exclusive modes.
- Explicit handling of endpoint properties.
- Detailed HRESULT diagnostics.
- Reduced dependence on an abstraction library.

**Disadvantages:**

- Greater implementation complexity.
- More responsibility for stream lifecycle management.
- More platform-specific code.
- Potentially longer initial development.

### Candidate B — miniaudio

Use miniaudio for audio-device access and stream management where suitable.

**Advantages:**

- Established audio abstraction.
- Potentially faster initial development.
- Existing handling for common stream operations.
- Smaller amount of custom low-level code.

**Disadvantages:**

- Abstraction may hide relevant Windows behavior.
- Some probe operations may require native APIs anyway.
- Capability reporting may not expose everything AudioHole needs.
- Advanced diagnostics may require additional integration.

### Candidate C — Hybrid

Use native Windows interfaces for detailed discovery, probing, and identity management, while evaluating miniaudio for routine stream operation.

**Advantages:**

- Native visibility for device investigation.
- Potentially simpler stream implementation.
- Separation between probing and runtime routing.
- Opportunity to replace the streaming backend without redesigning configuration.

**Disadvantages:**

- Two layers of audio integration.
- Potentially inconsistent device identifiers.
- Additional translation and synchronization logic.
- Risk of duplicating functionality.

### Evaluation Criteria

The selected approach must be evaluated against:

1. Reliable endpoint enumeration.
2. Stable device identity mapping.
3. Shared-mode support.
4. Exclusive-mode support.
5. Low-latency operation.
6. Duplex routing.
7. Independent clock handling.
8. Drift compensation.
9. Stream recovery.
10. Detailed diagnostics.
11. Implementation complexity.
12. Dependency maintenance.

### Required Evidence

A prototype should demonstrate:

- Capture from one physical endpoint.
- Playback to another endpoint.
- Simultaneous reverse-direction routing.
- Clean start and stop.
- Device disconnection and recovery.
- Measurable buffering and latency.
- Meaningful Windows error reporting.

**Current position:** No backend is accepted until a representative prototype validates its behavior.

---

## D-031 — GUI Framework

**Status:** Open

**Decision required:** Select the desktop GUI framework.

### Candidate A — Qt

**Advantages:**

- Mature desktop UI toolkit.
- Rich widgets.
- Strong layout capabilities.
- Accessibility support.
- High-DPI support.
- Suitable for complex configuration interfaces.

**Disadvantages:**

- Larger dependency footprint.
- Deployment complexity.
- Licensing considerations.
- Build-system integration requirements.

### Candidate B — wxWidgets

**Advantages:**

- Native-oriented desktop interface.
- Conventional Windows controls.
- Established C++ integration.
- Potentially smaller conceptual gap with Windows behavior.

**Disadvantages:**

- Some advanced layouts may require more manual work.
- Visual consistency and customization require evaluation.
- Integrated console presentation needs validation.

### Candidate C — Dear ImGui

**Advantages:**

- Straightforward integration with C++.
- Flexible custom diagnostic interfaces.
- Convenient technical controls.
- Rapid prototyping.

**Disadvantages:**

- Immediate-mode architecture may complicate conventional desktop interactions.
- Accessibility requires particular attention.
- Native-feeling forms and dialogs may require additional work.
- Tray and Windows integration require separate implementation.

### Evaluation Criteria

- Profile editor usability.
- Searchable device dropdowns.
- Tooltip behavior.
- Integrated verbose console.
- Keyboard accessibility.
- High-DPI scaling.
- Tray integration.
- Windows startup behavior.
- Licensing.
- Distribution size.
- Maintenance burden.

### Required Evidence

Build a small interface prototype containing:

- Four tabs.
- Add Hole workflow.
- Searchable device selection.
- Format and buffer dropdowns.
- Visible recommendation explanations.
- Warning tooltips.
- Scrolling diagnostic console.
- Tray icon and menu.

**Current position:** No framework selected.

---

## D-032 — JSON Library

**Status:** Provisional

**Preferred candidate:** nlohmann/json.

**Rationale:**

- Human-readable JSON support.
- Familiar C++ integration.
- Suitable for structured configuration.
- Convenient serialization and parsing.

**Validation required:**

- Schema validation strategy.
- Deterministic serialization.
- Error reporting.
- Atomic file-write integration.
- Migration handling.
- Dependency and licensing review.

---

## D-033 — Build System

**Status:** Accepted

**Decision:** Use CMake.

**Rationale:** CMake provides a conventional build system for a Windows C++ project and supports the candidate dependencies.

**Open implementation details:**

- Compiler baseline.
- Toolchain configuration.
- Dependency acquisition.
- Packaging.
- Continuous integration.

---

## D-034 — Format Conversion

**Status:** Open

**Decision required:** Select the format conversion and resampling implementation.

### Requirements

The implementation should support the conversions necessary to connect compatible Windows audio endpoints with differing native formats.

It should address:

- Sample format conversion.
- Sample-rate conversion.
- Channel conversion or mapping.
- Stable long-running operation.
- Bounded processing cost.
- Predictable latency.
- Drift compensation.

### Alternatives

- Backend-provided conversion.
- Dedicated resampling library.
- Custom limited conversion pipeline.
- Hybrid conversion strategy.

### Evaluation

Measure:

- Audio quality.
- CPU consumption.
- Latency.
- Stability under clock drift.
- Behavior during buffer underruns.
- Implementation complexity.

**Current position:** No conversion implementation selected.

---

## D-035 — Buffer Recommendation Algorithm

**Status:** Open

**Decision required:** Define how AudioHole selects its recommended buffer configuration.

### Accepted Requirements

- Balance latency and stability.
- Prefer meaningful probe evidence.
- Avoid assuming the minimum buffer is best.
- Provide conservative fallbacks.
- Explain the recommendation.
- Permit manual overrides.

### Candidate Approach

Use an evidence-weighted recommendation:

1. Identify supported or reported buffer constraints.
2. Exclude known invalid configurations.
3. Prefer configurations with successful test evidence.
4. Apply a stability margin.
5. Consider estimated latency.
6. Select a conservative fallback when evidence is insufficient.

### Required Evidence

Test devices with:

- Fixed buffer periods.
- Multiple supported periods.
- Shared-mode constraints.
- Exclusive-mode constraints.
- Unreliable or incomplete capability reporting.

**Current position:** The general policy is accepted; the exact algorithm is open.

---

## D-036 — Device Fingerprint Matching

**Status:** Open

**Decision required:** Define the matching algorithm and confidence thresholds for portable device records.

### Accepted Requirements

- Stable logical IDs.
- Durable attributes preferred.
- Transient Windows identifiers treated as local bindings.
- Ambiguous matches require confirmation.
- Missing devices remain in configuration.

### Candidate Matching Evidence

- USB vendor ID.
- USB product ID.
- Serial number.
- Interface identifier.
- Hardware IDs.
- Endpoint direction.
- Manufacturer.
- Reported device name.
- Driver metadata.

### Important Constraints

Not every device exposes every attribute.

Some devices have no serial number.

Multiple identical devices may be connected simultaneously.

A replacement device may share a model identifier without being the same physical unit.

### Required Evidence

Test matching across:

- USB port changes.
- Driver updates.
- Windows reinstallations.
- Multiple identical devices.
- Devices without serial numbers.
- Device replacement.
- Configuration transfer to another computer.

**Current position:** Identity architecture is accepted; matching weights and confirmation thresholds remain open.

---

## D-037 — External Control Transport

**Status:** Provisional

**Preferred initial approach:** Internal command/status interface, with Windows named pipes as a candidate first external transport.

### Alternatives

- Windows named pipes.
- Localhost HTTP.
- WebSocket.
- CLI wrapper.
- Combination of transports.

### Accepted Requirements

- Optional external control.
- Disabled by default where externally accessible.
- No direct audio-thread command execution.
- Consistent commands with GUI and tray.
- Local security controls.
- Low overhead.

### Candidate Architecture

1. Internal application-controller command interface.
2. Local IPC adapter.
3. Optional CLI client.
4. Optional HTTP or WebSocket adapter.

### Evaluation

- OBS integration.
- Stream Deck integration.
- Security.
- Authentication and permissions.
- Startup behavior.
- Event subscriptions.
- Dependency footprint.

**Current position:** Internal controller abstraction is accepted; external transport selection remains provisional.

---

# Part III — Open Product and Implementation Decisions

## D-038 — Tray Quick Actions

**Status:** Deferred

**Decision:** Individual hole enable/disable actions through the tray and external API are deferred beyond v0.1. Hole and duplex-direction status inspection remains supported; opening a hole in the GUI may be offered.

**Rationale:** Temporary overrides versus persistent edits, recovery, and staged editing require a consistent policy that is unnecessary for initial routing.

**Consequences:** No v0.1 tray hole toggles or required external `hole.enable`/`hole.disable` commands. Profile-level Start/Stop remains available.

---

## D-039 — Single-Click Tray Behavior

**Status:** Open

**Question:** Should a single left-click open a compact status panel?

**Accepted behavior:**

- Double-click opens the GUI.
- Right-click opens the context menu.

**Current position:** Single-click behavior remains open.

---

## D-040 — Routing Diagnostics Presentation

**Status:** Open

**Question:** Should live routing diagnostics appear in an expandable GUI panel, a separate view, or both?

**Accepted requirements:**

- Available while routing.
- Structured diagnostic events.
- Copy and export support.
- No audio-thread blocking.

---

## D-041 — Advanced Scan Coverage

**Status:** Open

**Question:** Which exact format, mode, and buffer combinations should Advanced Scan test by default?

### Considerations

- Scan duration.
- Device disruption.
- Meaningful coverage.
- Driver reliability.
- Test-result confidence.
- User-selected scan scope.

**Current position:** The probing framework is specified, but its default test matrix requires validation.

---

## D-042 — Startup Delay Limits

**Status:** Open

**Question:** What custom activation-delay range should the GUI support?

**Accepted behavior:**

- Presets of 5, 10, 15, 30, and 60 seconds.
- Custom duration.
- Ten-second default when Delayed is selected.
- Delay begins when AudioHole launches.
- Cancellation and manual override supported.

**Current position:** Exact minimum and maximum custom values remain open.

---

## D-043 — Logging Retention

**Status:** Open

**Question:** What default retention limits should apply to scan history and runtime diagnostics?

### Considerations

- Human-readable files.
- Storage growth.
- Diagnostic usefulness.
- User privacy.
- Export behavior.
- Manual deletion.
- Rotation and cleanup.

**Current position:** Logging and export are accepted; retention defaults remain open.

---

## D-044 — Packaging and Distribution

**Status:** Open

**Question:** How should AudioHole v0.1 be distributed?

### Candidates

- Portable ZIP.
- Conventional Windows installer.
- Both.

### Considerations

- No mandatory administrator privileges.
- Portable configuration.
- Optional Windows startup registration.
- Dependency packaging.
- Clean uninstall behavior.
- Updates.
- Code signing.

**Current position:** No packaging strategy selected.

---

## D-045 — Minimum Windows Audio Capabilities

**Status:** Open

**Question:** What minimum audio API capabilities and Windows versions should be assumed within the supported Windows 10/11 range?

### Considerations

- Shared-mode initialization.
- Exclusive-mode support.
- Audio-client interface availability.
- Endpoint notifications.
- Stream timing.
- Loopback capture where applicable.
- Device-driver differences.

**Current position:** Windows 10/11 x64 is accepted; exact API baseline requires technical validation.

---

# Part IV — Deferred Features

## D-046 — Virtual Audio Driver

**Status:** Deferred

**Decision:** Creating a new virtual audio endpoint is not part of AudioHole v0.1.

The project should focus on connecting existing endpoints.

---

## D-047 — Cross-Platform Support

**Status:** Deferred

**Decision:** Linux and macOS support are outside the initial release.

The architecture need not deliberately prevent future portability, but cross-platform abstraction must not complicate v0.1 without clear benefit.

---

## D-048 — Remote Network Routing

**Status:** Deferred

**Decision:** Routing audio between separate computers over a network is outside v0.1.

Localhost control interfaces are not equivalent to network audio transport.

---

## D-049 — Complex Mixing

**Status:** Deferred

**Decision:** AudioHole is not initially intended to be a full digital audio workstation or mixing console.

Advanced multi-input mixing, effect chains, and complex signal processing are outside the core v0.1 objective.

---

## D-050 — Unlimited Routing Scale

**Status:** Deferred

**Decision:** Supporting more than eight simultaneously active holes is outside v0.1.

The initial implementation should focus on correctness and stability within its defined scale.

---

## D-051 — Advanced Tray Dashboard

**Status:** Deferred

**Decision:** A custom floating tray dashboard with detailed meters, graphs, and controls is not required for v0.1.

The standard tray menu and main GUI should provide sufficient functionality.

---

## D-052 — Remote Web Administration

**Status:** Deferred

**Decision:** A browser-based remote administration interface is outside the initial scope.

Optional localhost control may be evaluated separately.

---

# Part V — Decision Validation Strategy

## D-053 — Directional Audio Configuration

**Status:** Accepted

**Decision:** Each directional path owns its effective audio settings. Optional hole-level `audio_defaults` supply inherited values; explicit directional settings override shared defaults. Duplex directions may differ independently.

**Consequences:** Configuration, GUI, migration, and diagnostics must preserve and expose inheritance versus explicit overrides. Concrete sample-format and buffer-field schemas remain subject to their existing decisions.

**Related:** `CONFIG_FORMAT.md`, `PROFILES.md`, `GUI.md`.

---

## D-054 — Profile Selection During Delayed Startup

**Status:** Accepted

**Decision:** Selecting a different profile during a pending startup countdown retains the deadline and retargets activation without immediately starting routing. Manual Start bypasses; Stop and Cancel Startup disarm. Stale callbacks cannot activate the old target.

**Related:** `PROFILES.md`, `GUI.md`, `TRAY.md`, `ARCHITECTURE.md`.

---

## D-055 — Pending Activation Status

**Status:** Accepted

**Decision:** Delayed startup is a transient controller-owned operation, separate from canonical routing state. Routing remains Stopped until activation begins; the controller exposes pending target, deadline, and remaining duration from a monotonic clock. GUI, tray, and optional API share this status; pending timers are not restored from persisted runtime state.

**Related:** `ARCHITECTURE.md`, `CONFIG_FORMAT.md`, `API.md`, `GUI.md`, `TRAY.md`.

---


## 53. Prototype Before Commitment

Technical choices that materially affect latency, stability, or architecture should be validated through small representative prototypes.

A prototype should answer a specific technical question.

It should not automatically become production code.

### 53.1 Audio Backend Prototype

Demonstrate reliable simplex and duplex routing between real Windows endpoints.

### 53.2 GUI Prototype

Demonstrate the actual configuration workflow, technical tooltips, diagnostic console, and tray integration.

### 53.3 Device Identity Prototype

Demonstrate device matching across enumeration changes and multiple hardware configurations.

### 53.4 Buffer and Format Prototype

Measure compatibility, latency, stability, and conversion behavior.

### 53.5 Recovery Prototype

Demonstrate disconnection, reconnection, and recovery without inconsistent state.

---

## 54. Evidence Requirements

A technical decision should not be marked Accepted solely because:

- A library is popular.
- An implementation appears convenient.
- A single device worked once.
- A test stream initialized successfully.
- A feature is theoretically supported.
- An AI coding agent recommends it without supporting evidence.

Appropriate evidence may include:

- Reproducible tests.
- Measured latency.
- Documented API behavior.
- Multiple-device compatibility tests.
- Source inspection.
- Dependency maintenance history.
- Clear tradeoff analysis.

---

## 55. Decision Changes

When a decision changes:

1. Record the reason.
2. Identify the previous decision.
3. Describe the replacement.
4. Update its status.
5. Identify affected documents.
6. Update relevant specifications.
7. Add or revise acceptance tests.
8. Preserve meaningful historical context.

Implementation must not silently diverge from the accepted specification.

---

## 56. Implementation Agent Guidance

Coding agents should treat this document as the decision authority for unresolved technical choices.

### 56.1 Accepted Decisions

Follow accepted decisions unless explicitly instructed to propose a revision.

### 56.2 Provisional Decisions

Evaluate the preferred approach, but do not treat it as permanently selected.

### 56.3 Open Decisions

Do not silently choose an approach that creates substantial architectural commitment.

Instead:

- Identify the decision.
- Explain the available alternatives.
- Recommend a validation approach.
- Produce a focused prototype where authorized.
- Record findings.
- Request a decision before committing to a long-term implementation.

### 56.4 Deferred Decisions

Do not implement deferred features unless explicitly authorized.

### 56.5 Documentation Consistency

When implementation findings invalidate an assumption, identify all affected specifications.

Do not quietly rewrite the project's intended behavior to accommodate a dependency.

---

## 57. Decision Summary

### Accepted Foundations

- Windows 10/11 x64.
- C++ and CMake.
- Normal user privileges.
- Existing Windows audio endpoints.
- Human-readable portable JSON configuration.
- Durable logical device identities.
- Simplex and duplex holes.
- One to four normal active holes; eight maximum.
- GUI-first configuration.
- Independent Profiles, Devices, Scanning, and Settings tabs.
- Guided first-run setup.
- Explainable format and buffer recommendations.
- Nonintrusive Easy Scan.
- Consent-based Advanced Scan.
- Integrated verbose scan console.
- Real-time-safe audio processing.
- Low-latency objectives.
- Explicit routing states.
- Automatic recovery.
- Transactional profile switching.
- Staged routing edits.
- Persistent tray interface.
- Configurable delayed startup.
- Notification preferences.
- Editable device, profile, and hole notes.
- Independent directional audio settings with optional shared defaults.
- Retargetable delayed startup and controller-owned pending status.
- One authoritative application controller.
- Three presentation-only personality modes; DFA hidden in Vanilla.

### Most Important Unresolved Decisions

1. Audio backend.
2. GUI framework.
3. Format conversion and resampling.
4. Buffer recommendation algorithm.
5. Portable device-matching algorithm.
6. External-control transport.
7. Advanced Scan test matrix.
8. Packaging and distribution.

### Deferred Beyond v0.1

- Virtual audio drivers.
- Cross-platform support.
- Network audio transport.
- Full mixing-console functionality.
- More than eight active holes.
- Individual tray/API hole quick toggles.
- Custom advanced tray dashboards.
- Remote web administration.

---

### D-056 — Personality Presentation Modes

**Status:** Accepted

**Decision:** AudioHole uses one `interface.personality_mode` setting with values `default`, `vanilla`, and `uwu`. The displayed options are **Normal — Default Audio Engineer**, **Vanilla — Strictly informational**, and **UwU — I know what you are**. Normal is the default.

**Consequences:** Presentation strings are intentionally authored and selected centrally. Personality does not alter routing, validation, probing, diagnostics, or persistence. Critical information remains clear in every mode. DFA is available in Normal and UwU, hidden in Vanilla, and never has operational side effects. The selector replaces a proposed Vanilla boolean toggle.

**Related:** `PERSONALITY.md`, `CONFIG_FORMAT.md`, `GUI.md`, `ARCHITECTURE.md`, `AGENTS.md`.

---

## 58. Guiding Principle

**AudioHole's technical implementation should serve its established behavior, not redefine that behavior.**

The project should remain:

- Transparent.
- Portable.
- Understandable.
- Recoverable.
- Low-latency.
- Reliable.
- User-controlled.
- Maintainable.

Uncertainty should be documented and investigated rather than hidden behind assumptions.

**A working prototype establishes what is possible. A recorded decision establishes what AudioHole is intended to do.**

---

### D-057 — Manual Destination Test Beep

**Status:** Accepted

**Decision:** v0.1 includes an explicit **Test Beep** action for one selected destination playback endpoint in hole setup/troubleshooting. It emits a short, quiet, cancelable signal to help the user identify the physical output. Duplex directions are selectable independently. The beep is distinct from passive source signal activity monitoring and from the inert DFA button.

**Consequences:** No automatic playback, no silent rerouting to another endpoint, no saved configuration or volume changes, and no disruption to active routing. Unavailable or exclusively occupied targets must produce clear feedback. Default approximately 1 kHz at -30 dBFS for one second with fades; exact implementation may be refined through testing while preserving conservative output and bounded duration. External API exposure is not required.

**Related:** `SPEC.md`, `GUI.md`, `ARCHITECTURE.md`, `API.md`, `ROADMAP.md`, `AGENTS.md`, `PERSONALITY.md`.

## Personality intensity clarification

**Decision:** UwU Mode is deliberately pronounced, not a subtle variant. Common noncritical UI events require authored flirtatious, innuendo-heavy, kaomoji-rich copy with frequent R/L→W substitutions in flavor text, as cataloged in `PERSONALITY.md` §16. Technical facts and serious warnings remain clear; Vanilla remains strictly neutral. This affects presentation only.
