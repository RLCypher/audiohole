# AudioHole v0.1 — Functional Specification

**Document:** `SPEC.md`  
**Status:** Working specification  
**Platform:** Windows 10/11, 64-bit  
**Primary language:** C++

---

## 1. Core Concept

**Essentially, the goal of AudioHole is to connect arbitrary audio devices as though they were connected by a physical duplex audio cable.**

AudioHole is a general-purpose Windows audio-routing application that transfers audio between capture and playback endpoints with minimal unnecessary processing, low latency, and predictable behavior.

A **hole** is a logical connection between audio devices. A hole may operate in either of two configurations:

- **Simplex:** Audio travels in one direction.
- **Duplex:** Audio travels in both directions through two associated directional paths.

A duplex hole should behave as closely as practical to a physical duplex audio cable connecting two devices.

A simplex hole should behave as a one-way audio cable.

In normal operation, AudioHole should be effectively invisible to the user. Once configured, it should continue routing audio without requiring routine interaction.

The application must remain useful when something goes wrong by providing detailed diagnostics, understandable status information, and straightforward recovery controls.

---

## 2. Project Goals

AudioHole v0.1 should provide:

- Reliable, low-latency audio routing between arbitrary compatible Windows audio endpoints.
- Simplex and duplex logical connections.
- Multiple simultaneous independent holes.
- Optional mirrored output routing.
- Automatic device discovery and recovery.
- Portable, human-readable configuration.
- Detailed device capability inspection.
- Experimental probing of unadvertised device capabilities.
- Automatic and manual failover.
- Profile-based configuration.
- A conventional Windows GUI and persistent system tray controls.
- Configurable Windows startup and routing activation.
- Nonintrusive notifications and persistent operational status.
- Detailed diagnostics without unnecessary interruption.

AudioHole should remain lightweight and should not require proprietary audio hardware, cloud services, or specialized kernel drivers for ordinary operation.

---

## 3. Target Platform

The initial target is:

- Windows 10, 64-bit.
- Windows 11, 64-bit.
- Conventional Windows audio capture and playback endpoints.
- USB audio devices.
- Integrated audio devices.
- Virtual audio endpoints.
- Wireless and network-backed audio endpoints exposed through Windows.

The application should operate with ordinary user permissions wherever practical.

Cross-platform support is not required for v0.1.

The architecture should not deliberately prevent future platform support, but Windows-specific optimization takes priority.

---

## 4. Reference Use Cases

These scenarios are representative acceptance targets rather than exclusive supported configurations.

### 4.1 RØDECaster Pro II to Steam Frame

The user operates a RØDECaster Pro II as the primary hardware mixer.

Game audio, Discord, alerts, and other sources are mixed or routed through the RØDECaster.

A RØDECaster USB capture endpoint provides audio to AudioHole.

AudioHole forwards that audio to the Steam Streaming Audio playback endpoint, allowing it to reach the Steam Frame headphones.

The hardware mixer remains in the audio path.

Low latency is particularly important for rhythm games such as Beat Saber.

### 4.2 Steam Frame Microphone to RØDECaster

The Steam Frame microphone appears through a Steam Streaming Audio capture endpoint.

AudioHole forwards that microphone audio to an appropriate RØDECaster USB playback endpoint.

The microphone can then participate in the hardware mixer's routing and monitoring configuration.

### 4.3 Full Duplex Connection

A single duplex hole connects the RØDECaster and Steam Streaming Audio endpoints in both directions.

Each direction may require independent format negotiation, buffering, and clock compensation.

The user should be able to configure, activate, monitor, and stop the duplex hole as one logical connection.

### 4.4 Automatic Headset Failover

The user has multiple potential listening devices, such as:

- Steam Frame.
- Wireless headset.
- Another available playback endpoint.

AudioHole should support failover between eligible devices while keeping the RØDECaster as the consistent source.

A Windows endpoint remaining enumerated does not necessarily mean its associated headset or streaming session is active.

AudioHole should use available transport and stream-health information where practical and report uncertainty when reliable availability detection is not possible.

### 4.5 Basic Capture-to-Playback Connection

A USB capture device is routed to a separate DAC or playback endpoint.

The user can configure the connection without needing to understand the application's internal audio backend.

### 4.6 Multiple Independent Holes

The user operates several unrelated audio connections simultaneously.

Each hole has independent configuration, status, and diagnostics.

A failure in one hole should not unnecessarily interrupt the others.

### 4.7 Mirrored Outputs

One source is routed to multiple playback destinations.

AudioHole should reuse compatible capture streams where practical while maintaining independent destination buffering and synchronization.

Additional mirrored destinations count as additional logical holes for capacity purposes.

### 4.8 Portable Configuration

The user copies AudioHole's portable configuration to another computer or a clean Windows installation.

After connecting the relevant devices, AudioHole should recognize them through durable identification hints and restore the intended routing with little or no manual intervention.

Ambiguous matches must not silently select the wrong device.

---

## 5. Hole Model

A hole is the primary logical unit of audio routing.

Each hole should support:

- A stable logical identifier.
- A user-defined name.
- Simplex or duplex operation.
- Logical device assignments.
- Requested audio configuration.
- Effective negotiated configuration.
- Start and stop controls.
- Runtime status.
- Diagnostics.
- Optional failover configuration.

### 5.1 Simplex Holes

A simplex hole carries audio in one direction.

It must not require an unused reverse stream.

### 5.2 Duplex Holes

A duplex hole contains two associated directional paths.

Both directions are managed as one logical connection, but their audio processing may be independent.

A duplex hole may continue operating in a degraded state when one direction fails, subject to the configured recovery policy.

### 5.3 Independent Holes

Multiple holes may operate simultaneously.

The application should isolate failures and resource management wherever practical.

### 5.4 Mirrored Connections

Mirroring a source to additional destinations should be supported.

Each additional destination consumes another logical hole slot.

Shared capture resources may be reused where compatible.

---

### 5.1 Directional Audio Settings and Startup Selection

Each simplex direction and each direction of a duplex hole has its own effective audio configuration. Optional shared hole defaults may be inherited, but explicit directional overrides prevail (D-053). During delayed startup, selecting another profile retargets the existing controller-owned countdown without restarting it; individual tray/API hole quick toggles are deferred beyond v0.1 (D-038, D-054, D-055).


## 6. Connection Capacity

AudioHole v0.1 supports:

- **1–4 active holes:** Normal operation.
- **5–8 active holes:** Extended operation with a nonblocking performance advisory.
- **More than 8 active holes:** Not supported in v0.1.

A hole consumes one capacity slot regardless of whether it is simplex or duplex.

Inactive holes and saved profile definitions do not count toward the limit.

The application should not impose a corresponding limit on the number of saved profiles or inactive hole definitions.

The extended-range advisory should be shown once per relevant activation or transition rather than repeatedly interrupting the user.

Actual endpoint stream count and resource consumption should be monitored separately.

The architecture should not hardcode the v0.1 limit throughout the audio engine.

---

## 7. User Interface

AudioHole v0.1 is a GUI-first application.

The GUI should provide:

- Profile management.
- Hole creation and editing.
- Device selection.
- Audio configuration.
- Device capability inspection.
- Easy and Advanced scanning.
- Failover configuration.
- Start and stop controls.
- Connection status.
- Startup preferences.
- Notification preferences.
- Diagnostic console access.

The interface should expose advanced controls without requiring users to understand low-level Windows audio APIs for ordinary operation.

A full standalone command-line interface is not required for v0.1.

---

## 8. System Tray and Application Lifetime

AudioHole should remain running when its main window is hidden or closed.

The system tray should provide:

- Open AudioHole.
- Start routing.
- Stop routing.
- Select profile.
- View operational status.
- Open diagnostics.
- Exit AudioHole.

Closing the main window should not terminate active routing by default.

An explicit Exit command should shut down the application and its active audio streams.

A second normal application launch should activate the existing instance rather than start a conflicting second routing engine.

---

## 9. Windows Startup

AudioHole must support optional automatic launch when the user signs into Windows.

### 9.1 Start with Windows

Provide a user-configurable **Start AudioHole with Windows** option.

It should be disabled by default.

Enabling it should not require administrator privileges under ordinary circumstances.

### 9.2 Startup Profile

The default startup profile behavior is:

**Restore the last used profile.**

The user may override this by selecting a specific profile to load at startup.

These options are mutually exclusive.

### 9.3 Routing Activation

The user must be able to choose:

- **Manual:** Load the startup profile without activating routing.
- **Immediate:** Automatically activate routing when AudioHole starts.
- **Delayed:** Automatically activate routing after a configurable delay.

The routing activation setting applies to application startup whether initiated manually or through Windows sign-in.

A pending delayed activation must be cancellable.

An explicit Stop command must cancel pending automatic activation and must not trigger an unexpected restart.

### 9.4 Missing Devices at Startup

AudioHole must not fail to launch merely because a configured device is missing.

It should:

- Load the requested profile.
- Identify unavailable devices.
- Mark affected holes appropriately.
- Display a nonintrusive notification.
- Update its persistent status indicator.
- Follow the configured failover or recovery policy.

A device becoming available later may trigger automatic recovery if routing was requested and the configured policy permits it.

Loading a profile in Manual mode must not itself authorize automatic routing.

---

## 10. Audio Engine

AudioHole should support low-latency capture and playback through Windows audio facilities.

Requirements include:

- Shared-mode operation.
- Exclusive-mode operation where supported.
- Independent capture and playback streams.
- Multiple simultaneous holes.
- Format negotiation.
- Buffer management.
- Clock-drift compensation.
- Stream-health monitoring.
- Controlled startup and shutdown.
- Recovery from recoverable failures.

The selected implementation may use miniaudio, direct WASAPI, or a justified combination.

Direct WASAPI should be seriously evaluated for its diagnostic transparency, detailed format negotiation, and low-level stream control.

The application must not assume that direct WASAPI automatically provides lower latency than an appropriately configured library using the same underlying Windows audio facilities.

---

## 11. Format Negotiation and Conversion

AudioHole should avoid unnecessary audio conversion.

The preferred configuration should consider:

- Sample rate.
- Sample representation and bit depth.
- Channel count and layout.
- Shared or exclusive mode.
- Device period and buffering requirements.
- Stability and latency.

The principal format compatibility dimensions are the **Shared Trinity**:

1. Sample rate.
2. Sample representation and bit depth.
3. Channel configuration.

When a directly compatible configuration is available, AudioHole should prefer it where practical.

However, matching nominal formats does not guarantee that devices share a physical clock.

Conversion and clock compensation may still be required.

AudioHole must not introduce unrequested effects, equalization, compression, or other creative audio processing.

---

## 12. Clock Management

AudioHole must support routing between devices with independent clocks.

The application should:

- Detect and compensate for sustained clock drift.
- Monitor buffer occupancy.
- Avoid long-term buffer accumulation or starvation.
- Minimize audible artifacts.
- Distinguish clock drift from temporary scheduling jitter.
- Record synchronization problems in diagnostics.

Where a clock-master selection is necessary, the application should provide a reasonable automatic choice and may expose an advanced override.

The selected synchronization method must be validated through testing.

---

## 13. Latency Requirements

The engineering target is approximately **10 ms or less of AudioHole-attributable latency**.

The stretch goal is approximately **5 ms**, where supported by the hardware and operating environment.

These targets are not guarantees.

Total end-to-end latency may also include:

- Hardware buffering.
- Driver latency.
- Windows audio processing.
- Wireless transport.
- Steam streaming.
- Other external processing.

AudioHole should report measured and estimated latency separately.

It must not present unmeasured end-to-end latency as a verified result.

Latency optimization must not come at the expense of persistent audio instability.

---

## 14. Device Discovery

AudioHole should discover compatible Windows capture and playback endpoints.

Device discovery should collect available identification and capability information without unnecessarily disrupting normal audio operation.

The application should distinguish:

- Currently connected devices.
- Previously known but unavailable devices.
- Newly discovered devices.
- Ambiguous identity matches.

Device records should persist when a device is temporarily absent.

---

## 15. Device Capability Probing

The detailed probing specification is defined in `DEVICE_PROBING.md`.

AudioHole must provide two principal scanning modes.

### 15.1 Easy Scan

The default scan mode.

It should:

- Enumerate available devices.
- Collect Windows-reported capabilities.
- Avoid intrusive stream testing.
- Avoid unnecessary interruption of active audio.

### 15.2 Advanced Scan

An explicitly authorized experimental scan.

It should:

- Investigate likely unadvertised format configurations.
- Test shared and exclusive operation where appropriate.
- Attempt stream initialization.
- Perform bounded operational verification.
- Record exact failures and results.
- Support cancellation.
- Provide detailed live progress.

Before starting, AudioHole must advise the user to connect and power on all devices intended for testing and ensure relevant wireless transports or sessions are active.

The application must warn that advanced testing may interrupt audio, take exclusive control of devices, or produce audible clicks, pops, or other unpleasant sounds.

The user must explicitly consent before intrusive testing begins.

### 15.3 Full and Update Scans

Provide:

- **Full Scan:** Examine the selected device population comprehensively.
- **Update Scan:** Recognize existing records and focus on new or changed devices.

A force-reprobe option should be available.

### 15.4 Capability Evidence

Results must distinguish between:

- Reported capabilities.
- Accepted configurations.
- Operationally verified configurations.
- Failed tests.
- Inconclusive tests.
- Untested configurations.

Successful initialization alone must not be described as proof of successful audio transfer.

Shared-mode format acceptance must not automatically be interpreted as native hardware support.

---

## 16. Device Identity and Configuration Portability

**AudioHole configurations must remain portable across computers, Windows reinstalls, and reasonable hardware changes.**

A device must not be identified solely by a machine-specific Windows endpoint identifier.

Logical device records should use multiple durable identification hints where available.

These may include:

- User-defined aliases.
- Device names.
- Vendor and product identifiers.
- Hardware and interface information.
- Driver metadata.
- Previously observed endpoint identifiers.

The application should automatically resolve confident matches.

Ambiguous matches require user confirmation.

The user must be able to reassign a logical device alias to replacement hardware without rebuilding an entire profile.

Machine-specific identifiers and caches must be separated from portable configuration.

A user should be able to copy the relevant AudioHole configuration directory to another Windows installation, reconnect their devices, and restore the intended routing with minimal intervention.

---

## 17. Profiles

Profiles are named collections of hole definitions and associated operating preferences.

The user should be able to:

- Create profiles.
- Rename profiles.
- Duplicate profiles.
- Edit profiles.
- Save profiles.
- Activate profiles.
- Switch between profiles.
- Delete profiles with appropriate confirmation.

A new profile should begin with one hole definition by default.

Profiles may contain more saved holes than the v0.1 active capacity limit, provided activation respects the limit.

Profile files must remain human-readable and manually editable.

The GUI and external text editors should operate on the same documented configuration format.

---

## 18. Failover

AudioHole must support configurable fallback devices.

A logical routing role may contain multiple eligible physical or virtual endpoints.

Supported policies should include:

- Prefer the highest-priority available endpoint.
- Retain the current endpoint until it becomes unavailable.
- Require manual endpoint selection.

Failover should be based on the best available evidence of actual device usability.

Endpoint enumeration alone must not be treated as conclusive proof of an active wireless or streaming session.

If availability cannot be determined reliably, AudioHole should communicate that uncertainty.

Failover must not silently redirect microphone audio to an unintended destination.

---

## 19. Recovery and Partial Failure

AudioHole should recover automatically from appropriate transient failures when routing remains requested.

Examples include:

- Device disconnection and reconnection.
- Temporary wireless endpoint unavailability.
- Stream initialization failure.
- Device reenumeration.
- Sleep and resume.
- Recoverable audio-driver errors.

Recovery attempts should use bounded retries and backoff.

### 19.1 Partial Duplex Failure

If one direction of a duplex hole fails, the healthy direction should normally remain operational.

The hole should enter a degraded state.

A profile may specify coupled behavior requiring both directions to operate together.

The GUI and diagnostics must identify the affected direction.

### 19.2 Manual Stop

An explicit user Stop command must prevent automatic recovery from restarting intentionally stopped routing.

Recovery and startup activation must respect the user's requested operating state.

### 19.3 Failure Isolation

A failure in one hole should not unnecessarily interrupt unrelated holes.

---

## 20. Notifications and Status Indicators

AudioHole should report meaningful operational changes without unnecessarily interrupting the user.

**Notifications are informative by default, not modal interruptions.**

### 20.1 Notification Events

Examples include:

- Required device missing at startup.
- Previously working device disconnected.
- Device recovered.
- Automatic failover succeeded or failed.
- Hole entered degraded operation.
- Profile activation failed.
- Repeated buffer instability.
- Advanced scan completed.
- Configuration requires attention.

Ordinary successful operations should not generate unnecessary notifications.

### 20.2 Severity Levels

Notifications should use three severity levels:

**Informational**
- Recovery completed.
- Scan completed.
- Successful failover.

**Warning**
- Missing device.
- Partial duplex operation.
- Repeated audio instability.
- Reduced functionality.

**Error**
- Routing cannot start.
- No eligible failover destination remains.
- Persistent unrecoverable failure.
- Configuration cannot be loaded.

### 20.3 Notification Preferences

The user should be able to choose:

- All events.
- Warnings and errors.
- Errors only.

The default is **Warnings and errors**.

These preferences should affect transient notifications without suppressing persistent diagnostics or ongoing status indicators.

### 20.4 Tray and Taskbar Indicators

The system tray icon should indicate:

- Normal operation.
- Idle or stopped routing.
- Starting or pending activation.
- Warning or degraded operation.
- Error requiring attention.

Where practical, the taskbar icon should reflect the same status while the main window is visible.

Status must not depend solely on color.

### 20.5 Notification Behavior

AudioHole must:

- Avoid duplicate notifications.
- Avoid notification storms.
- Preserve event history.
- Suppress unnecessary alerts during intentional shutdown.
- Keep notifications independent of real-time audio processing.
- Avoid requiring user acknowledgment for routine recovery.

Notifications may offer contextual actions such as opening AudioHole, viewing the affected hole, or opening diagnostics.

The persistent status indicator and diagnostic history remain authoritative when a Windows toast is not displayed.

---

## 21. Diagnostics

AudioHole should provide detailed diagnostics without requiring a standalone CLI.

### 21.1 Live Diagnostic Console

Provide a live, CLI-style diagnostic display within the GUI or a detachable window.

It should support:

- Timestamps.
- Verbosity controls.
- Search and filtering.
- Per-device context.
- Per-hole context.
- Stream initialization results.
- Probe progress.
- Exact error codes where available.
- Copying and exporting output.

### 21.2 Persistent Logs

Logs must be human-readable and organized predictably.

They should record significant events without introducing excessive runtime overhead.

### 21.3 Useful Errors

User-facing errors should explain:

- What happened.
- What was affected.
- What AudioHole attempted.
- Whether recovery is underway.
- What the user can do next.

Optional `skill_issue` humor may supplement error messages but must never obscure actionable information.

---

## 22. Configuration and Data Storage

All user-facing configuration, profile, device database, and diagnostic files must remain human-readable and logically organized.

JSON is the preferred structured configuration format.

The exact schemas are defined in `CONFIG_FORMAT.md`.

### 22.1 Device Records

Maintain individual per-device records under:

- `devices/recording/`
- `devices/playback/`

Generate consolidated summaries:

- `devices/recording_devices.json`
- `devices/playback_devices.json`

Individual device files are authoritative.

Consolidated summaries are regenerable.

### 22.2 Portable and Machine-Local Data

Portable configuration should be clearly separated from machine-specific caches and transient state.

Copying portable configuration should not require preserving obsolete Windows endpoint identifiers.

### 22.3 Safe Editing

Configuration writes should be validated and performed safely.

Malformed configuration should produce an actionable error rather than silently destroying or resetting user data.

Schema changes should support controlled migration.

---

## 23. Confirmation and User Interaction

AudioHole should avoid excessive confirmation dialogs.

Routine operations should execute directly where safe.

Confirmation should be reserved for actions such as:

- Deleting profiles.
- Overwriting existing configuration.
- Replacing significant device associations.
- Interrupting active audio for intrusive probing.
- Other potentially destructive or disruptive operations.

Notifications and confirmations serve different purposes:

- **Notifications** report meaningful events.
- **Confirmations** request authorization before consequential actions.

The user should not have to repeatedly acknowledge ordinary audio-device changes.

---

## 24. Performance and Reliability

AudioHole should be designed for sustained background operation.

Requirements include:

- Predictable CPU and memory usage.
- Bounded audio buffering.
- Avoidance of blocking work in real-time callbacks.
- Stable operation with independent device clocks.
- Recovery from transient device failures.
- Graceful handling of unavailable endpoints.
- Minimal interference from GUI and diagnostic activity.
- Appropriate Windows multimedia thread scheduling.

Performance should be validated through actual hardware testing rather than inferred solely from API capabilities.

---

## 25. Security and Privacy

AudioHole should operate locally by default.

Ordinary routing must not require:

- Cloud services.
- User accounts.
- Mandatory network access.
- Administrator privileges.
- Kernel-mode drivers.
- Telemetry collection.

Configuration imports must be validated.

Diagnostic exports should avoid unnecessarily exposing sensitive user or machine information.

---

## 26. Future Extensions

The architecture should permit future capabilities without requiring them in v0.1.

Potential extensions include:

- A specialized Steam Frame edition or preset.
- More advanced transport-aware availability detection.
- Additional routing capacity.
- Advanced audio analysis.
- More sophisticated synchronization policies.
- Headless or service-based operation.
- A standalone CLI.
- Remote management.
- Additional audio backends.

Future features should reuse the same core routing and configuration model wherever practical.

---

## 27. Acceptance Scenarios

AudioHole v0.1 should demonstrate the following:

1. Create a simplex hole between compatible Windows audio endpoints and transfer audio reliably.
2. Create a duplex hole and transfer audio in both directions.
3. Operate four simultaneous holes without a capacity warning.
4. Operate five through eight simultaneous holes with an appropriate nonblocking advisory.
5. Prevent activation beyond eight active holes.
6. Route one source to multiple destinations through separately accounted holes.
7. Preserve unrelated routing when one hole fails.
8. Recover appropriately after a disconnected device returns.
9. Detect and report partial duplex failure.
10. Support automatic failover between eligible endpoints.
11. Perform Easy Scan without unnecessary audio disruption.
12. Perform Advanced Scan after explicit user consent.
13. Preserve detailed probing evidence in human-readable files.
14. Restore a portable profile after a Windows reinstall or computer change, subject to device availability and unambiguous matching.
15. Continue routing when the main GUI is closed.
16. Start with Windows when enabled.
17. Restore the last used profile by default or load a selected startup profile.
18. Support manual, immediate, and delayed routing activation.
19. Cancel pending delayed activation when the user explicitly stops routing.
20. Report a missing startup device through a nonintrusive notification and status indicator.
21. Avoid repeated notifications for an unchanged condition.
22. Provide live diagnostic information without interrupting audio.
23. Remain stable during prolonged operation and common device changes.

---

## 28. Definition of Done

AudioHole v0.1 is functionally complete when:

- The primary routing scenarios work reliably.
- Simplex and duplex holes are supported.
- Capacity limits are enforced consistently.
- Device identity and configuration portability are functional.
- Device probing produces meaningful, persistent evidence.
- Profiles can be created, edited, saved, and restored.
- Failover and recovery operate according to configured policies.
- The GUI and system tray provide complete normal-operation controls.
- Windows startup and routing activation preferences work correctly.
- Unusual events are communicated without unnecessary interruption.
- Diagnostics are sufficient to investigate ordinary device and routing failures.
- The application meets its validated performance and reliability targets to the extent supported by the tested hardware.

**AudioHole should behave like a physical audio cable when everything is working, and like a well-instrumented diagnostic tool when it isn't.**
---

## Temporary Signal Activity Monitoring (v0.1)

AudioHole shall provide a temporary, off-by-default GUI signal-presence indicator during setup and troubleshooting. It shows whether AudioHole is receiving non-silent samples from the source of each active direction, including independent forward and reverse indications for duplex holes.

It is not a calibrated meter, a mixer, a recording tool, or proof that the destination is audible. It must be passive, use the existing audio path, preserve real-time performance, and stop observing when the panel closes. No raw audio may be stored or logged.

The separately specified **Test Beep** is an explicit active output test, not part of passive signal activity monitoring.


---

## Setup Test Beep (v0.1)

AudioHole shall provide a **Test Beep** button in hole setup/troubleshooting for the explicitly selected destination playback endpoint. The goal is to let the user confirm that a brief audible beep comes out of the expected device. In duplex holes the destination path must be selected unambiguously; endpoints that cannot render playback must not be offered as valid beep targets.

The beep is manual, short, conservative in level (default approximately 1 kHz at -30 dBFS for one second, with brief fades), visibly indicated while active, and cancelable. It must not start automatically, change system/device volume, rewrite profile settings, inject into capture/microphone endpoints, or fan out to unspecified outputs. If output cannot be safely opened, report the reason rather than reroute. Playback testing alone does not establish that normal source-to-destination routing works.

The existing real-time routing pipeline must remain unaffected. The implementation may use the existing playback path when safe, or a temporary playback session when routing is stopped; conflicts with exclusive access require explicit handling. All resources must be released after the beep.
