# AudioHole v0.1 — Software Architecture

**Document:** `ARCHITECTURE.md`  
**Status:** Proposed architecture, subject to evaluation  
**Platform:** Windows 10/11, 64-bit  
**Primary language:** C++

---

## 1. Purpose

This document describes the proposed internal architecture of AudioHole v0.1.

AudioHole is a low-latency Windows audio-routing application intended to connect arbitrary capture and playback devices as though they were connected by physical audio cables.

A **hole** is a logical audio connection. It may be:

- **Simplex:** Audio flows in one direction.
- **Duplex:** Audio flows in both directions through two associated directional paths.

A hole is the primary unit of configuration, supervision, and capacity accounting.

The architecture must support:

- Low-latency, reliable audio transfer.
- Independent capture and playback device clocks.
- Multiple simultaneous holes.
- Automatic device discovery and recovery.
- Portable, human-readable configuration.
- Advanced device capability probing.
- Profile management and failover.
- GUI and system tray operation.
- Configurable startup and routing activation.
- Nonintrusive operational notifications.
- Detailed diagnostics without disrupting normal operation.

The implementation should favor predictable behavior, maintainability, and transparency over unnecessary abstraction.

---

## 2. Architectural Principles

### 2.1 Separate Audio Processing from Application Control

The audio engine must operate independently of the graphical interface.

Opening, closing, minimizing, or navigating the GUI must not interrupt active audio routes.

The GUI communicates with the engine through well-defined application interfaces rather than directly manipulating audio streams.

### 2.2 Minimize Work in Real-Time Threads

Audio processing threads must not perform:

- Filesystem operations.
- JSON parsing or serialization.
- Blocking diagnostic logging.
- GUI operations.
- Network operations.
- Device enumeration.
- Unbounded memory allocation.
- Unbounded lock acquisition.

Audio callbacks should perform only the operations necessary to move, convert, synchronize, and deliver audio.

### 2.3 Keep Dependencies Replaceable

Third-party libraries should be isolated behind interfaces where doing so provides meaningful architectural flexibility.

The routing supervisor should not need to know whether a stream is implemented using miniaudio or direct WASAPI.

Backend-specific capabilities and limitations must remain visible to the relevant subsystems rather than being discarded by an overly simplistic abstraction.

### 2.4 Preserve Human-Readable State

Profiles, logical device records, user settings, and diagnostic exports must remain understandable and manually editable.

The application should not rely on opaque databases or hidden state for information necessary to restore a configuration.

### 2.5 Treat Device Availability as Dynamic

An enumerated Windows endpoint is not necessarily usable.

Devices may be disconnected, suspended, occupied, reenumerated, or temporarily unavailable through their transport.

AudioHole must distinguish endpoint existence from demonstrated stream availability.

### 2.6 Prefer Nonintrusive Operation

AudioHole should remain unobtrusive during normal operation.

Routine state changes should not require confirmation dialogs.

Meaningful failures and unusual conditions should be communicated through notifications, status indicators, and diagnostics.

The application should request user intervention only when necessary.

---

## 3. Process Architecture

### 3.1 Single-Process Design

AudioHole v0.1 should initially use a single Windows application process containing:

- Audio engine.
- Routing supervisor.
- Device manager.
- Profile manager.
- Probe manager.
- Diagnostics manager.
- Notification manager.
- GUI.
- System tray controller.

These components operate on separate threads or execution contexts as appropriate.

A separate Windows service is not required for v0.1.

A future service or headless host may be introduced if a demonstrated requirement justifies the additional complexity.

### 3.2 Application Lifetime

The application process is not tied to the visibility of its main window.

Closing the main window should normally hide it while leaving the audio engine and system tray running.

The user must have an explicit **Exit AudioHole** command that shuts down active routes and terminates the application.

The tray should provide quick access to:

- Open AudioHole.
- Start routing.
- Stop routing.
- Select an available profile.
- View connection status.
- Open diagnostics.
- Exit AudioHole.

The application should clearly distinguish hiding the interface, stopping routing, and exiting the process.

### 3.3 Single-Instance Behavior

Normal application launches should not accidentally create multiple independent routing engines.

A second launch should normally activate the existing application window.

The implementation should use an appropriate single-instance coordination mechanism.

---

## 4. Major Components

### 4.1 Application Controller

Coordinates application startup, shutdown, user settings, and communication between major subsystems.

Responsibilities include:

- Initializing configuration and diagnostics.
- Starting the device manager.
- Creating the routing supervisor.
- Starting the GUI and tray controller.
- Processing user commands.
- Coordinating orderly shutdown.
- Handling Windows session and power events.
- Applying startup preferences.

The application controller must not perform real-time audio processing.

### 4.2 Routing Supervisor

The routing supervisor is the authoritative controller of AudioHole's active audio topology.

Responsibilities include:

- Creating and removing holes.
- Managing simplex and duplex configurations.
- Starting and stopping routes.
- Enforcing connection limits.
- Coordinating access to endpoints.
- Managing route state transitions.
- Applying failover policies.
- Responding to device changes.
- Coordinating profile activation.
- Preventing conflicting probing operations.

Only the supervisor may authorize changes to the active routing topology.

### 4.3 Audio Engine

The audio engine performs audio capture, buffering, conversion, clock compensation, and playback.

It exposes a backend-independent interface for normal stream operations.

The audio engine must support multiple simultaneous directional paths and independent endpoint clocks.

### 4.4 Device Manager

The device manager maintains AudioHole's current understanding of available Windows audio endpoints.

Responsibilities include:

- Endpoint enumeration.
- Device arrival and removal detection.
- Endpoint property collection.
- Logical identity matching.
- Device availability reporting.
- Coordination with the persistent device database.

### 4.5 Probe Manager

The probe manager implements the behavior specified in `DEVICE_PROBING.md`.

It supports:

- Easy discovery scans.
- Advanced experimental probing.
- Full and update scans.
- Probe cancellation.
- Progress reporting.
- Detailed test evidence.
- Capability database updates.

Intrusive probing requires explicit user authorization.

### 4.6 Profile Manager

The profile manager loads, validates, edits, and saves user configurations.

It translates human-readable profile definitions into routing plans for the supervisor.

It must not directly create or destroy audio streams.

### 4.7 Diagnostics Manager

Collects application events, route state changes, device failures, and probe results.

It provides structured information to:

- The GUI.
- The live diagnostic console.
- Persistent log files.
- User-requested diagnostic exports.

Diagnostic collection must not compromise real-time audio performance.

### 4.8 Notification Manager

The notification manager translates meaningful application events into user-facing notifications and persistent status indicators.

Responsibilities include:

- Event severity classification.
- Windows toast notifications.
- Notification deduplication.
- Notification preference enforcement.
- Tray and taskbar status updates.
- Contextual notification actions.

It must operate independently of real-time audio processing.

---

## 5. Audio Backend Architecture

### 5.1 Backend Independence

The audio engine should expose an interface capable of representing:

- Capture and playback streams.
- Shared and exclusive operating modes.
- Requested and negotiated formats.
- Buffer configuration.
- Stream timing information.
- Device clock information where available.
- Initialization and runtime errors.
- Stream lifecycle operations.

The interface must preserve backend-specific diagnostic details when necessary.

### 5.2 Candidate Backends

Three approaches require evaluation:

**miniaudio-centric**

Use miniaudio for ordinary streaming and direct WASAPI for advanced probing and diagnostics.

**Hybrid**

Provide both miniaudio and native WASAPI streaming implementations behind a common interface.

**WASAPI-centric**

Use native WASAPI for the primary streaming engine, introducing additional libraries only where beneficial.

Direct WASAPI must be evaluated seriously rather than treated solely as a fallback.

### 5.3 Initial Implementation Strategy

The recommended starting approach is:

1. Define the audio backend interface.
2. Implement direct WASAPI capability inspection and probing.
3. Establish a working streaming backend using the simplest suitable approach.
4. Benchmark miniaudio and native WASAPI under comparable configurations.
5. Select the primary streaming implementation based on evidence.

Avoid maintaining two complete streaming implementations indefinitely unless they provide distinct, useful capabilities.

### 5.4 Backend Selection

If multiple backends are retained, selection should be based on:

- Supported device features.
- Reliability.
- Measured latency.
- Buffer configuration requirements.
- Diagnostic requirements.
- User-configured overrides where appropriate.

Backend changes must not silently alter user-defined routing behavior.

---

## 6. Audio Routing Model

### 6.1 Hole

A hole is a logical audio connection containing one or two directional audio paths.

Each hole has:

- A logical identifier.
- A user-defined name.
- A connection mode: simplex or duplex.
- Source and destination device assignments.
- Requested operating configuration.
- Effective negotiated configuration.
- Buffering and synchronization state.
- Runtime status.
- Diagnostic information.

A hole must not assume that connected devices use identical clocks.

### 6.2 Simplex Hole

A simplex hole carries audio in one direction.

It does not require a dummy reverse stream.

### 6.3 Duplex Hole

A duplex hole contains two associated directional paths.

The two directions may have different:

- Sample rates.
- Buffer sizes.
- Channel layouts.
- Latency characteristics.
- Availability states.

The supervisor coordinates the duplex hole while retaining independent operational state for each direction.

### 6.4 Mirrored Outputs

One capture source may feed multiple playback destinations.

Where practical, the engine should share the source capture stream instead of opening duplicate capture streams.

Each destination requires its own buffering and clock compensation when its playback clock is independent.

For capacity accounting, additional mirrored destinations should be represented as additional logical holes.

Mirrored destinations must not be assumed to remain sample-synchronized merely because they share a source.

### 6.5 Shared Endpoint Streams

The routing engine should distinguish between:

- Logical holes.
- Directional audio paths.
- Physical or virtual endpoint streams.

Multiple logical holes may share a compatible endpoint stream where safe and beneficial.

Stream sharing must not introduce unintended audio mixing or microphone leakage.

---

## 7. Connection Capacity

AudioHole v0.1 supports:

- **1–4 active holes:** Normal operating range.
- **5–8 active holes:** Extended operating range with a nonblocking performance advisory.
- **More than 8 active holes:** Not supported in v0.1.

A hole consumes one capacity slot whether it is simplex or duplex.

Mirrored outputs consume additional logical hole slots even when they share an underlying capture stream.

Capacity is reserved for enabled holes in the armed plan, including holes waiting for devices or recovery. Saved definitions outside that plan and disabled holes do not count. The eight-hole policy is not a guarantee that every hardware configuration can run eight concurrent holes.

The supervisor should separately monitor actual endpoint stream count, since stream count may differ from logical hole count.

The capacity policy should be centralized and configurable internally rather than hardcoded throughout the engine.

The user-facing advisory must not interrupt active audio.

---

## 8. Format Negotiation

AudioHole should minimize unnecessary format conversion.

For each proposed directional path:

1. Resolve source and destination capabilities.
2. Identify compatible operating modes.
3. Compare supported sample rates.
4. Compare sample representations and bit depths.
5. Compare channel counts and layouts.
6. Consider device period and buffering requirements.
7. Select a suitable stream configuration.
8. Record the negotiated result.

The system should prefer directly compatible configurations when doing so provides a meaningful advantage.

However, format matching must not override reliability or create excessive latency.

### 8.1 Shared Trinity

The principal compatibility dimensions are:

- Sample rate.
- Sample representation and bit depth.
- Channel configuration.

These form the Shared Trinity described in `DEVICE_PROBING.md`.

### 8.2 Conversion

Conversion should occur only when required by the route configuration or runtime synchronization.

Potential operations include:

- Sample-rate conversion.
- Integer-to-floating-point conversion.
- Floating-point-to-integer conversion.
- Channel remapping.
- Channel-count conversion.

Conversion decisions should be inspectable through diagnostics.

AudioHole must not introduce unrequested effects, equalization, compression, or other creative DSP.

---

## 9. Clock Synchronization

Different audio devices generally operate from independent hardware clocks.

Even when both devices nominally use the same sample rate, their actual rates may differ.

AudioHole must compensate for clock drift to prevent gradual buffer overrun or underrun.

### 9.1 Clock Domains

Each independently clocked endpoint should be associated with a clock domain.

Shared endpoint streams may belong to the same clock domain.

Clock relationships must be based on actual backend and device behavior rather than assumed from matching nominal formats.

### 9.2 Buffer Monitoring

The engine should monitor buffer occupancy and relevant timing information.

The synchronization controller should distinguish:

- Temporary scheduling jitter.
- Persistent clock drift.
- Device stalls.
- Stream discontinuities.

### 9.3 Drift Compensation

A suitable drift-compensation mechanism should make small, controlled adjustments to prevent long-term buffer instability.

The preferred implementation should minimize audible artifacts and additional latency.

The selected algorithm must be documented and benchmarked.

### 9.4 Clock Master

Where a route requires a reference clock, the system should select an appropriate clock domain.

A user override may be provided for advanced configurations.

Clock-master selection must not imply that other independent hardware clocks become physically synchronized.

---

## 10. Buffering and Latency

### 10.1 Latency Objective

The target is no more than approximately **10 ms of AudioHole-attributable latency**, with a stretch goal of approximately **5 ms** where hardware and operating conditions permit.

These are engineering targets, not guarantees.

Hardware, driver, operating system, wireless transport, and Steam streaming latency are separate contributors.

### 10.2 Buffer Design

The engine should use bounded, preallocated audio buffers wherever practical.

Buffer sizing should consider:

- Device periods.
- Scheduling jitter.
- Independent clock behavior.
- Route reliability.
- Latency requirements.

### 10.3 Underrun and Overrun Handling

The engine must detect and record:

- Capture overruns.
- Playback underruns.
- Buffer starvation.
- Excessive drift.
- Discontinuities.

Recovery behavior should avoid cascading failures across unrelated routes.

### 10.4 Latency Reporting

Diagnostics should distinguish between:

- Requested buffer configuration.
- Negotiated buffer configuration.
- Estimated software buffering latency.
- Observed stream timing.
- Unknown external latency.

Do not present estimated total end-to-end latency as a measured value.

---

## 11. Threading Model

### 11.1 Real-Time Audio Threads

Audio processing should use event-driven operation where supported.

Windows MMCSS should be evaluated for appropriate multimedia scheduling.

Real-time threads must remain bounded and predictable.

### 11.2 Control Thread

The routing supervisor operates outside real-time audio callbacks.

It processes:

- User commands.
- Profile changes.
- Device events.
- Failover decisions.
- Route lifecycle transitions.

### 11.3 Probe Workers

Advanced device probing runs on dedicated worker threads.

Probe workers must support cancellation and bounded timeouts where feasible.

### 11.4 GUI Thread

The GUI operates on its own event loop.

It receives snapshots or notifications rather than holding locks on real-time engine state.

### 11.5 Diagnostic Processing

Real-time events should be recorded using bounded, nonblocking mechanisms.

Formatting, filtering, persistence, and display occur on non-real-time threads.

Diagnostic overload must not stall audio processing.

---

## 12. Device Identity and Portability

AudioHole must not depend on a single machine-specific identifier to recognize devices.

Each logical device record should contain multiple identification hints where available.

Potential hints include:

- Friendly name.
- Endpoint role and direction.
- Hardware identifiers.
- USB vendor and product identifiers.
- Device interface properties.
- Driver information.
- Previously observed endpoint identifiers.
- User-defined alias.

### 12.1 Matching

Device matching should use a documented confidence system.

Possible results include:

- Exact match.
- Strong match.
- Probable match.
- Ambiguous match.
- Missing device.

The system may automatically resolve sufficiently confident matches according to an explicit policy.

Ambiguous matches require user confirmation.

### 12.2 Machine-Specific Data

Current Windows endpoint IDs, temporary availability, and local caches must be separated from portable profile definitions.

Copying a configuration to a different Windows installation must not require manually editing opaque identifiers.

### 12.3 Replacement Devices

Users should be able to associate a replacement physical device with an existing logical device alias.

This association should preserve the intended routing configuration while triggering capability revalidation where necessary.

---

## 13. Device Probing Architecture

The probe manager implements the requirements of `DEVICE_PROBING.md`.

### 13.1 Easy Scan

Uses nonintrusive Windows and driver-reported information.

It must not unnecessarily interrupt active streams.

### 13.2 Advanced Scan

Performs explicitly authorized experimental testing.

It may:

- Query candidate formats.
- Attempt stream initialization.
- Start streams.
- Run bounded operational tests.
- Record exact errors and results.

### 13.3 Probe Access Arbitration

The routing supervisor must coordinate probing and normal routing.

Advanced probing must not silently take exclusive control of an endpoint used by an active route.

When a conflict exists, the user should be informed and offered an appropriate choice.

### 13.4 Evidence

Probe results should preserve distinctions between:

- Reported.
- Accepted.
- Verified.
- Failed.
- Inconclusive.
- Not tested.

A successful initialization must not automatically be classified as verified audio operation.

### 13.5 Storage

Per-device JSON files are authoritative.

Consolidated device summaries are derived and regenerable.

The probe manager should preserve historical results and avoid unnecessary retesting of unchanged devices.

---

## 14. Routing Supervisor State Machine

The supervisor should use explicit states rather than relying on loosely related boolean flags.

Canonical public routing states (D-019):

- `stopped`
- `ready`
- `starting`
- `running`
- `degraded`
- `recovering`
- `failed`
- `stopping`

Resolving is an internal phase of Starting, not a replacement for Ready or an additional public routing state. Individual directional paths and endpoint streams may have their own internal states; public aggregation uses the canonical states.

### 14.1 State Transitions

Transitions should be driven by defined events, including:

- User start or stop.
- Profile activation.
- Device arrival or removal.
- Stream initialization success or failure.
- Runtime audio failure.
- Failover candidate availability.
- Recovery timeout.

### 14.2 Partial Duplex Failure

If one direction of a duplex hole fails, the supervisor should not automatically terminate the healthy direction unless the profile requires coupled behavior.

The GUI must clearly identify partial duplex operation.

### 14.3 Error Isolation

Failure of one hole should not terminate unrelated holes.

The supervisor should attempt recovery only where appropriate and avoid unbounded restart loops.

---

## 15. Failover and Recovery

AudioHole must support fallback endpoints and configurable failover behavior.

### 15.1 Failover Groups

A failover group defines multiple eligible endpoints for a logical routing role.

Candidates may have priorities and eligibility conditions.

### 15.2 Policies

Supported policies should include:

- Prefer highest-priority available endpoint.
- Retain the current endpoint until it becomes unusable.
- Require manual selection.

### 15.3 Availability

Availability checks should consider more than Windows endpoint enumeration.

Potential evidence includes:

- Endpoint presence.
- Device state.
- Stream initialization success.
- Runtime stream health.
- Transport or session indicators where accessible.

The system must report uncertainty rather than claim reliable session detection without sufficient evidence.

### 15.4 Recovery

Recovery should use bounded retries and backoff.

Repeated failures should not create excessive CPU usage, log flooding, or rapid audible interruptions.

Automatic recovery must respect explicit user stop commands.

---

## 16. Profile Activation

Profile activation should be treated as a controlled topology transition.

The profile manager produces a validated routing plan.

The supervisor then:

1. Resolves logical devices.
2. Checks capacity limits.
3. Checks endpoint access conflicts.
4. Evaluates supported formats.
5. Prepares required resources.
6. Applies the new topology.
7. Reports success, partial success, or failure.

Where practical, avoid interrupting unaffected holes.

The application must not silently discard the previously active profile if activation fails.

Profile definitions and runtime state should remain distinct.

---

## 17. Configuration and Persistence

AudioHole uses human-readable, manually editable configuration files.

The exact schema is defined in `CONFIG_FORMAT.md`.

Suggested categories include:

- Application settings.
- User profiles.
- Logical device records.
- Device capability records.
- Generated device summaries.
- Diagnostic logs.
- Machine-local caches.

### 17.1 File Ownership

Each persistent data category should have a clearly defined authoritative source.

Generated summaries and caches must be identifiable as such.

### 17.2 Schema Versioning

Persistent files should contain schema version information.

Migrations must preserve user data and avoid silent destructive changes.

### 17.3 Safe Writes

Configuration updates should use validated, atomic replacement where practical.

Malformed files should produce actionable errors rather than silently resetting user settings.

### 17.4 Manual Editing

The GUI and external text editors must operate on the same underlying file formats.

The application should detect externally modified configuration where practical and avoid silently overwriting those changes.

---

## 18. Windows Startup and Automatic Routing

AudioHole must support optional automatic launch when the user signs into Windows.

Startup behavior is a user preference, not a prerequisite for installation.

### 18.1 Start with Windows

Provide a **Start AudioHole with Windows** setting.

When enabled, AudioHole should launch at user sign-in.

Automatic startup should be disabled by default.

For v0.1, prefer a per-user startup mechanism that does not require administrator privileges.

Windows Task Scheduler or the per-user Run registry key may be evaluated.

Avoid installing a Windows service solely to support automatic startup.

### 18.2 Startup Profile

The default startup profile is the **last used profile**.

The user may override this by selecting a specific profile to load at startup.

These options should be mutually exclusive:

- Restore last used profile.
- Load a manually selected startup profile.

The selected startup profile should be loaded regardless of whether routing activates automatically.

### 18.3 Routing Activation

The user may select one of three modes:

1. **Manual:** Load the profile but do not start routing until requested.
2. **Immediate:** Start routing automatically when the application initializes.
3. **Delayed:** Start routing automatically after a user-configurable delay.

The selected activation mode applies whether AudioHole was launched manually or automatically with Windows.

A delayed start must be cancellable.

A manual Stop command must prevent a pending automatic activation from unexpectedly restarting routing.

### 18.4 Startup Behavior

When launched automatically:

- Start without unnecessarily displaying the main window.
- Initialize the system tray.
- Load application settings.
- Discover available devices.
- Resolve the requested startup profile.
- Start routing according to the configured activation mode.
- Report failures through notifications, status indicators, and diagnostics.

Startup must not assume all devices are immediately available.

### 18.5 Missing Devices

Missing devices must not prevent AudioHole from starting.

When a required device is unavailable:

- Load the requested profile.
- Identify affected holes.
- Display a nonintrusive notification.
- Update persistent status indicators.
- Apply configured failover and recovery policies.
- Automatically recover when appropriate and permitted by the user's configuration.

Do not repeatedly notify the user about an unchanged missing-device condition.

### 18.6 User Control

The user must be able to disable automatic startup from application settings.

Installation must not enable automatic startup without the user's consent.

---

### 18.7 Pending Activation Status

Delayed startup is a transient controller-owned pending operation, not a ninth canonical routing state. Routing remains `stopped` until activation begins. The controller exposes canonical `routing_state` plus `startup.activation_mode`, `startup.pending_activation`, `startup.target_profile_id`, `startup.activation_deadline_utc`, and `startup.remaining_seconds`.

Illustrative status snapshot (not persisted configuration):

```json
{
  "routing_state": "stopped",
  "startup": {
    "activation_mode": "delayed",
    "pending_activation": true,
    "target_profile_id": "vr-audio",
    "activation_deadline_utc": "2026-10-17T19:30:30Z",
    "remaining_seconds": 15
  }
}
```

The authoritative deadline uses a monotonic clock; the UTC timestamp is informational. GUI, tray, and optional external adapters derive presentation from this single controller status, not independent timers. Changing the selected profile updates the target but not the deadline. Cancellation, expiration, or manual bypass clears pending status and publishes a state update. No pending countdown is reconstructed from persisted runtime state.

### 18.8 Countdown Command Coordination

Profile selection, timer expiration, Start Routing Now, Stop Routing, Cancel Startup, and shutdown must be coordinated by the controller. Each pending activation must use a generation token or equivalent stale-callback rejection. Canceled or superseded timer completions cannot create a second activation or activate an obsolete profile.

---

## 19. GUI and System Tray

### 19.1 GUI Responsibilities

The GUI should provide:

- Profile creation and editing.
- Hole management.
- Device selection.
- Capability inspection.
- Probe controls.
- Failover configuration.
- Startup preferences.
- Connection status.
- Diagnostic console access.

### 19.2 Tray Responsibilities

The system tray provides persistent lightweight control while the main window is hidden.

Tray status should distinguish normal operation, degraded operation, and stopped routing.

### 19.3 Command Handling

GUI actions should submit validated commands to the application controller or routing supervisor.

The GUI should not directly modify live stream internals.

### 19.4 Confirmations

Confirmations should be reserved for operations that are destructive, disruptive, or difficult to reverse.

Routine operations should not require excessive confirmation dialogs.

---

## 20. Notifications and Operational Status

AudioHole should communicate meaningful operational changes through Windows notifications and persistent application status indicators.

**Notifications are informative by default, not modal interruptions.**

### 20.1 Notification Events

Potential notification events include:

- A required device is missing at startup.
- A previously working device disconnects.
- A device reconnects and routing resumes.
- A hole enters a degraded or failed state.
- Automatic failover succeeds or fails.
- A profile cannot be activated.
- An advanced scan completes.
- Repeated buffer instability is detected.
- A configuration requires user intervention.

Ordinary successful operations should not generate unnecessary notifications.

### 20.2 Notification Severity

Events should be classified into three levels.

**Informational**

- Normal recovery.
- Scan completion.
- Successful failover.
- Other noteworthy but nonurgent events.

**Warning**

- Missing device.
- Partial duplex operation.
- Repeated underruns.
- Operating with reduced functionality.

**Error**

- Routing cannot start.
- All configured failover candidates have failed.
- Persistent unrecoverable stream failure.
- Configuration cannot be loaded.

Severity should affect presentation and persistent status, not automatically trigger modal dialogs.

### 20.3 Notification Preferences

Provide three notification levels:

- **All events:** Informational, warning, and error notifications.
- **Warnings and errors:** Suppress routine informational notifications.
- **Errors only:** Display only error notifications.

The default should be **Warnings and errors**.

These preferences control transient notifications, not diagnostic recording or persistent application status.

### 20.4 Tray and Taskbar Status

The system tray icon should reflect the application's current operating condition.

Suggested states:

- **Normal:** All requested holes operating.
- **Idle:** Application running, routing stopped.
- **Starting:** Routing initialization or delayed activation pending.
- **Warning:** One or more holes degraded or unavailable.
- **Error:** Routing failed or requires intervention.

The taskbar icon may reflect the same state while the main window is visible, using an overlay or other appropriate Windows integration.

Status must not rely exclusively on color. Tooltips and accessible text should describe the condition.

### 20.5 Notification Management

The notification subsystem must:

- Deduplicate repeated events.
- Avoid notification storms during device instability.
- Prioritize changes requiring user attention.
- Respect user-configured notification preferences.
- Preserve detailed event history in diagnostics.
- Avoid blocking real-time audio processing.
- Suppress unnecessary notifications during intentional user-initiated shutdown or device transitions.

Windows does not guarantee delivery or display of every toast notification.

Therefore, transient notifications must not be the sole record of an ongoing problem.

### 20.6 Notification Actions

Where supported, notifications may offer contextual actions such as:

- Open AudioHole.
- View affected hole.
- Open diagnostics.
- Select a replacement device.

Actions should be optional conveniences, not required steps for normal recovery.

### 20.7 Guiding Principle

**AudioHole should tell the user when something meaningful changes, make ongoing problems visible at a glance, and otherwise stay out of the way.**

---

### 20.8 Personality Presentation Boundary

The controller, routing engine, device matcher, and diagnostic producers emit structured status, events, and stable message identifiers, not personality-specific prose. A presentation-layer message catalog selects intentionally authored copy for `interface.personality_mode`: `default`, `vanilla`, `uwu`, or an approved user-authored catalog ID. Vanilla is compiled in; optional commentary is loaded and validated as a complete data file outside controller/audio processing. Missing events fall back individually; unavailable selected catalogs fall back entirely to Vanilla. This is an interface preference only and must not change application behavior or controller state.

Technical identifiers, endpoint names, paths, user-authored strings, machine-readable logs, and structured errors are passed through unchanged. Safety-critical, security, and data-integrity messages must remain transparent and actionable in all modes. Do not apply automatic phonetic or UwU rewriting to arbitrary text. Missing variants fall back to clear neutral copy. Under the approved `PERSONALITY.md` §16.6 refinement, optional noncritical Norm/UwU commentary may select from authored pools with reproducible tests and transient no-repeat history. Stable technical messages and Vanilla remain deterministic. Selection occurs on event occurrences, not polling/unchanged refreshes, and has no operational or persistent effects.

Mode changes refresh visible mode-sensitive UI copy and apply to future messages without restarting audio routing. The GUI owns DFA button/slider presentation and visibility; neither control may call controller operations or mutate persisted state. See `PERSONALITY.md`.

## 21. Diagnostics Architecture

Diagnostics should be structured around events and snapshots.

### 21.1 Events

Examples include:

- Device detected.
- Device identity matched.
- Stream initialization attempted.
- Format rejected.
- Hole started.
- Buffer underrun.
- Failover initiated.
- Device recovered.
- Profile activation failed.

### 21.2 Live Console

The GUI should offer a live, CLI-style diagnostic view.

It should support:

- Verbosity levels.
- Filtering.
- Search.
- Timestamps.
- Per-device and per-hole context.
- Probe progress.
- Copying or exporting output.

A separate standalone CLI is not required for v0.1.

### 21.3 Persistent Logs

Logs should be human-readable and organized predictably.

Machine-specific diagnostic data should not be confused with portable configuration.

### 21.4 Error Presentation

User-facing errors should explain:

- What failed.
- Which device or hole was involved.
- What AudioHole attempted.
- Whether automatic recovery is in progress.
- What action the user can take.

Optional humorous `skill_issue` messaging must not replace useful diagnostic information.

---

## 22. Reliability and Fault Containment

AudioHole should continue operating when recoverable problems occur.

Examples include:

- A device disconnecting.
- A wireless transport becoming unavailable.
- A driver refusing a format.
- A profile referencing a missing endpoint.
- A malformed configuration file.
- A probe test failing.
- A single hole experiencing buffer instability.

Failures should be isolated to the smallest practical subsystem.

The application should distinguish between:

- Recoverable runtime conditions.
- User configuration errors.
- Unsupported hardware configurations.
- Internal programming failures.

Unexpected internal failures should generate useful diagnostic records.

---

## 23. Security and Permissions

AudioHole should operate with ordinary user privileges wherever practical.

The application should not require:

- Administrator privileges for normal routing.
- Kernel-mode drivers.
- Mandatory network access.
- Cloud accounts.
- Background telemetry services.

If a feature requires elevated permissions, the reason must be explicit.

Configuration imports must be validated before activation.

Diagnostic exports should avoid unnecessarily exposing sensitive machine or user information.

---

## 24. Testing Architecture

Testing should include several layers.

### 24.1 Unit Tests

Cover:

- JSON parsing and validation.
- Profile models.
- Logical device matching.
- Capacity accounting.
- Failover decisions.
- State transitions.
- Format-selection logic.
- Startup preference resolution.
- Notification classification and deduplication.

### 24.2 Simulated Integration Tests

Use mock endpoints and simulated device events to test:

- Device arrival and removal.
- Stream initialization failures.
- Clock drift.
- Buffer underruns.
- Partial duplex failure.
- Recovery and failover.
- Conflicting probe requests.
- Missing devices during automatic startup.
- Delayed routing activation and cancellation.

### 24.3 Hardware Integration Tests

Test against actual Windows audio endpoints.

Important scenarios include:

- USB capture to USB playback.
- Full duplex between separate devices.
- Mirrored playback.
- Shared and exclusive mode.
- Device hotplug.
- Sleep and resume.
- Wireless endpoint interruption.
- Multiple simultaneous holes.

### 24.4 Performance Tests

Measure:

- AudioHole-attributable latency.
- CPU utilization.
- Memory usage.
- Buffer stability.
- Drift compensation behavior.
- Recovery time.
- Diagnostic overhead.

Benchmark results should record hardware, drivers, operating mode, and test conditions.

---

## 25. Suggested Source Organization

The following structure is illustrative rather than mandatory.

```text
AudioHole/
├── CMakeLists.txt
├── README.md
├── AGENTS.md
├── docs/
│   ├── SPEC.md
│   ├── ARCHITECTURE.md
│   ├── DEVICE_PROBING.md
│   ├── DEPENDENCIES.md
│   ├── CONFIG_FORMAT.md
│   └── DECISIONS.md
├── src/
│   ├── app/
│   ├── audio/
│   │   ├── backends/
│   │   ├── routing/
│   │   ├── buffering/
│   │   └── synchronization/
│   ├── devices/
│   ├── probing/
│   ├── profiles/
│   ├── supervisor/
│   ├── diagnostics/
│   ├── notifications/
│   ├── gui/
│   ├── platform/
│   │   └── windows/
│   └── config/
├── include/
├── tests/
├── third_party/
└── resources/
```

Runtime configuration and device records should live in a separate, clearly documented data directory rather than being mixed into the source tree.

---

## 26. Recommended Implementation Sequence

### Phase 1 — Foundation

- Establish CMake project.
- Implement application lifecycle.
- Define core configuration models.
- Define audio backend interfaces.
- Implement basic diagnostics.

### Phase 2 — Audio Proof

- Establish one capture-to-playback route.
- Compare miniaudio and direct WASAPI where appropriate.
- Implement buffering and clock compensation.
- Measure latency and stability.

### Phase 3 — Device Intelligence

- Implement device enumeration.
- Implement portable identity matching.
- Implement Easy Scan.
- Implement Advanced Scan.
- Implement capability database persistence.

### Phase 4 — Routing Supervisor

- Implement simplex and duplex holes.
- Implement multiple concurrent holes.
- Implement mirrored outputs.
- Implement state machines.
- Implement failover and recovery.
- Enforce capacity policy.

### Phase 5 — User Interface

- Implement GUI and system tray.
- Implement profile editing.
- Implement live diagnostics.
- Implement startup preferences.
- Implement notification management.
- Implement confirmations and user-facing errors.

### Phase 6 — Validation

- Test against representative hardware.
- Benchmark latency.
- Validate device recovery.
- Validate configuration portability.
- Test sleep/resume and Windows startup.
- Resolve critical reliability defects.

These phases remain a scope grouping and may overlap where appropriate. Execution follows the reconciled working sequence in `ROADMAP.md`: foundation, simplex, independent clocks/latency, backend/exclusive comparison, recovery/identity, then duplex. Minimum controller/status/diagnostics begin early; complete Advanced Scan and GUI follow the audio proof. This documentation reconciliation does not authorize prototypes or implementation.

---

## 27. Architectural Decisions Still Open

The following choices require investigation before being treated as final:

1. Primary audio streaming backend.
2. Whether multiple streaming backends are justified.
3. Drift-compensation algorithm and resampling implementation.
4. GUI framework.
5. Logging implementation.
6. Testing framework.
7. Exact configuration directory layout.
8. Device identity confidence thresholds.
9. Default buffering and latency policies.
10. Specific Windows startup mechanism.
11. Partial-duplex failure and recovery policies.
12. Notification integration and Windows compatibility details.

These decisions should be recorded in `DECISIONS.md` as they are resolved.

---

## 28. Definition of Architectural Success

The architecture is successful when AudioHole can:

- Establish stable low-latency simplex and duplex holes.
- Operate independently of GUI visibility.
- Recover gracefully from device changes.
- Preserve portable configurations across Windows installations.
- Inspect and experimentally probe device capabilities.
- Provide accurate and useful diagnostics.
- Support multiple concurrent holes without unnecessary interference.
- Start with Windows when configured.
- Load the last used or selected startup profile.
- Activate routing manually, immediately, or after a configured delay.
- Communicate meaningful problems without unnecessary interruption.
- Remain understandable and maintainable without requiring knowledge of hidden application state.

**AudioHole should behave like a physical audio cable during normal operation, while providing substantially better visibility and control when something goes wrong.**
---

## Temporary Source Signal Activity Observation (v0.1)

Expose an optional, read-only observation of whether non-silent audio samples are passing through each active directional source path. Use samples already available in the audio engine; do not open another capture stream. Computation must be real-time safe, bounded, allocation-free on audio callbacks, and must not alter sample data, routing, buffering, gain, or latency.

The engine may compute a cheap per-buffer activity estimate and publish aggregated, throttled observations to non-real-time consumers. UI refresh of roughly 10–15 Hz is sufficient; the exact activity threshold and smoothing may be chosen during implementation and must be documented and tested. Disable publication when no diagnostic subscriber is active.

The observation is source-side only: it does not certify destination playback, audibility, or end-to-end correctness. Distinguish silence, unavailable endpoints, and stopped paths in the status model. Do not persist or log audio samples. Metering must never become a required dependency for routing.


---

## Explicit Destination Test Beep (v0.1)

Provide a controller-owned, user-triggered, bounded test signal targeting exactly one resolved **playback destination endpoint**. It is separate from passive source activity monitoring. A duplex hole's forward and reverse destinations are distinct selectable targets. Do not generate a test signal into a capture-only endpoint or silently substitute another device.

Suggested default: a one-second 1 kHz sine beep at -30 dBFS with short click-free fade-in/out. The implementation must clamp amplitude, bound duration, and stop promptly on cancellation, profile switch, device invalidation, routing stop, or shutdown. A second click must not stack overlapping test signals. A beep must not persist across sessions or alter saved configuration, master volume, gain, buffer sizes, or routing decisions.

If the destination is already part of active routing, the engine may use an existing safe playback path only if it does not disrupt program audio or compromise real-time guarantees; otherwise report the conflict. When stopped, a temporary playback client may be used, released immediately afterward. Exclusive-mode contention must be surfaced rather than worked around by opening a different device. Tone synthesis and command/status coordination must not allocate, block, or take locks on the real-time callback.

Expose transient test state (idle/playing/unavailable/failure and exact target) to the GUI. Success means that the test stream rendered to the selected endpoint, not that a human heard it or that normal routing is verified. Keep device identity, errors, and target information mode-neutral.

---

## Reconciled engineering boundaries and remaining contracts

### Requirements versus candidates

The owner-approved separation in Sections 3–4 remains the baseline. Native event-driven WASAPI and wxWidgets are leading candidates, not selected production backends/frameworks. D-030 and D-031 retain their prototype gates. The initial tested platform baseline is Windows 10 22H2 and supported Windows 11 releases; runtime feature detection, rather than assumed universal low-period support, governs API use. This is a planned validation baseline, not completed testing.

### Sample transfer, synchronization, and fidelity

Destination-paced operation, bounded FIFOs, and adaptive synchronization are accepted. Distinguish:

- **Direct sample transfer:** AudioHole forwards sample values without conversion or clock correction, where technically feasible.
- **Clock-compensated transfer:** independent clocks require correction, potentially altering samples even at equal nominal sample rates.
- **Format conversion:** incompatible rate, representation, or channel requirements require transformation. Conversion and clock compensation may coexist.

Preserve unmodified samples whenever technically feasible without sacrificing reliable continuous operation. O-005 in `DECISIONS.md` deliberately leaves the final policy open. Evaluate eligibility separately from instability detection and the selected response. Matching nominal formats is insufficient proof of common clocks; bounded buffers alone cannot absorb sustained drift indefinitely. Observe occupancy trends, timing evidence, discontinuities, underruns/overruns, and stalls while distinguishing jitter from drift. Candidate responses include adaptive resampling, Windows-managed rate adjustment, and controlled restart/rebuffer. Automatic is now the provisional prototype default with proactive compensation permitted for evaluation; final policy and algorithms remain Open. Strict preservation is internal/experimental; insertion/removal is experimental rather than routine synchronization. See the provisional synchronization section below.

Evaluate SpeexDSP or another established resampler before production selection. Report actual conversion and compensation; never claim bit-perfect operation while samples are altered. Copying samples unchanged inside AudioHole also does not prove an unchanged end-to-end path through Windows, drivers, or transports.

Latency evidence must state measurement boundaries: capture delivery, application queue occupancy, converter delay, and render buffering versus external device/driver/transport contributions. Distinguish periods, endpoint buffer capacities, and application queue depths. The conditional 10 ms objective and 5 ms stretch goal remain unchanged; estimates and external loopback measurements are not automatically isolated AudioHole measurements.

### Resource ownership and asynchronous control

Compatible capture sharing is accepted with independent bounded destination queues and synchronization. Compatibility depends on actual endpoint binding, mode, format, and relevant stream constraints, not friendly names. Shared resources must outlive their consumers; one destination must not backpressure unrelated destinations. General application mixing is outside v0.1. Many-to-one behavior, render-sharing details, and concrete preference fields must not be invented to fill O-004. Exclusive conflicts produce actionable errors. Preserve component boundaries for future mixing/endpoint types without adding a general graph framework.

Control commands are coordinated outside real-time processing. Potentially slow stream/device operations must not block GUI/tray interaction. Apply generation-based cancellation to opens, retries, failover, profile transitions, scans, beep operations, and startup timers. Stop invalidates pending work; stale completions cannot commit resources or restart routing. Cancellation does not imply every synchronous driver call can be interrupted immediately.

COM initialization, interface threading requirements, callback lifetime, and stream release ownership must be explicit in the implementation. Device notifications enqueue work rather than enumerate or open streams inline. Audio processing uses bounded preallocated buffers and telemetry. Formatting, logger calls, serialization, disk writes, and UI work stay outside audio processing; an asynchronous logger alone does not provide real-time safety. Overflow behavior and operation completion must remain observable without stalling audio.

### Provisional intrusive-probe isolation

Normal routing retains the single-process baseline. An Advanced Scan helper process is a provisional narrow exception, not an adopted requirement. Compared with in-process workers, it can contain a stalled user-mode test and permit a watchdog to abandon the helper, at the cost of IPC, lifecycle, consent, and resource coordination. A thread timeout cannot safely terminate arbitrary driver calls; a process watchdog cannot guarantee recovery of a defective driver or OS audio stack. Evaluate both approaches before adoption. No process/thread termination guarantee may be presented as successful driver cleanup.

### Owner decisions and setup tools

O-001 (fallback health), O-002 (switch preparation threshold), O-003 (close/tray preferences), O-004 (persistence), O-005 (direct transfer), and O-006 (storage roots) remain Open. Existing working-routing preservation, explicit Ready activation, close-to-tray defaults, and safe configuration behavior remain binding. Recommendations do not settle these contracts.

Activity observation uses existing source samples and ends with the panel subscription; it never opens another capture stream. Test Beep remains a separate exact-destination operation. A shared render path is usable only when safe and non-disruptive; otherwise report the conflict. Cancellation stops further test production promptly, but already queued endpoint samples can delay acoustic silence. No universal acoustic cancellation bound or new active-route mixing policy is introduced.

### Provisional synchronization strategies within one engine

D-034 and O-005 remain Open. One engine accommodates unchanged transfer, application-managed adaptive resampling, and Windows-managed shared-render rate adjustment where supported. Resolve preferences independently per direction with D-053 inheritance; do not build a second engine for another policy. Jointly optimize continuous reliable delivery, low consistent latency, practical fidelity, and avoidance of unnecessary processing.

Automatic is the proposed prototype default. It may proactively compensate when evidence indicates independent-clock mismatch; waiting for buffer instability is not required. Favor unchanged transfer when technically appropriate. Compare choosing a stable strategy during initialization with dynamic transitions, including filter history/phase, delay changes, discontinuities, and repeated-switch avoidance. Do not select transition algorithms or thresholds before evidence.

Strict preservation is an internal diagnostic/experimental capability, not a public v0.1 requirement. Its candidate contract preserves captured PCM values and sequence within AudioHole: no intentional resampling, insertion, duplication, dropping, or other modification. Reject incompatible formats; report inability to continue and permit explicit restart/rebuffer without silent compensation. Capture/device discontinuities remain observable and cannot be presented as an intact session. Windows/driver/hardware behavior prevents an unqualified end-to-end bit-perfect claim.

Discrete corrections remain experimental; no third public policy is introduced. Automatic generally favors smooth compensation over periodic discontinuities. Emergency buffer dropping or reinitialization is a separately recorded recovery event; exact Automatic recovery permissions remain unresolved.

`AUDCLNT_STREAMFLAGS_RATEADJUST` and `IAudioClockAdjustment::SetSampleRate` provide a Windows-managed comparison candidate for shared rendering, not exclusive streams or capture. Rate adjustment does not prove sample identity or eliminate conversion. `SetSampleRate` must not run on a real-time processing thread. Investigate supported combinations with event-driven `IAudioClient::Initialize`, `IAudioClient3::InitializeSharedAudioStream`, and low-period configurations; do not assume the latter accepts the same flags. References: [stream flags](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants), [SetSampleRate](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclockadjustment-setsamplerate), and [InitializeSharedAudioStream](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient3-initializesharedaudiostream).

Keep selected preference, actual processing, observed health, and endpoint mode independent. Publish bounded telemetry for mismatch/confidence, occupancy/trend, effective ratio, transitions/reasons, lost/corrected frames, discontinuities/recovery, and latency. Formatting and logs remain outside audio processing. These concepts do not establish new payload fields, persistent settings, or canonical routing states. The expanded comparison and evidence gates belong to `ROADMAP.md`.


### Owner-authorized tuning-policy refinement

The GUI edits per-hole policy and retained manual values; a pure control-side
resolver excludes inactive overrides before validation/preparation. Saved records,
edited requests and the copied simulator live plan remain separate. The Routing
Engine selector currently requests Automatic only and cannot load or initialize
an engine. Future actual negotiation, buffering and routing remain engine-owned;
no permanent engine or synchronization implementation is selected by this GUI
refinement. Test-only compatibility is specified in PROVISIONAL_TEST_PERSISTENCE.md.


## Current owner-authorized engine integration

The provided engine 0.6 is now integrated as an independent static module.
ConfigurationController retains persistence/scanner ownership and delegates typed
routing commands to RoutingService. That service owns Runtime on a dedicated
non-audio control thread, carries immutable profile/binding copies and rejects
stale publications by generation. Native lifecycle/audio workers remain inside
the module. Widgets own no audio resources. Exit/restart requests Stop, continues
the GUI event loop until confirmed cleanup, then destroys service ownership.

See ENGINE_INTEGRATION.md for admission/progress, access/fallback, runtime-intent
and diagnostic contracts. There is no production supervisor/recovery/transactional
live-switch implementation added by this increment. Tutorial never creates the
real service or touches stored device/profile data.

## Embedded Plug Board

The owner-authorized Plug Board 0.2.6 module has an application-lifetime control
service and an embedded wx view. It consumes scanner bindings and committed
profile activations; it never changes the engine or profile editor. Native app
policy, discovery and storage remain on module-owned workers. See
[PLUG_BOARD_INTEGRATION.md](PLUG_BOARD_INTEGRATION.md).
