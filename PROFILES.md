# AudioHole v0.1 — Profile Management

**Document:** `PROFILES.md`  
**Status:** Proposed behavioral specification  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `CONFIG_FORMAT.md`, `DEVICE_PROBING.md`, `GUI.md`, `API.md`

---

## 1. Purpose

This document defines how AudioHole profiles are created, edited, loaded, activated, switched, monitored, and recovered.

A profile describes a desired audio-routing configuration. It does not directly represent the current state of Windows audio hardware.

Profiles must remain useful even when devices are disconnected, powered off, replaced, renamed, or temporarily unavailable.

The primary goals are:

- Make complex audio setups easy to save and recall.
- Keep profile editing predictable.
- Preserve existing audio connections whenever possible.
- Allow profiles to wait for unavailable hardware.
- Recover automatically from device interruptions.
- Avoid unintended microphone routing.
- Support configuration portability.
- Keep all profile data human-readable and manually editable.
- Expose consistent status information for the GUI, tray icon, diagnostics, and future external controllers.

**Profiles describe what the user wants connected. The routing supervisor determines how to establish and maintain those connections.**

---

## 2. Core Terminology

### 2.1 Profile

A named collection of logical audio connections and their operating preferences.

A profile may contain enabled and disabled holes.

### 2.2 Hole

One logical audio connection.

A hole may be:

- **Simplex:** One directional audio path.
- **Duplex:** Two associated directional audio paths.

A duplex hole counts as one hole.

### 2.3 Directional Path

A capture-to-render audio path with a source endpoint, destination endpoint, and relevant operating preferences.

### 2.4 Saved Profile

The configuration stored in a profile's JSON file.

### 2.5 Loaded Profile

The profile currently selected for inspection, editing, or activation.

Loading a profile does not necessarily start routing.

### 2.6 Active Routing Session

The runtime implementation of an activated profile.

A routing session can remain active even when none of its requested audio paths are currently operational.

### 2.7 Desired State

The routing configuration requested by the profile.

### 2.8 Actual State

The routing configuration AudioHole has successfully established.

The GUI and diagnostics must distinguish desired state from actual state.

### 2.9 Armed Profile

A profile for which activation has been requested and not subsequently cancelled or stopped.

An armed profile remains eligible for automatic activation and recovery as devices become available.

An armed profile may be Ready, Starting, Running, Degraded, Recovering, or Failed.

---

## 3. Simplex and Duplex Examples

### 3.1 Simplex: Impromptu PA System

A simplex hole connects one audio source to one destination.

For example, a microphone connected to a USB audio interface can be routed directly to speakers.

```text
Impromptu PA System

USB Microphone / Interface Capture
                |
                v
           AudioHole
                |
                v
        Speaker Playback
```

This is one simplex hole.

Only one directional audio path is required.

The reverse direction is not applicable.

Other possible simplex uses include:

- Sending an audio mix to a recording device.
- Routing desktop audio to a virtual playback endpoint.
- Monitoring an external audio source.
- Sending one source to a secondary listening device.

If the same source is routed to multiple independent destinations, each additional destination requires another hole in v0.1.

### 3.2 Duplex: Streaming or VR Workflow

A duplex hole contains two associated directional paths.

For example, a hardware mixer may exchange audio with a streaming application or remote VR headset.

```text
Streaming / VR Audio

Forward Direction:

RØDECaster USB Capture
          |
          v
      AudioHole
          |
          v
Steam Streaming Playback


Reverse Direction:

Steam Streaming Microphone
          |
          v
      AudioHole
          |
          v
RØDECaster USB Playback
```

Together, these paths form one duplex hole.

The two directions may have different:

- Device availability.
- Audio formats.
- Buffer requirements.
- Latency characteristics.
- Runtime health.

They remain associated as one logical connection.

### 3.3 Duplex Does Not Imply Mirroring

The two directions are explicitly configured.

AudioHole must not infer a reverse microphone path merely because a forward path exists.

---

## 4. Profile Identity and Storage

Profiles are stored as individual human-readable JSON files.

Example:

```text
profiles/
├── Default.json
├── PA System.json
├── VR Audio.json
└── Desktop Audio.json
```

### 4.1 Display Name

Each profile has a user-facing `display_name`.

The display name should match its JSON filename, subject to Windows filename restrictions and collision handling.

### 4.2 Stable Profile ID

Each profile has a stable `profile_id`.

The profile ID must not change when:

- The profile is renamed.
- Its JSON file is renamed.
- The configuration moves to another computer.
- Its holes are edited.
- A referenced device is replaced.

Other configuration files must reference profiles by stable ID rather than filename.

### 4.3 External Renaming

Renaming `VR Audio.json` to `Headset Audio.json` should update the profile's display name.

The profile ID remains unchanged.

### 4.4 Notes

Profiles may contain a freeform `notes` field.

Notes are editable through the GUI or directly in JSON.

They are informational only and must not influence routing behavior.

### 4.5 Portable Device References

Profiles reference devices using stable AudioHole logical IDs.

They must not depend on Windows endpoint IDs, USB port positions, or current enumeration order.

---

### 4.6 Hole Notes and Directional Audio Settings

Each hole may contain an editable `notes` field. Notes are informational only and do not affect routing, device matching, recovery, or activation. Profile loading, duplication, migration, and saving preserve hole notes.

Each directional path owns its effective audio settings. A duplex hole may provide optional shared `audio_defaults`, but `forward.audio` and `reverse.audio` independently override inherited values. Both effective configurations must be inspectable through the GUI and diagnostics. See `CONFIG_FORMAT.md` Section 15 and D-053.

---

## 5. Profile Creation

### 5.1 Default Profile

On first launch, AudioHole should create a default profile if none exists.

The default profile should contain one unconfigured hole.

No audio routing should begin until the hole has valid endpoints and activation is requested.

### 5.2 Creating Profiles

Users should be able to:

- Create an empty profile.
- Duplicate an existing profile.
- Rename a profile.
- Add or remove holes.
- Configure simplex or duplex operation.
- Add notes.
- Save changes.

### 5.3 Initial Hole

A newly created profile should normally contain one unconfigured hole for convenience.

An explicitly empty profile must also be permitted.

### 5.4 Duplicate Profile

Duplicating a profile copies its configuration but creates new profile and hole IDs.

The duplicate must not activate automatically.

### 5.5 Unconfigured Holes

An unconfigured hole may exist in a saved profile.

It must not be treated as a valid route or activated using guessed endpoints.

The GUI should clearly identify incomplete configuration.

---

## 6. Hole Management

Each hole contains:

- Stable `hole_id`.
- Display name.
- Notes.
- Enabled state.
- Simplex or duplex mode.
- Explicit directional endpoint references.
- Audio operating preferences.
- Failure and recovery preferences.
- Optional failover configuration.

### 6.1 Adding Holes

Users may add simplex or duplex holes.

Each new hole receives a unique stable ID within its profile.

### 6.2 Renaming Holes

Renaming a hole changes its display name, not its ID.

### 6.3 Disabling Holes

A disabled hole remains saved but is excluded from activation.

Disabling a running hole requires applying the change.

### 6.4 Removing Holes

Removing a hole deletes its saved definition when committed.

If the hole is currently running, AudioHole must clearly indicate that applying the change will stop its audio paths.

### 6.5 Mirrored Destinations

Routing one source to multiple destinations requires additional holes in v0.1.

AudioHole should not silently introduce an implicit multichannel routing graph.

---

## 7. Connection Capacity

AudioHole v0.1 supports:

| Enabled holes requested for activation | Behavior |
|---|---|
| 0 | Valid; no routing required |
| 1–4 | Normal operating range |
| 5–8 | Extended range with a nonblocking advisory |
| More than 8 | Activation prohibited |

### 7.1 Counting Rules

A simplex hole counts as one.

A duplex hole counts as one.

A mirrored destination implemented through another hole counts as another hole.

### 7.2 Saved Capacity

Profiles may contain more than eight saved holes.

Disabled holes do not count toward the active capacity limit. Enabled holes in the armed plan reserve capacity even while waiting for hardware or recovery. Eight holes is a policy ceiling, not a guarantee of achievable performance on every endpoint combination.

### 7.3 Performance Advisory

Activating five through eight holes should produce a one-time performance advisory.

The advisory must not repeatedly interrupt normal use.

### 7.4 Capacity Failure

Attempting to activate more than eight enabled holes must be rejected with a clear explanation.

The saved profile remains intact.

---

## 8. Profile Status Model

AudioHole must distinguish configuration validity, activation intent, and runtime health.

These are related but separate concepts.

### 8.1 Configuration Validity

A profile may be:

- `valid`
- `incomplete`
- `invalid`

A valid profile may reference devices that are not currently available.

An incomplete profile has unresolved required configuration, such as an unassigned source endpoint.

An invalid profile contains structural or semantic errors that prevent safe interpretation.

### 8.2 Activation Intent

A profile may be:

- `stopped`
- `armed`

Stopped means routing is intentionally inactive.

Armed means AudioHole should establish and maintain the requested routing whenever prerequisites permit.

### 8.3 Runtime Status

The user-facing runtime statuses are:

| Status | Meaning |
|---|---|
| `stopped` | Routing intentionally inactive |
| `ready` | Armed and waiting for prerequisites that are not currently available |
| `starting` | Establishing requested connections |
| `running` | All enabled holes are operational |
| `degraded` | Some requested paths are operating; others are unavailable or failed |
| `recovering` | Previously established routing was interrupted and recovery is underway |
| `failed` | A routing error prevents all requested paths from operating |
| `stopping` | Shutdown of routing resources is in progress |

### 8.4 Ready Is Not Failure

A Ready profile is valid and armed but waiting for required devices or services.

Examples:

- A USB mixer is powered off.
- A headset has not connected.
- A virtual endpoint is not yet available.
- A streaming transport has not been established.

A Ready profile may wait indefinitely.

It should not generate repeated failure notifications merely because the expected device remains unavailable.

### 8.5 Recovering Versus Ready

Ready generally describes a profile waiting for prerequisites before establishing the requested routing.

Recovering describes previously established routing that was interrupted.

If recovery settles into a stable waiting condition, the profile may transition to Ready while remaining armed.

Diagnostic history must preserve the fact that an interruption occurred.

### 8.6 Failed Does Not Mean Stopped

A Failed profile remains armed unless the user explicitly stops it or switches away.

AudioHole must continue appropriate recovery attempts or event-driven rechecks.

A complete routing failure must not automatically abandon the profile.

### 8.7 Zero-Hole Profiles

A profile with no enabled holes has no requested audio routing.

It should be displayed as idle or stopped rather than failed.

### 8.8 Status Aggregation

Profile status must be derived from actual hole and path states.

A profile must not report Running while an enabled required path is unavailable.

The supervisor should distinguish expected waiting conditions from operational errors when selecting Ready, Degraded, Recovering, or Failed.

---

## 9. Loading and Activating Profiles

### 9.1 Loading

Loading selects a profile for inspection or activation.

Loading alone must not start routing when activation mode is Manual.

### 9.2 Validation

Before activation, AudioHole must validate:

- Profile schema.
- Unique hole IDs.
- Required endpoint references.
- Endpoint direction compatibility.
- Enabled-hole capacity.
- Audio preference values.
- Failover configuration.
- Known resource conflicts.

Missing physical devices do not automatically invalidate an otherwise valid profile.

### 9.3 Activation

Activation arms the profile and requests that the routing supervisor establish its enabled holes.

The supervisor resolves logical devices to current Windows endpoints, negotiates formats, and starts streams when possible.

### 9.4 Activation with Missing Hardware

If required hardware is unavailable, AudioHole should:

1. Preserve the activation request.
2. Enter Ready or Degraded as appropriate.
3. Monitor for relevant device availability changes.
4. Establish the affected paths when prerequisites become available.
5. Update status indicators.
6. Record the transition in diagnostics.

### 9.5 Explicit Stop

An explicit Stop command must:

- Stop active routing.
- Disarm the profile.
- Cancel pending delayed activation.
- Cancel automatic restart attempts associated with that session.
- Preserve the loaded profile.
- Preserve saved configuration.

The supervisor must not interpret intentional stopping as a recoverable failure.

---

## 10. Automatic Startup and Ready State

A central requirement is that AudioHole can start before the equipment it needs.

### 10.1 Example: Windows Startup Before VR Headset Connection

1. Windows starts.
2. AudioHole launches automatically.
3. The configured VR profile loads.
4. Routing activation is requested.
5. The Steam streaming endpoints are not yet usable.
6. The profile enters Ready.
7. AudioHole remains in the background.
8. The headset and streaming transport become available.
9. AudioHole establishes the configured paths.
10. The profile transitions to Running.
11. A silent informational notification may appear.

No manual reactivation should be necessary.

### 10.2 No Arbitrary Timeout

A profile must not permanently fail merely because an expected device takes a long time to appear.

Waiting for hardware may continue indefinitely.

### 10.3 Device Availability

Windows endpoint enumeration alone must not be treated as proof that a wireless or remote audio transport is usable.

AudioHole should distinguish an enumerated endpoint from a successfully usable route.

### 10.4 Startup Profile Selection

Supported selection modes:

- `last_used`
- `selected`

The selected profile is referenced by stable profile ID.

### 10.5 Activation Modes

Supported activation modes:

- `manual`
- `immediate`
- `delayed`

These apply whenever AudioHole launches, not only during Windows startup.

### 10.6 Delayed Activation and Profile Selection

An explicit Stop or Cancel Startup cancels pending delayed activation. If the user selects a different profile during an active countdown, the controller preserves the original deadline and updates the pending activation target. Selection does not immediately start routing or restart the countdown. GUI and tray immediately show the new target.

At expiration only the current pending target may activate. An invalid, deleted, or unresolved target produces an explicit error rather than silently selecting another profile. A valid profile with missing hardware may enter Ready. Start Routing Now bypasses the timer and activates the current target; Stop or Cancel Startup disarms it. A changed saved startup-profile preference alone does not retarget the current countdown: an explicit profile-selection action is required. Obsolete timer callbacks must have no effect.

---

## 11. Profile Switching

Profile switching must preserve working audio whenever possible.

**A failed profile switch must not replace a functioning profile with silence.**

### 11.1 Transactional Switching

The currently active profile remains authoritative until the destination profile is sufficiently prepared for transition.

The supervisor should:

1. Validate the destination profile.
2. Resolve required devices.
3. Check whether its requested routing can be established.
4. Identify reusable streams and paths.
5. Prepare additional resources where possible.
6. Commit the transition when sufficiently ready.
7. Release obsolete resources.
8. Report the resulting status.

### 11.2 Failed Switch

If the destination cannot establish its required routing, the switch should fail without replacing the previous running profile.

AudioHole must:

- Preserve the previous session where technically possible.
- Report the failed switch.
- Generate a notification according to user preferences.
- Avoid silently arming the unavailable destination in place of the previous profile.

### 11.3 Intentional Switch to Ready

The user may explicitly choose to arm a destination profile even when its required devices are unavailable.

This must be a deliberate action.

In that case, AudioHole may stop the previous profile and enter Ready with the destination profile.

### 11.4 Switching Between Compatible Profiles

Compatible, unchanged audio paths should remain running.

A path may be reused when its effective runtime requirements remain compatible, including:

- Source endpoint.
- Destination endpoint.
- Operating mode.
- Audio format.
- Channel mapping.
- Stream-sharing requirements.
- Relevant processing and buffering settings.

Matching display names or hole IDs alone does not establish compatibility.

### 11.5 Exclusive-Mode Conflicts

Exclusive-mode resources may prevent preparing the destination profile while the previous profile is running.

AudioHole must not promise seamless switching when hardware constraints make it impossible.

Where preparation requires interrupting the current session, AudioHole should warn the user before a potentially disruptive transition.

### 11.6 Commit and Rollback

The supervisor should preserve sufficient information to restore the previous session if a transition fails.

Rollback is best-effort when physical resources or drivers have already changed state.

If full rollback is impossible, AudioHole must preserve healthy available paths and report the actual result.

### 11.7 Selected Versus Active Profile

The GUI must distinguish:

- Profile being viewed.
- Profile currently running.
- Pending destination profile.
- Failed switch attempt.

Merely selecting another profile for inspection must not imply that it has become active.

---

## 12. Editing an Active Profile

### 12.1 Separate States

AudioHole should distinguish:

- Saved configuration.
- Editable configuration.
- Active runtime configuration.

### 12.2 Explicit Apply

Routing changes should be staged until the user selects Apply.

This prevents unexpected audio interruptions while editing.

### 12.3 Non-Disruptive Changes

Changes such as display name and notes may be saved without restarting audio.

### 12.4 Routing Changes

Changes requiring Apply include:

- Source or destination.
- Hole enablement.
- Simplex/duplex mode.
- Format preferences.
- Buffering preferences.
- Failover candidates.
- Failure policy.

### 12.5 Minimal Disruption

Applying changes should preserve compatible, unchanged paths.

### 12.6 Revert

The GUI should provide a way to discard unapplied changes.

### 12.7 External Changes

External modifications must be detected where practical.

If the GUI has unsaved edits, AudioHole must not silently overwrite either version.

---

## 13. Missing Devices

A profile remains valid when referenced hardware is temporarily unavailable.

### 13.1 Known but Unavailable

A device record exists, but the endpoint is not currently usable.

### 13.2 Unresolved Reference

The profile references a logical device ID absent from the device database.

This is a configuration issue, not merely a disconnected device.

### 13.3 Preservation

AudioHole must not remove unavailable devices from profiles automatically.

### 13.4 Activation

Available holes should be activated where possible.

Unavailable holes should remain Ready or otherwise appropriately represented.

### 13.5 Device Return

When a device becomes available, the supervisor should reevaluate affected paths.

Recovery must respect activation intent and failover policy.

### 13.6 Device Replacement

Users may explicitly associate replacement hardware with an existing logical device ID.

Relevant capabilities must be revalidated when the hardware differs.

---

## 14. Partial and Complete Failure

AudioHole must preserve the active profile through partial or complete routing failure.

### 14.1 Partial Failure

A partial failure occurs when some requested paths stop operating while others remain healthy.

The profile should enter Degraded or Recovering as appropriate.

Healthy unrelated paths should continue.

### 14.2 Complete Failure

A complete failure occurs when no requested path is operating because of an error.

The profile should enter Failed or Recovering as appropriate.

**Complete failure must not automatically disarm the profile.**

### 14.3 Notification

Partial and complete failures should generate a Windows toast notification, subject to the configured notification level and Windows notification availability.

Notifications must be deduplicated.

### 14.4 Persistent Recovery

The supervisor should continue appropriate recovery attempts or wait for relevant events.

It must avoid rapid retry loops and repeated disruptive initialization attempts.

### 14.5 Manual Stop

Only an explicit Stop, a committed profile switch, application exit, or another deliberate deactivation should cancel the session's recovery intent.

---

## 15. Duplex Failure Behavior

A duplex hole contains two related directional paths.

They may fail independently.

### 15.1 Default Policy

The default policy is:

`keep_healthy_direction`

If one direction fails, the other may continue.

### 15.2 Coupled Policy

An optional coupled policy may require both directions to be operational together.

The exact supported policy values must be defined consistently with `CONFIG_FORMAT.md`.

### 15.3 Status Reporting

The GUI should distinguish:

- Both directions operational.
- Forward only.
- Reverse only.
- Neither direction operational.
- Recovery in progress.

### 15.4 Microphone Safety

AudioHole must never create an unintended reverse microphone route during recovery.

Only explicitly configured paths may be activated.

---

## 16. Failover Behavior

Failover allows a directional path to use an alternate endpoint.

### 16.1 Candidate Lists

Candidates are referenced by stable logical device IDs.

They may have explicit priorities.

### 16.2 Supported Policies

- `prefer_highest_priority`
- `retain_current`
- `manual`

### 16.3 Default Policy

The recommended default is:

`retain_current`

Once a healthy fallback is operating, AudioHole should avoid unnecessary switching.

### 16.4 Failover Scope

Failover should affect only the relevant path unless the hole's failure policy requires coupled behavior.

### 16.5 Saved Configuration

Failover must not silently rewrite the profile's preferred endpoint.

### 16.6 Return to Primary

Returning to the preferred endpoint must follow the configured policy.

Brief endpoint reappearance must not trigger repeated switching.

### 16.7 Notification

Failover activation should be reported through the tray, diagnostics, and notifications according to user preferences.

---

## 17. Recovery and Reconnection

### 17.1 Recovery Triggers

Examples include:

- Device disconnection.
- Device reenumeration.
- Stream invalidation.
- Driver reset.
- Initialization failure.
- Wireless transport loss.
- Endpoint becoming available again.

### 17.2 Recovery Requirements

Recovery should:

- Preserve healthy unrelated holes.
- Avoid unnecessary session restarts.
- Reuse valid resources where possible.
- Use bounded retry attempts and appropriate backoff.
- Respond to relevant device-availability events.
- Avoid rapid switching loops.
- Report persistent errors.
- Respect explicit Stop.

### 17.3 Identity Resolution

Returning devices must be resolved through portable identity evidence and current machine-local bindings.

Ambiguous matches require confirmation.

### 17.4 Capability Changes

If a returning device has different capabilities, AudioHole should renegotiate affected paths.

Historical probe results must not be treated as unconditional guarantees.

### 17.5 Recovery Completion

When all requested paths become operational, the profile transitions to Running.

The event should be recorded and may generate a silent recovery notification.

---

## 18. Tray Status and Notifications

The system tray is a persistent, silent indicator of AudioHole's state.

Windows toast notifications supplement it.

### 18.1 Tray Indicators

The tray icon should distinguish at least:

| State | Suggested visual treatment |
|---|---|
| Stopped | Neutral |
| Ready | Waiting / amber |
| Starting | Transitional |
| Running | Healthy / green |
| Degraded | Warning / amber |
| Recovering | Recovery / amber |
| Failed | Error / red |

Colors must not be the only means of distinguishing states.

Tooltips and accessible labels should communicate the actual status.

### 18.2 Silent Informational Notifications

Normal transitions such as Ready → Running should use silent informational toasts.

A silent toast must not produce a notification sound.

Windows notification settings may still suppress its display.

### 18.3 Failure Notifications

Partial or complete failures should generate warning or error notifications.

The notification should identify:

- Profile.
- Affected hole or path, when known.
- Nature of the problem.
- Whether automatic recovery remains armed.

### 18.4 Recovery Notifications

When routing recovers, AudioHole may generate a silent informational toast.

### 18.5 Notification Preferences

The existing application notification levels apply:

- `all_events`
- `warnings_and_errors`
- `errors_only`

The default remains:

`warnings_and_errors`

Consequently, routine Ready → Running notifications are shown only when informational notifications are enabled.

### 18.6 Deduplication

AudioHole must avoid repeated toasts for the same persistent condition.

A new notification is appropriate when the condition meaningfully changes.

### 18.7 Authoritative Status

The tray icon and diagnostic history remain available even when Windows suppresses notifications.

A toast must never be the sole record of a failure.

---

## 19. External Status and Control

AudioHole should expose a consistent internal status model that can eventually be used by external controllers.

Examples include:

- Stream Deck.
- OBS plugins.
- Desktop automation utilities.
- Custom monitoring applications.

### 19.1 Observable Information

External consumers may need to inspect:

- Available profiles.
- Loaded profile.
- Active profile.
- Profile status.
- Hole status.
- Directional path status.
- Device availability.
- Failover state.
- Recent errors and recovery events.

### 19.2 Potential Commands

External controllers may eventually request:

- Load profile.
- Activate profile.
- Stop routing.
- Enable or disable a hole.
- Query current status.
- Request recovery or retry.

### 19.3 Stable Identifiers

External control must use stable profile, hole, and device IDs.

It must not depend on display names, filenames, or Windows endpoint IDs.

### 19.4 Single Authority

The routing supervisor remains the authority for routing decisions.

External applications must not manipulate audio-engine resources directly.

### 19.5 Consistent Behavior

Commands received through an external API must follow the same:

- Validation.
- Activation rules.
- Switching safeguards.
- Recovery policies.
- Capacity limits.
- Error reporting.

An external command must not bypass safety behavior simply because it did not originate in the GUI.

### 19.6 API Specification

Transport, authentication, command schemas, subscriptions, and versioning belong in `API.md`.

The API is a planned integration boundary, not a mandatory dependency for core v0.1 routing.

---

## 20. Profile Import and Export

### 20.1 Export

Profile export should preserve:

- Profile ID.
- Display name.
- Notes.
- Hole definitions.
- Device references.
- Failover configuration.
- Audio preferences.

### 20.2 Dependencies

A standalone profile may reference device records absent from the destination installation.

AudioHole should identify these dependencies.

### 20.3 Import

Import should:

1. Validate the schema.
2. Detect profile ID collisions.
3. Check device references.
4. Identify unresolved devices.
5. Offer a clear resolution path.
6. Preserve the profile even when hardware is disconnected.

### 20.4 Complete Migration

A complete portable configuration backup should include the relevant device records and fingerprints.

This allows the profile to be restored without reconstructing all device references.

### 20.5 Collision Handling

Importing a profile with an existing ID must not silently overwrite the existing profile.

Importing as a separate profile requires a new ID.

---

## 21. Profile Deletion

Deleting a profile is destructive.

### 21.1 Confirmation

AudioHole should request confirmation before deleting a profile.

### 21.2 Active Profile

Deleting the active profile requires stopping or switching away first.

### 21.3 Last Remaining Profile

Deleting the last profile may be permitted, but AudioHole should provide an obvious way to create another.

### 21.4 Device Records

Deleting a profile must not delete device records.

Device records may be shared across profiles.

---

## 22. Confirmations and User Safety

Confirmations should be reserved for destructive, disruptive, or surprising operations.

Examples:

- Deleting a profile.
- Removing an active hole.
- Applying changes that require stream interruption.
- Reassigning a logical device to different hardware.
- Replacing a profile during import.
- Switching profiles when exclusive-mode conflicts require interrupting the current session.

Routine inspection, renaming, and note editing should not require confirmation.

Error messages should identify the affected profile, hole, or endpoint and explain the consequence.

Dry humor may appear in secondary text, but must not obscure actionable diagnostics.

---

## 23. Performance and Resource Management

### 23.1 Resource Reuse

Compatible capture and render resources should be shared where safe.

### 23.2 Independent Clocks

Connected endpoints may use independent hardware clocks.

The audio engine must handle drift appropriately.

Matching nominal sample rates does not eliminate clock drift.

### 23.3 Latency Preferences

Profiles may express latency preferences.

These guide the audio engine but do not guarantee a particular end-to-end latency.

### 23.4 Real-Time Safety

Profile management must not perform JSON parsing, filesystem access, GUI operations, or blocking diagnostics on real-time audio threads.

### 23.5 Resource Conflicts

If a requested configuration cannot be established because of exclusive access or incompatible requirements, AudioHole should preserve unrelated healthy routing and report the conflict.

---

## 24. Manual Profile Editing

Profiles are ordinary JSON files.

Manual editing is a supported workflow.

### 24.1 Validation

External changes must be validated before affecting runtime routing.

### 24.2 Invalid Profiles

Invalid files should remain available for inspection and correction.

AudioHole must not silently replace them with defaults.

### 24.3 Active Profile Changes

External changes to an active profile must not immediately restart audio.

AudioHole should offer a controlled reload or Apply operation.

### 24.4 Filename Changes

Renaming a profile file updates its display name while preserving its stable ID.

### 24.5 Conflicts

If the GUI has unsaved edits when an external change is detected, AudioHole must not silently overwrite either version.

---

## 25. Profile Acceptance Tests

### 25.1 Simplex PA System

1. Create a simplex hole.
2. Assign microphone capture as source.
3. Assign speaker playback as destination.
4. Activate the profile.
5. Verify one directional path operates.
6. Verify the hole counts as one active connection.

### 25.2 Duplex Streaming Workflow

1. Create a duplex hole.
2. Configure mixer capture to streaming playback.
3. Configure streaming microphone to mixer playback.
4. Activate the profile.
5. Verify both directions operate.
6. Verify the duplex connection counts as one hole.

### 25.3 Ready at Windows Startup

1. Configure AudioHole to launch with Windows.
2. Select a profile requiring a disconnected device.
3. Configure automatic routing activation.
4. Launch AudioHole without the device.
5. Verify the profile enters Ready.
6. Verify the tray indicates waiting.
7. Connect the device.
8. Verify routing starts automatically.
9. Verify the profile transitions to Running.
10. Verify an informational event is recorded.

### 25.4 Partial Failure

1. Activate a duplex profile.
2. Disconnect one required endpoint.
3. Verify the affected direction stops.
4. Verify the healthy direction continues under the default policy.
5. Verify the profile reports Degraded or Recovering.
6. Verify a notification is generated according to preferences.
7. Reconnect the device.
8. Verify recovery.

### 25.5 Complete Failure

1. Activate a profile.
2. Make all requested paths fail.
3. Verify the profile reports Failed or Recovering.
4. Verify a failure notification is generated.
5. Verify the profile remains armed.
6. Restore the devices.
7. Verify routing resumes without manual reactivation.

### 25.6 Failed Profile Switch

1. Activate a working profile.
2. Attempt to switch to a profile requiring unavailable hardware.
3. Verify the switch does not commit.
4. Verify the previous profile continues operating.
5. Verify the user receives a clear explanation.

### 25.7 Intentional Switch to Ready

1. Activate a working profile.
2. Select an unavailable destination profile.
3. Explicitly choose to arm it despite missing devices.
4. Verify the previous session stops.
5. Verify the destination enters Ready.
6. Make its devices available.
7. Verify automatic activation.

### 25.8 Compatible Profile Switch

1. Activate Profile A.
2. Create Profile B with one unchanged path and one different path.
3. Switch to Profile B.
4. Verify compatible paths are preserved where technically possible.
5. Verify only necessary resources restart.

### 25.9 Active Profile Editing

1. Activate a profile.
2. Change an endpoint in the GUI.
3. Verify routing does not change before Apply.
4. Apply the change.
5. Verify the supervisor transitions to the new configuration.

### 25.10 Manual Stop

1. Activate a profile.
2. Trigger a recoverable failure.
3. Explicitly stop routing.
4. Restore the device.
5. Verify routing does not restart automatically.

### 25.11 Delayed Activation

1. Configure delayed activation.
2. Launch AudioHole.
3. Verify the startup profile loads.
4. Stop before the delay expires.
5. Verify pending activation is cancelled.

### 25.12 Failover

1. Configure a preferred endpoint and fallback.
2. Activate the profile.
3. Make the preferred endpoint unavailable.
4. Verify failover follows the selected policy.
5. Restore the preferred endpoint.
6. Verify return behavior follows the selected policy.

### 25.13 Notification Deduplication

1. Cause a persistent device failure.
2. Verify the failure is reported.
3. Allow multiple recovery attempts.
4. Verify identical notifications are not repeatedly generated.
5. Restore the device.
6. Verify the recovery event is recorded.

### 25.14 Tray State

1. Observe Stopped.
2. Arm a profile with unavailable hardware.
3. Verify Ready is indicated.
4. Make the hardware available.
5. Verify Running is indicated.
6. Cause a partial failure.
7. Verify Degraded is indicated.
8. Stop routing.
9. Verify the tray returns to Stopped.

### 25.15 Portable Configuration

1. Export the portable configuration.
2. Import it on a clean Windows installation.
3. Connect the original devices.
4. Verify logical-device matching.
5. Verify profiles activate without reconstructing their routing definitions.

### 25.16 Active-Hole Capacity

1. Create a profile containing nine saved holes.
2. Enable eight.
3. Verify activation is permitted with an advisory.
4. Enable the ninth.
5. Verify activation is rejected without damaging the saved profile.

### 25.17 External Status Consistency

1. Activate a profile.
2. Observe status through the GUI and internal event interface.
3. Cause a hole failure.
4. Verify both report consistent states.
5. Recover the hole.
6. Verify consistent recovery events.

This test establishes the internal status contract; it does not require a completed third-party plugin.

---

## 26. Deferred Features

The following are outside the initial v0.1 scope unless implementation reveals a compelling need:

- Profile inheritance.
- Nested profiles.
- Automatic profile switching based on application focus.
- Scheduled profile switching.
- Complex conditional routing rules.
- Arbitrary multi-node audio graphs.
- Cloud profile synchronization.
- Multi-user shared profile management.
- Full Stream Deck and OBS plugins.
- General-purpose scriptable automation.

The profile model should remain extensible without implementing these features prematurely.

---

## 27. Remaining Design Questions

### 27.1 Coupled Duplex Policy

The default is to preserve the healthy direction.

The exact behavior of an optional coupled-failure policy should be defined before implementation.

### 27.2 Exclusive-Mode Switching

The architecture must determine how to handle profile transitions that cannot be prepared without first releasing an exclusive-mode endpoint.

### 27.3 Import Collisions

The GUI must define a clear replacement-versus-duplicate workflow.

### 27.4 API Transport

The future external-control transport and message schemas belong in `API.md`.

These questions do not change the established profile lifecycle or failure-recovery requirements.

---

## 28. Guiding Principles

1. **Profiles describe desired routing, not current hardware state.**
2. **Loading a profile is not the same as activating it.**
3. **A hole is one logical connection, simplex or duplex.**
4. **Ready is a valid operating state, not an error.**
5. **An armed profile remains eligible for activation and recovery.**
6. **Partial or complete routing failure must not automatically disarm a profile.**
7. **A failed profile switch must preserve the previous working session whenever possible.**
8. **Missing devices must not destroy valid profiles.**
9. **Unchanged audio paths should survive profile switches whenever technically possible.**
10. **Editing configuration must not unexpectedly interrupt audio.**
11. **Healthy directions and unrelated holes should survive partial failures.**
12. **Failover must not silently rewrite preferred routing.**
13. **An explicit Stop command takes precedence over automatic recovery.**
14. **The tray icon provides persistent status; notifications report meaningful transitions.**
15. **External controllers must use the same status model and routing safeguards as the GUI.**
16. **Profile files must remain human-readable, manually editable, and portable.**

**A profile should behave like a saved patchbay: load it, arm it, and AudioHole establishes the intended connections whenever the equipment is ready, without requiring the user to rebuild the wiring or manually restart a failed route.**

---

## Reconciled activation and unresolved owner policy

The canonical public states are D-019's Stopped, Ready, Starting, Running, Degraded, Recovering, Failed, and Stopping. Resolving is an internal Starting phase. Enabled holes in the armed plan reserve capacity even when unavailable or recovering; eight is a capacity policy, not a universal throughput guarantee.

O-001 remains Open: successful configured fallback could be Running with an explicit fallback indication or remain Degraded. The recommendation is the former when all routing obligations are satisfied, but neither policy is accepted. The tray's fallback example is illustrative and must not settle aggregation. Always distinguish preferred and actual endpoints.

O-002 remains Open: “sufficiently prepared” in Section 11.1 does not yet define an all-path versus authorized-partial commit threshold. Preparing all enabled required paths is a recommendation, not an accepted rule. Existing D-022 safeguards remain: preserve previous working routing where technically possible, warn about disruptive exclusive transitions, retain best-effort rollback, and require deliberate activation into Ready rather than silently replacing working routing. Initial activation with missing hardware and switching away from working routing are distinct cases.

O-004 leaves exact coupled-duplex policy and persistence fields open. Preserve the existing `keep_healthy_direction` default. O-005 leaves direct-transfer eligibility and instability response open; nominal format matching never guarantees perpetual unchanged transfer. O-003/O-006 leave nondefault close/tray behavior and storage-root selection open.

All asynchronous session operations use generation-based cancellation and stale-result protection. Stop disarms the session and takes precedence over startup, recovery, failover, and pending scan work; obsolete completions cannot restart streams. This does not imply unsafe termination of a blocked driver call. Recovery remains bounded/backed off and event-aware rather than abandoning armed intent after a fixed hardware-wait timeout.

Device matching is conservative and explainable. Local bindings remain subject to current identity evidence, and ambiguous returning endpoints require confirmation. No silence-based failure inference, guessed reverse route, or silent substitution is permitted.

### Provisional directional synchronization preferences

One engine resolves synchronization independently for each direction, preserving D-053's shared-default inheritance and explicit overrides. Automatic is the proposed prototype default, not a finalized algorithm. Compare unchanged transfer, application-managed compensation, and Windows-managed shared-render adjustment where available. A processing change in one direction must respect existing healthy-direction isolation; no new coupled-failure values are defined.

Strict preservation remains internal/experimental, not a required public profile feature. Its candidate semantics reject incompatible formats and prohibit intentional modification, insertion, duplication, dropping, or resampling of captured PCM frames. Inability to continue is reported; restart/rebuffer is explicit, never silent compensation. Emergency recovery and upstream discontinuities cannot be hidden as preservation. O-004/O-005 remain Open: no new profile fields, enums, defaults in persisted files, or public actions are finalized. Concrete experimental controls do not become production configuration by implication.

## Owner-authorized per-hole tuning policy

New holes default to Automatic. Advanced Tuning is a configuration policy per
hole, independently gating both directions' retained manual settings. Disabled
settings remain stored and ignored for effective requests/validation; enabling
makes explicit overrides eligible, leaving individual Automatic fields managed.
Unsupported applicable settings fail clearly. Basic assignments and stable IDs
survive toggling. Editing or saving a policy never changes the copied live plan;
explicit validated Apply/Activate is required. Requested/saved/runtime values stay
separate. A buffer-only update cannot silently enable a live route's policy.

The Test-only v3 representation preserves directional settings in place and
migrates missing legacy flags conservatively to the previous active-tuning policy.
See PROVISIONAL_TEST_PERSISTENCE.md for backup, restart, unknown-field and import
contracts. This does not close O-004/O-006 or implement production persistence,
real negotiation, channel mapping, fallback or clock synchronization algorithms.


Routing Engine is a per-hole requested selector, shared by the duplex directions.
Only Automatic is offered now. Its Test JSON identifier is `routing_engine` with
`automatic` as default, including when absent in older files. It obeys the Advanced
Tuning gate; future unavailable requests are preserved inactive or explicitly
rejected when eligible, never silently mapped to a different implementation.
