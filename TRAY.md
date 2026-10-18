# AudioHole v0.1 — System Tray & Notifications

**Document:** `TRAY.md`  
**Status:** Proposed design specification  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `CONFIG_FORMAT.md`, `PROFILES.md`, `GUI.md`, `API.md`

---

## 1. Purpose

AudioHole is designed to operate continuously in the background, routing audio between Windows endpoints without requiring its main window to remain open.

The Windows system tray (notification area) provides AudioHole's persistent everyday interface.

The tray must allow users to:

- Understand the current routing status at a glance.
- Identify the active profile.
- Start and stop routing.
- Switch profiles.
- Inspect individual hole states.
- Recognize failures and recovery activity.
- Open the main GUI.
- Exit AudioHole explicitly.

Windows toast notifications supplement the tray by reporting meaningful events.

The tray and notification system must not interrupt or interfere with real-time audio processing.

---

## 2. Design Principles

1. AudioHole remains usable without its main window open.
2. The tray icon communicates overall routing health.
3. Common operations should require minimal interaction.
4. Status must reflect actual routing behavior.
5. Profile selection and activation must remain distinct.
6. Stopping routing must not terminate AudioHole.
7. Exiting AudioHole must stop routing and terminate the application.
8. Notifications should be informative without becoming distracting.
9. Automatic recovery should not require user intervention.
10. Status must remain understandable without relying on color.
11. The tray, GUI, and optional API must use the same authoritative application state.
12. Tray interactions must never directly manipulate real-time audio buffers.

---

## 3. Application Lifecycle

AudioHole consists of an application controller, routing supervisor, audio engine, and user interfaces.

The tray is part of the application's persistent control interface.

### 3.1 Main Window Closed

Closing the main GUI hides the window.

AudioHole remains running.

Any active routing continues uninterrupted.

The tray icon remains available.

### 3.2 Main Window Reopened

The user may reopen the GUI through the tray icon or tray menu.

Reopening the GUI must not restart routing or change the active profile.

### 3.3 Stop Routing

Stop Routing disarms the active routing session.

It must:

- Stop active audio streams.
- Cancel pending activation.
- Cancel automatic recovery attempts.
- Cancel pending failover operations.
- Leave AudioHole running.
- Leave the tray icon available.

The resulting routing state is Stopped.

### 3.4 Exit AudioHole

Exit AudioHole explicitly terminates the application.

It must:

- Stop routing.
- Cancel pending routing operations.
- Perform orderly application shutdown.
- Close optional external-control listeners.
- Release audio resources.
- Remove the tray icon.

Exit must not be treated as equivalent to hiding the GUI.

### 3.5 Application Startup

When AudioHole launches, the tray icon should become available as soon as practical.

Routing startup behavior follows the user's settings.

The application may start:

- With no profile armed.
- With a selected profile armed.
- With immediate activation.
- With delayed activation.
- In Ready state while waiting for required devices.

The tray must reflect the actual state throughout startup.

---

## 4. Tray Icon Status Model

The tray icon communicates the overall state of the active routing session.

The canonical routing states are defined in `PROFILES.md`.

| State | Suggested Appearance | Meaning |
|---|---|---|
| Stopped | Neutral gray | No routing session is armed |
| Ready | Amber | Profile armed; waiting for required resources |
| Starting | Transitional | Routing initialization in progress |
| Running | Green | Requested active routing is operating |
| Degraded | Amber with warning distinction | Some requested paths are not operating |
| Recovering | Amber with recovery distinction | AudioHole is attempting to restore interrupted routing |
| Failed | Red | Routing cannot currently operate because of an error |
| Stopping | Transitional | Routing shutdown in progress |

### 4.1 State Accuracy

The icon must reflect the routing supervisor's authoritative state.

It must not indicate Running merely because the user requested activation.

### 4.2 Noncolor Distinctions

Color alone is insufficient.

The icon design should include recognizable differences in shape, overlay, or symbol where practical.

Examples:

- Stopped: neutral icon.
- Ready: waiting indicator.
- Running: active indicator.
- Degraded: warning indicator.
- Recovering: recovery indicator.
- Failed: error indicator.

Exact artwork remains a visual design decision.

### 4.3 Icon Size

The icon must remain legible at ordinary Windows notification-area sizes.

Fine text, complex diagrams, and multiple tiny indicators should be avoided.

### 4.4 Animation

Subtle animation may be considered for Starting or Recovering.

Animation is optional and must not consume significant resources.

The status must remain understandable without animation.

### 4.5 Theme Compatibility

Icons should remain legible against light and dark Windows taskbar themes.

### 4.6 Display Scaling

The tray implementation should support appropriate Windows DPI scaling and icon sizes.

---

## 5. Tray Tooltip

Hovering over the tray icon should display a concise status summary.

### 5.1 Basic Information

The tooltip should identify:

- AudioHole.
- Active profile, if any.
- Overall routing state.
- A brief status explanation where space permits.

### 5.2 Examples

**Running**

```text
AudioHole
Profile: VR Audio
Status: Running
2 holes active
```

**Ready**

```text
AudioHole
Profile: VR Audio
Status: Ready
Waiting for Steam Streaming Audio
```

**Degraded**

```text
AudioHole
Profile: VR Audio
Status: Degraded
1 of 2 holes unavailable
```

**Recovering**

```text
AudioHole
Profile: VR Audio
Status: Recovering
Reconnecting RØDECaster
```

**Stopped**

```text
AudioHole
Status: Stopped
```

### 5.3 Tooltip Limitations

Windows notification-area tooltips have practical length and formatting limitations.

AudioHole should prioritize the most important information.

If a complete explanation does not fit, the tray menu or main GUI should provide further detail.

### 5.4 Device Names

Long device names should be shortened carefully.

The full device name must remain accessible through the GUI.

### 5.5 No Misleading Counts

A duplex hole counts as one logical hole.

Tooltip counts must not count its two directional paths as separate holes.

---

## 6. Tray Interaction

### 6.1 Left Click

A single left-click may open a compact status menu or panel if the chosen Windows framework supports this reliably.

For v0.1, standard Windows tray behavior should take priority over a custom interaction model.

### 6.2 Double Click

Double-clicking the tray icon should open or focus the main AudioHole GUI.

### 6.3 Right Click

Right-clicking should open the primary tray context menu.

### 6.4 Keyboard Accessibility

The tray menu should remain usable through standard Windows keyboard interaction where supported.

### 6.5 Interaction Reliability

The application must not depend on timing-sensitive click combinations to expose essential controls.

All essential actions must be available through the context menu.

---

## 7. Primary Tray Menu

The context menu should remain compact.

A proposed structure is:

```text
AudioHole — Running
Profile: VR Audio

Profiles                  >
Holes                     >

Start Routing
Stop Routing

Open AudioHole
---------------------------
Exit AudioHole
```

Actual enabled and disabled actions depend on the current state.

### 7.1 Status Header

The menu should show the current overall status.

The status header is informational, not an activation control.

### 7.2 Active Profile

The active profile should be clearly identified.

If no profile is active, display an appropriate neutral label.

### 7.3 Start Routing

Start Routing should arm or activate the selected profile according to the application's activation rules.

If the required devices are unavailable, the profile may enter Ready.

### 7.4 Stop Routing

Stop Routing explicitly disarms routing.

It must remain distinct from Exit AudioHole.

### 7.5 Open AudioHole

Opens or focuses the main GUI.

### 7.6 Exit AudioHole

Stops routing and exits the application.

### 7.7 Menu State

Actions that cannot currently be performed should be disabled or replaced with contextually appropriate alternatives.

The menu must not display a command as available when its required context is missing.

---

## 8. Profiles Submenu

The Profiles submenu provides quick access to saved profiles.

### 8.1 Profile List

Each saved profile should appear by its user-assigned display name.

The active profile should be visibly distinguished.

### 8.2 Profile Activation

Selecting an inactive profile should request activation through the application controller.

**Exception:** During a pending startup countdown, selection only retargets the pending activation without restarting or bypassing the countdown (Section 10.8.11).

Otherwise it must use the same transactional switching rules as the GUI.

### 8.3 Failed Switch

If a requested profile cannot be activated, AudioHole should preserve the previously working profile where technically possible.

The tray must not falsely indicate that the destination profile became active.

### 8.4 Explicit Ready Activation

If the destination profile cannot currently operate because required devices are unavailable, the user may deliberately choose to arm it in Ready state.

This must be an explicit action, not an accidental consequence of selecting a profile.

### 8.5 Profile Names

Long profile names should remain distinguishable.

If menu presentation requires truncation, the GUI must provide access to the full name.

### 8.6 Large Profile Collections

If the user has many profiles, the menu may use a practical maximum display count with an option to open the full profile manager.

A searchable custom tray interface is not required for v0.1.

### 8.7 Profile Editing

Creating, renaming, deleting, and extensively editing profiles belong in the GUI.

The tray is primarily an operational interface.

---

## 9. Holes Submenu

The Holes submenu provides a compact overview of the active profile's connections.

### 9.1 Purpose

Users should be able to inspect individual hole states without opening the main GUI.

### 9.2 Hole Entries

Each entry should identify:

- Hole display name.
- Simplex or duplex type.
- Current status.
- Directional status where relevant.

### 9.3 Simplex Example

```text
Holes >
    Microphone → Speakers       Running
```

### 9.4 Duplex Example

```text
Holes >
    RØDECaster ↔ Steam Audio    Running
        Forward                Running
        Reverse                Running
```

### 9.5 Degraded Duplex Example

```text
Holes >
    RØDECaster ↔ Steam Audio    Degraded
        Forward                Running
        Reverse                Recovering
```

### 9.6 Hole Counts

A duplex hole remains one logical hole.

Its directional paths may be displayed separately for diagnostic clarity.

### 9.7 Hole Actions

For v0.1 the Holes submenu is for status inspection: hole name, overall state, and per-direction state for duplex holes. It may offer **Open hole in GUI**. Individual hole enable/disable commands are deferred beyond v0.1. Profile-level Start/Stop and profile selection remain available.

### 9.8 Persistence of Quick Changes

Individual hole quick toggles are deferred beyond v0.1, so no tray-specific temporary or persistent toggle semantics are defined. Any future implementation must reconcile persistence, staged edits, controller state, and optional API behavior first.

### 9.9 Menu Complexity

The submenu should not attempt to reproduce the complete profile editor.

Detailed format, buffer, failover, and device configuration belongs in the GUI.

---

## 10. Start and Stop Behavior

### 10.1 Start from Stopped

When the user selects Start Routing:

1. The application controller receives the request.
2. The routing supervisor validates the selected profile.
3. AudioHole attempts to prepare the required streams.
4. The tray reflects Starting, Ready, Running, or an appropriate failure state.

### 10.2 Ready State

If required hardware is unavailable, a valid armed profile may enter Ready.

AudioHole should continue waiting according to the configured recovery and activation behavior.

### 10.3 Stop from Running

Stop Routing terminates the active routing session.

The tray changes to Stopped after shutdown completes.

### 10.4 Stop from Ready

Stop Routing disarms the waiting profile.

AudioHole must not automatically begin routing when the missing device subsequently appears.

### 10.5 Stop from Recovering

Stop Routing cancels recovery activity.

No further automatic retry should occur until routing is explicitly armed again.

### 10.6 Stop During Delayed Activation

Stop Routing cancels the pending activation timer.

### 10.7 Repeated Commands

Repeated Start or Stop requests should be handled safely.

They must not create duplicate audio streams or inconsistent state.

### 10.8 Delayed Startup

AudioHole must support configurable delayed routing activation, as defined in `GUI.md`.

The tray must provide full visibility and control over pending activation, even when the main GUI is closed.

#### 10.8.1 Countdown Behavior

When AudioHole launches with Delayed activation enabled:

1. The application starts normally and creates its tray icon.
2. The selected startup profile is resolved and validated.
3. A nonblocking countdown begins using the configured activation delay.
4. Routing remains inactive during the countdown.
5. When the countdown expires, AudioHole attempts to activate the selected profile.
6. If required devices are unavailable, the profile enters Ready and waits for them.

The delay begins when AudioHole starts, not when Windows begins booting.

#### 10.8.2 Tray Status

During the countdown, the tray must indicate that automatic activation is pending.

Example tooltip:

```text
AudioHole
Profile: VR Audio
Status: Stopped
Starting in 15 seconds
```

The tray may use the neutral Stopped icon with a pending-activation distinction.

A countdown must not be represented as Running or Ready before activation actually begins.

Delayed activation is a startup substate, not an additional canonical routing state.

#### 10.8.3 Tray Menu

During a pending countdown, the menu should display:

```text
AudioHole — Starting in 15s
Profile: VR Audio

Profiles                  >
Holes                     >

Start Routing Now
Cancel Startup

Open AudioHole
---------------------------
Exit AudioHole
```

**Start Routing Now** immediately bypasses the remaining countdown and requests activation.

**Cancel Startup** cancels the pending activation and leaves AudioHole running in Stopped state.

Outside the countdown, these controls revert to the ordinary Start Routing and Stop Routing commands.

Cancel Startup must use the same underlying disarm operation as Stop Routing.

#### 10.8.4 Countdown Updates

The remaining time should update at a human-readable interval, approximately once per second.

Updates must not:

- Block the application controller.
- Interrupt audio.
- Require the main GUI.
- Generate notification toasts.
- Repeatedly rebuild an open menu unnecessarily.

If the tray menu is already open, its countdown display may remain a snapshot until reopened, provided the authoritative countdown remains correct.

#### 10.8.5 Manual Override

Selecting Start Routing Now must:

1. Cancel the pending timer.
2. Prevent the timer from subsequently firing.
3. Request immediate activation through the application controller.
4. Update the tray according to the actual routing result.

The timer must not create a duplicate activation request.

#### 10.8.6 Cancellation

Selecting Cancel Startup must:

1. Cancel the countdown.
2. Disarm pending routing.
3. Leave the application running.
4. Set the routing state to Stopped.
5. Prevent automatic activation when the original deadline passes.

Closing the GUI must not cancel the countdown.

Exiting AudioHole must cancel the countdown before application shutdown.

#### 10.8.7 Timer Expiration

When the countdown expires, AudioHole follows the normal activation sequence.

Possible resulting states include:

- Starting.
- Ready.
- Running.
- Degraded.
- Failed.

The tray must reflect the actual result.

#### 10.8.8 Recovery Independence

The startup delay applies once per application launch.

It must not be repeated during:

- Device reconnection.
- Automatic stream recovery.
- Failover.
- Return from fallback.
- Subsequent profile switching.

Those operations follow their own policies.

#### 10.8.9 Configuration Changes

If startup settings are changed while a countdown is active, AudioHole should preserve the existing countdown unless the user explicitly cancels or restarts it.

Changes to the configured delay take effect on the next application launch.

This avoids unexpected changes to an already scheduled activation.

#### 10.8.10 State Synchronization

The GUI and tray must display the same pending-activation deadline and selected profile.

The countdown belongs to the application controller, not to either interface.

The tray must never maintain its own independent activation timer.


---

#### 10.8.11 Profile Selection During Countdown

Selecting a different profile from the tray during delayed startup keeps the original countdown and deadline, updates the pending target, and immediately updates tray status. Selection does not bypass the delay. Only the current pending target may activate when the timer expires; stale callbacks cannot activate the previous selection. Start Routing Now bypasses the timer, while Cancel Startup disarms it. The tray must not own a separate timer and must distinguish pending selection from the active routing session.

---

## 11. Windows Toast Notifications

AudioHole should use native Windows toast notifications where supported.

Notifications report meaningful events that may require attention or provide useful confirmation.

### 11.1 Notification Levels

The application should support the following preferences:

- **All Events**
- **Warnings and Errors** — Default
- **Errors Only**

These settings should be available in the main GUI.

### 11.2 All Events

Includes informational state changes as well as warnings and errors.

### 11.3 Warnings and Errors

Reports degraded routing, meaningful interruptions, failed recovery, and other significant problems.

Routine successful transitions should generally remain silent.

### 11.4 Errors Only

Reports serious failures requiring attention.

### 11.5 Windows Notification Settings

AudioHole must respect Windows notification suppression and user preferences.

The tray icon remains the persistent status indicator even when toasts are suppressed.

---

## 12. Notification Events

Potential notification events include:

| Event | Typical Severity | Notification Behavior |
|---|---|---|
| Profile activated successfully | Informational | All Events |
| Ready → Running | Informational | Silent toast under All Events |
| Profile entered Ready | Informational | All Events |
| Hole degraded | Warning | All Events or Warnings and Errors |
| Device disconnected during routing | Warning | All Events or Warnings and Errors |
| Recovery started | Warning or informational | According to significance |
| Recovery succeeded | Informational | All Events |
| Recovery failed | Error | All applicable levels |
| Profile activation failed | Error | All applicable levels |
| Profile switch rejected | Warning or error | According to cause |
| Device identity ambiguous | Warning | All Events or Warnings and Errors |
| Fatal routing error | Error | All applicable levels |

### 12.1 Ready to Running

When AudioHole starts with Windows and waits for unavailable hardware, the profile may remain Ready.

When the device appears and routing begins successfully:

- The tray changes to Running.
- An informational toast may be displayed if All Events is selected.
- That toast should be silent.
- Audio should begin without requiring the main GUI.

### 12.2 Failures

Partial or complete routing failures deserve clear notification according to the selected notification level.

The profile remains armed unless explicitly stopped.

### 12.3 Recovery

Successful automatic recovery should update the tray immediately.

An informational notification may be generated if enabled.

### 12.4 Explicit User Actions

Failures resulting from direct user actions should also be explained through the initiating interface.

A toast must not be the only indication that an attempted profile switch failed.

---

## 13. Notification Content

Notifications should be concise and actionable.

### 13.1 Example — Device Disconnected

**AudioHole — Routing Degraded**

"RØDECaster Pro II disconnected. AudioHole is attempting recovery."

### 13.2 Example — Recovery Failed

**AudioHole — Routing Failed**

"Unable to restore the reverse path in VR Audio. Automatic recovery remains armed."

### 13.3 Example — Ready to Running

**AudioHole — Routing Started**

"VR Audio is now running."

This informational notification should be silent.

### 13.4 Example — Failed Profile Switch

**AudioHole — Profile Switch Failed**

"VR Audio could not be activated. The previous profile remains running."

### 13.5 No Unsupported Claims

Notifications must not claim recovery succeeded until the routing supervisor confirms the relevant paths are operating.

### 13.6 Device Names

Use user-assigned device names where appropriate.

If a name is ambiguous, provide enough context to identify the affected endpoint.

---

## 14. Notification Deduplication

Audio devices may repeatedly disconnect, reconnect, or fail initialization.

AudioHole must avoid producing excessive notifications.

### 14.1 Repeated Events

Repeated identical failures should be grouped or suppressed within a reasonable interval.

### 14.2 State Transitions

A genuine change in state may justify a new notification.

### 14.3 Recovery Loops

Automatic retry loops must not generate a new toast on every attempt.

### 14.4 Escalation

A warning that becomes a serious failure may generate a new notification even if earlier warnings were suppressed.

### 14.5 Notification History

Persistent diagnostic logs should retain meaningful events even when their toast notifications are deduplicated.

### 14.6 User Control

Notification-level preferences should be sufficient for v0.1.

Per-device notification rules may be considered later.

---

## 15. Notification Sounds

### 15.1 Informational Transitions

Routine informational notifications should be silent where specified.

In particular, Ready → Running should not produce a notification sound.

### 15.2 Warnings and Errors

Warnings and errors may use normal Windows notification behavior.

### 15.3 Audio Routing Considerations

AudioHole should avoid generating unnecessary sounds that could be captured or routed through the user's audio setup.

### 15.4 System Control

Windows notification settings ultimately determine whether notification sounds are played.

AudioHole should not attempt to override system-level suppression.

---

## 16. Tray and Automatic Recovery

The tray must accurately represent recovery activity.

### 16.1 Missing Device at Startup

A valid profile waiting for a missing device enters Ready.

This is not automatically a failure.

### 16.2 Device Lost During Routing

If a previously operating device disappears, AudioHole enters an appropriate degraded or recovery state.

### 16.3 Partial Failure

If some paths remain operational, the overall profile may be Degraded.

### 16.4 Complete Failure

If no required routing can operate because of an error, the profile may enter Failed.

### 16.5 Armed State

Ready, Recovering, Degraded, and Failed do not imply that routing has been explicitly stopped.

Automatic recovery may remain armed.

### 16.6 Explicit Stop

Stop Routing cancels automatic recovery.

### 16.7 Tray Updates

The tray should update promptly when:

- A device disappears.
- A device reappears.
- A hole fails.
- A hole recovers.
- Failover activates.
- The profile changes state.

### 16.8 No GUI Dependency

Automatic recovery and tray status updates must function while the main GUI is closed.

---

## 17. Failover Indication

AudioHole may use configured fallback devices when a preferred endpoint becomes unavailable.

### 17.1 Actual Endpoint

The tray must not imply that the preferred device is still active when a fallback is in use.

### 17.2 Tooltip

Where space permits, the tooltip may indicate that failover is active.

Example:

```text
AudioHole
Profile: VR Audio
Status: Degraded
Using fallback headset
```

### 17.3 Hole Submenu

The Holes submenu should provide more specific information about the affected path.

### 17.4 Notification

A meaningful failover event may generate a warning or informational notification depending on the circumstances.

### 17.5 Return to Preferred Device

Returning from fallback to the preferred endpoint must follow the configured failover and recovery policy.

The tray must not independently decide when to switch devices.

---

## 18. Tray State Synchronization

The tray, GUI, and optional API must observe the same application state.

### 18.1 Authoritative Source

The routing supervisor and application controller provide authoritative status.

### 18.2 No Independent Routing State

The tray must not maintain a separate interpretation of which profile is active or which streams are running.

### 18.3 Command Dispatch

Tray commands must use the same application-controller interface as GUI and external-control commands.

### 18.4 Asynchronous Operations

Operations such as profile switching may complete asynchronously.

The tray should distinguish a requested action from a confirmed result.

### 18.5 Rapid State Changes

The tray should handle rapid state transitions without displaying stale status indefinitely.

### 18.6 Multiple Interfaces

Commands originating from the GUI or API must update the tray.

Commands originating from the tray must update the GUI and API status.

---

## 19. Tray Performance

The tray interface must impose minimal overhead.

### 19.1 Audio Isolation

Tray rendering, menus, and notifications must not execute on real-time audio threads.

### 19.2 Event-Driven Updates

Status should update primarily in response to state changes.

Constant high-frequency polling is unnecessary.

### 19.3 Menu Construction

Menu generation should use a consistent state snapshot.

### 19.4 Long Operations

Device scanning, profile validation, and stream initialization must not block tray interaction.

### 19.5 Resource Use

The tray should remain lightweight even when AudioHole has many saved profiles or device records.

---

## 20. Startup and Shutdown Edge Cases

### 20.1 Windows Startup

If AudioHole is configured to start with Windows, the tray should appear without requiring the main GUI to open.

### 20.2 Missing Startup Device

If the selected startup profile is valid but its devices are unavailable, AudioHole enters Ready.

### 20.3 Startup Error

If configuration loading fails, AudioHole should remain accessible through the tray where possible.

The user should be able to open the GUI to investigate.

### 20.4 Shutdown During Recovery

Exit AudioHole must cancel recovery before releasing resources.

### 20.5 Windows Session Ending

AudioHole should respond to Windows session shutdown appropriately.

It must not display unnecessary blocking dialogs during ordinary system shutdown.

### 20.6 Explorer Restart

If Windows Explorer restarts and the notification area is recreated, AudioHole should restore its tray icon where supported.

### 20.7 Duplicate Instances

AudioHole should avoid creating multiple conflicting tray icons or routing controllers for the same application instance.

Single-instance behavior should be coordinated with the application architecture.

---

## 21. Error Handling

### 21.1 Tray Icon Failure

Failure to create the tray icon must not silently corrupt routing state.

The application should log the failure and provide an alternative way to access controls where possible.

### 21.2 Notification Failure

Failure to display a toast must not affect audio routing.

### 21.3 Invalid Profile

An invalid profile must not be activated merely because it appears in the tray menu.

### 21.4 Missing Device

A missing saved device must not be silently replaced with an unrelated endpoint.

### 21.5 Failed Command

A failed tray command should produce a clear explanation through an appropriate UI mechanism.

### 21.6 Stale Menu

If the application state changes while a menu is open, commands must be validated against current state when executed.

---

## 22. Accessibility and Windows Integration

### 22.1 Standard Windows Behavior

Prefer conventional Windows notification-area interactions.

### 22.2 Keyboard Support

Essential menu actions should be keyboard accessible.

### 22.3 Screen Readers

Menu labels and status descriptions should remain understandable through accessibility technologies where supported.

### 22.4 Color Independence

Status must not depend exclusively on icon color.

### 22.5 High-DPI Support

Tray artwork should remain legible under Windows display scaling.

### 22.6 Focus Management

Opening the GUI from the tray should bring the existing window forward rather than creating duplicate windows.

### 22.7 Notification Preferences

Respect Windows Focus Assist, Do Not Disturb, and other applicable notification controls.

---

## 23. Configuration Transparency

Tray preferences must be stored in the human-readable configuration defined by `CONFIG_FORMAT.md`.

### 23.1 Persistent Preferences

Examples include:

- Start with Windows.
- Start minimized.
- Close to tray.
- Notification level.
- Startup profile.
- Startup activation behavior.

### 23.2 No Hidden Profile State

The tray must not store separate opaque profile definitions.

### 23.3 Runtime State

Transient routing status may be maintained in machine-local runtime state as specified elsewhere.

### 23.4 Manual Edits

Manual configuration edits should be detected and validated according to the application's established configuration rules.

---

## 24. Tray Acceptance Tests

### 24.1 Basic Lifecycle

1. Launch AudioHole.
2. Verify the tray icon appears.
3. Open the GUI.
4. Close the GUI.
5. Verify the tray remains.
6. Verify routing continues.
7. Reopen the GUI.

### 24.2 Start and Stop

1. Select a valid profile.
2. Start routing from the tray.
3. Verify the state transitions correctly.
4. Stop routing from the tray.
5. Verify streams stop.
6. Verify the application remains running.

### 24.3 Explicit Exit

1. Start routing.
2. Select Exit AudioHole.
3. Verify streams stop.
4. Verify the application terminates.
5. Verify the tray icon is removed.

### 24.4 Ready State

1. Configure a startup profile requiring an unavailable device.
2. Launch AudioHole.
3. Verify Ready is displayed.
4. Connect the device.
5. Verify routing starts automatically.
6. Verify the tray changes to Running.

### 24.5 Ready to Running Notification

1. Enable All Events.
2. Arm a profile with a missing device.
3. Connect the device.
4. Verify a silent informational toast is requested.
5. Verify the tray changes to Running.

### 24.6 Notification Preferences

1. Select All Events.
2. Verify informational notifications are eligible.
3. Select Warnings and Errors.
4. Verify routine informational notifications are suppressed.
5. Select Errors Only.
6. Verify warnings are suppressed while serious errors remain eligible.

### 24.7 Partial Failure

1. Activate a profile containing multiple holes.
2. Disconnect one required endpoint.
3. Verify the overall state becomes Degraded where appropriate.
4. Verify the affected hole is identifiable.
5. Verify a warning notification is generated according to preferences.

### 24.8 Complete Failure

1. Activate a working profile.
2. Cause a failure preventing all required routing.
3. Verify the tray indicates Failed or Recovering as appropriate.
4. Verify the profile remains armed.
5. Verify automatic recovery can proceed.

### 24.9 Explicit Stop During Recovery

1. Cause a routing interruption.
2. Verify recovery begins.
3. Select Stop Routing.
4. Verify retries stop.
5. Reconnect the device.
6. Verify routing does not restart automatically.

### 24.10 Failed Profile Switch

1. Activate a working profile.
2. Attempt to switch to an unavailable profile.
3. Verify the previous profile remains running.
4. Verify the tray continues identifying the correct active profile.
5. Verify the failure is explained.
6. Verify explicit Ready activation remains available.

### 24.11 Duplex Status

1. Activate a duplex hole.
2. Verify the tray counts it as one hole.
3. Verify both directional paths are visible in the Holes submenu.
4. Interrupt one direction.
5. Verify the affected direction is identifiable.

### 24.12 Notification Deduplication

1. Cause repeated equivalent recovery failures.
2. Verify AudioHole does not generate a toast for every retry.
3. Verify diagnostic history still records relevant events.
4. Verify meaningful escalation can produce a new notification.

### 24.13 GUI Synchronization

1. Start routing from the GUI.
2. Verify the tray updates.
3. Stop routing from the tray.
4. Verify the GUI updates.
5. Switch profiles through the optional API, if implemented.
6. Verify both interfaces reflect the same state.

### 24.14 Explorer Restart

1. Launch AudioHole.
2. Restart Windows Explorer.
3. Verify the tray icon is restored where supported.
4. Verify routing remains unaffected.

### 24.15 Startup Minimized

1. Enable Start with Windows.
2. Enable minimized startup.
3. Start a new Windows session.
4. Verify AudioHole starts without opening the main GUI.
5. Verify the tray appears.
6. Verify startup routing behavior follows configuration.

---

### 24.16 Delayed Startup

1. Configure Delayed activation with a 30-second delay.
2. Launch AudioHole.
3. Verify the tray appears immediately.
4. Verify the selected startup profile is displayed.
5. Verify the countdown is visible.
6. Verify routing remains inactive during the countdown.
7. Close the main GUI and verify the countdown continues.
8. Allow the timer to expire and verify activation is attempted exactly once.
9. Repeat the test and select Start Routing Now.
10. Verify activation begins immediately and the original timer cannot trigger a second activation.
11. Repeat the test and select Cancel Startup.
12. Verify routing remains Stopped after the original deadline passes.
13. Repeat with required hardware disconnected.
14. Verify timer expiration transitions to Ready rather than repeatedly restarting the countdown.
15. Reconnect the hardware and verify automatic activation occurs without another startup delay.
16. Verify changes to startup-delay settings during an active countdown do not unexpectedly modify its deadline.
17. Select a different profile during an active countdown. Verify the remaining duration is preserved and only the newly selected profile activates at expiration.
18. Cancel a countdown, then select another profile. Verify the selection does not rearm or restart the canceled countdown.


## 25. Open Design Decisions

### 25.1 Single Left-Click Behavior

Determine whether single-click opens a compact status view or performs no special action beyond standard Windows behavior.

Double-click and right-click behavior should remain conventional.

### 25.2 Hole Quick Toggles — Deferred

Individual hole enable/disable quick actions are deferred beyond v0.1. Hole-status inspection remains supported; persistence semantics are not part of the initial release.

### 25.3 Menu Organization

Determine the final placement of profile selection, hole status, and routing controls after testing with realistic profile collections.

### 25.4 Notification History

Determine whether a dedicated notification-history view is useful or whether existing diagnostic logs provide sufficient history.

### 25.5 Icon Artwork

Define final icons for each routing state.

The artwork must remain readable at small sizes and without relying on color.

### 25.6 Optional Animation

Determine whether transitional states benefit from subtle animation.

Animation is not required for v0.1.

### 25.7 Profile Menu Scalability

Determine how to handle large profile collections without creating excessively tall menus.

---

## 26. Guiding Principles

1. **The tray is AudioHole's persistent everyday interface.**
2. **The GUI may close without stopping audio.**
3. **The icon communicates overall routing health.**
4. **The menu provides quick access to profiles, holes, and routing controls.**
5. **A duplex hole remains one logical hole.**
6. **Stop Routing and Exit AudioHole are distinct operations.**
7. **Ready means armed and waiting, not stopped or necessarily failed.**
8. **Failed and Recovering profiles remain armed unless explicitly stopped.**
9. **Failed profile switches must preserve working routing where possible.**
10. **Windows toast notifications supplement persistent tray status.**
11. **Ready-to-Running informational notifications should be silent.**
12. **Repeated failures must not produce notification spam.**
13. **The tray, GUI, and optional API must share one authoritative control and status model.**
14. **Tray operations must never interfere with real-time audio processing.**
15. **Tray preferences remain human-readable and manually editable.**

**AudioHole should remain quietly available in the background, communicate meaningful problems when they occur, recover automatically when possible, and give the user immediate control without requiring the main GUI.**
## Personality-aware tray copy

Routine noncritical tray notifications use the authored Normal/Vanilla/UwU variants in `PERSONALITY.md` §16. Keep endpoint names and actionable error content literal. Switching personality changes future notification copy immediately without changing tray actions or routing state.
