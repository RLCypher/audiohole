# AudioHole v0.1 — Graphical User Interface

**Document:** `GUI.md`  
**Status:** Proposed design specification  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `CONFIG_FORMAT.md`, `DEVICE_PROBING.md`, `PROFILES.md`, `TRAY.md`, `API.md`

---

## 1. Purpose

The AudioHole GUI provides a straightforward way to discover audio devices, inspect their capabilities, create routing profiles, configure simplex and duplex connections, and diagnose problems.

The interface must serve two types of interaction:

- **Everyday operation:** Connect devices, activate a profile, and see whether routing works.
- **Technical configuration:** Inspect capabilities, tune formats and buffers, perform advanced probing, and investigate failures.

These should not require two separate applications or modes.

The primary design goal is:

**Connecting two compatible audio endpoints should be as simple as adding a hole, selecting Device A, selecting Device B, and accepting the recommended settings.**

Advanced functionality should remain available without dominating the ordinary workflow.

AudioHole connects arbitrary Windows audio devices as though they were connected by physical audio cables. It does not require users to understand the underlying audio APIs to establish those connections.

---

## 2. Design Principles

1. Simple operations must remain simple.
2. Technical complexity should be available progressively.
3. The GUI must never hide important routing failures.
4. Defaults must be sensible, explainable, and clearly labeled.
5. Device discovery and probing must be independent of profile editing.
6. Settings must be accessible without changing profiles.
7. All technical settings should have explanatory tooltips.
8. Potentially unfavorable configurations should receive visible warnings.
9. Profiles, holes, and devices must expose their editable notes.
10. The GUI must reflect actual routing status, not merely requested routing.
11. Closing the GUI must not stop active audio.
12. All configuration remains human-readable and editable outside the GUI.
13. Recommendation explanations should be visible without requiring a tooltip.
14. The interface should avoid unnecessary terminology that could confuse AudioHole with unrelated audio-routing products.

---

## 3. Main Navigation

The initial GUI should have four primary tabs:

| Tab | Purpose |
|---|---|
| **Profiles** | Create, configure, and activate audio connections |
| **Devices** | Browse, identify, rename, and inspect discovered devices |
| **Scanning** | Discover and probe devices, with live diagnostic output |
| **Settings** | Application behavior, notifications, defaults, and advanced preferences |

A persistent status area should display the current routing state regardless of the selected tab.

### 3.1 Persistent Status Area

The GUI should display:

- Active profile.
- Current routing status.
- Start/Stop control.
- Warning or error indicator when appropriate.

The active profile must remain distinguishable from the profile currently being viewed or edited.

### 3.2 Navigation Independence

Switching between tabs must not:

- Interrupt audio.
- Change the active profile.
- Start device probing.
- Discard unsaved edits.
- Trigger automatic routing changes.

### 3.3 Diagnostics Access

The Scanning tab contains verbose scan output.

Live routing diagnostics may also be accessible from the persistent status area or a dedicated expandable diagnostics panel.

---

## 4. First-Run Introduction

On first launch, AudioHole should present a guided introduction.

The introduction should explain what AudioHole does without requiring the user to understand WASAPI, endpoint enumeration, sample formats, or buffering.

### 4.1 Introduction Goals

The user should finish the introduction with:

- An understanding of simplex and duplex holes.
- Discovered audio devices.
- At least one configured profile, if suitable devices exist.
- A clear understanding of how to start and stop routing.
- Knowledge of where to find device scanning and diagnostics.

### 4.2 Suggested Introduction Flow

**Step 1 — Welcome**

Introduce AudioHole as a tool for connecting arbitrary Windows audio devices, as though they were connected by physical audio cables.

Explain that connections can be one-way (simplex) or two-way (duplex), and that multiple connections can be organized into reusable profiles.

**Step 2 — Discover Devices**

Prompt the user to run an Easy Scan.

Explain that Easy Scan reads Windows-reported device information without deliberately opening disruptive test streams.

**Step 3 — Review Devices**

Display the discovered recording and playback devices.

Allow the user to identify and rename devices where helpful.

**Step 4 — Create a Profile**

Prompt the user to name their first profile.

**Step 5 — Add a Hole**

Explain simplex and duplex connections with practical examples.

A simplex hole could connect a microphone to speakers for an impromptu PA system.

A duplex hole could connect a hardware mixer to a streaming or VR audio workflow, with independent audio paths in both directions.

**Step 6 — Select Endpoints**

Guide the user through selecting source and destination devices.

**Step 7 — Review Recommendations**

Show the recommended format and buffer settings.

Provide short, visible explanations of why the settings were selected.

Explain that defaults are intended to balance compatibility, stability, and latency.

**Step 8 — Activate**

Offer to activate the completed profile.

Explain that AudioHole may remain Ready until required hardware becomes available.

**Step 9 — Finish**

Show where to find the Profiles, Devices, Scanning, and Settings tabs.

### 4.3 Skipping the Introduction

The introduction must be skippable.

Users must not be forced to perform a scan before accessing the application.

### 4.4 Rerunning the Introduction

Settings must include:

**Run Introduction Again**

Rerunning the introduction must not delete existing profiles or reset settings.

### 4.5 Interrupted Introduction

The introduction should tolerate application closure or cancellation without corrupting partially created configuration.

---

## 5. Profiles Tab

The Profiles tab is the primary configuration interface.

It should emphasize the active profile and its logical connections.

### 5.1 Profile Selection

The user should be able to:

- Select a profile.
- Create a profile.
- Duplicate a profile.
- Rename a profile.
- Delete a profile.
- Edit profile notes.
- Activate a profile.
- Stop routing.

### 5.2 Profile Notes

Each profile has an editable Notes field.

Notes should be visible in the profile editor without requiring direct JSON editing.

Long notes may be collapsed or displayed in an expandable area.

Notes must remain independent of routing behavior.

### 5.3 Hole List

The selected profile displays its holes.

Each hole should show:

- Display name.
- Simplex or duplex designation.
- Source and destination.
- Recommended or overridden settings.
- Enabled state.
- Current runtime status, when applicable.
- Warnings or unresolved configuration.

### 5.4 Add Hole

The primary creation control should be:

**+ Add Hole**

Selecting it should create a new hole editor.

The user then chooses Simplex or Duplex.

Simplex may be the initial default.

### 5.5 Hole Configuration

The basic editor should require only:

1. Source device.
2. Destination device.
3. Audio format.
4. Buffer size.

Format and buffer controls should populate automatically with recommendations whenever sufficient information is available.

The user should not need to open advanced settings to complete an ordinary hole.

### 5.6 Duplex Configuration

A duplex hole presents two directional paths:

**Forward**

Source A → Destination B

**Reverse**

Source B → Destination A

Each direction has its own endpoint and format/buffer controls.

AudioHole may offer a convenience action to populate the reverse direction using the corresponding capture and playback endpoints of the selected physical devices.

However, it must not assume that every device has matching capture and playback endpoints.

The user must be able to inspect and change each direction independently.

### 5.7 Hole Notes

Each hole may have editable notes.

These should be available in an expandable section of the hole editor.

---

## 6. Device Selection

Device selection should use searchable dropdown menus.

### 6.1 Source Dropdown

The source selector should list suitable recording/capture endpoints.

### 6.2 Destination Dropdown

The destination selector should list suitable playback/render endpoints.

### 6.3 Device Labels

Dropdown entries should prioritize user-assigned display names.

The Windows-reported name should remain accessible through a tooltip or secondary detail.

### 6.4 Device Availability

Entries should distinguish:

- Available.
- Unavailable.
- Unresolved.
- Ambiguous identity.
- Known but not yet tested.

Unavailable saved devices must remain selectable when editing profiles.

### 6.5 Identical Device Names

Devices with identical names should be distinguishable using additional readable identity information.

The GUI must not rely solely on Windows display names.

### 6.6 Device Details

A device entry may provide a shortcut to its full device record.

The user should not have to leave the profile editor merely to identify a device, but deeper information should remain accessible.

---

## 7. Audio Format Selection

Each directional path should have an audio-format dropdown.

### 7.1 Automatic Recommendation

After the source and destination are selected, AudioHole should calculate a recommended operating format.

The recommendation should consider:

- Experimentally verified capabilities.
- Windows-reported capabilities.
- Shared/exclusive operating mode.
- Sample-rate compatibility.
- Channel compatibility.
- Sample-format compatibility.
- Required conversion.
- Latency implications.
- Known driver limitations.
- Reliability of available evidence.

### 7.2 Recommendation Priority

The general preference should be:

1. A verified, compatible configuration with favorable latency and stability characteristics.
2. A compatible configuration supported by reliable reported capabilities.
3. A conservative fallback when capability evidence is incomplete.

The recommendation must not assume that the highest sample rate or bit depth is automatically the best choice.

### 7.3 Conversion

When source and destination native formats differ, AudioHole may use supported internal conversion.

The GUI should explain when conversion is required.

A recommended operating format may therefore be valid even when the endpoints do not advertise an identical native format.

### 7.4 Format Dropdown

The dropdown should display readable options such as:

- Recommended — 48 kHz, stereo, float32.
- 48 kHz, stereo, 24-bit PCM.
- 44.1 kHz, stereo, 16-bit PCM.

These are illustrative labels, not assumed capabilities of any particular device.

### 7.5 User Override

The user may override the recommendation.

Unsupported combinations must not be silently presented as working configurations.

### 7.6 Evidence Labels

Options should indicate their evidence level where useful:

- Verified.
- Reported.
- Untested.
- Known unsupported.

Known unsupported formats should normally be disabled rather than offered as ordinary selections.

### 7.7 No Reliable Recommendation

If AudioHole lacks sufficient information, it should select a conservative fallback and label it appropriately.

For example:

**Default — Conservative compatibility setting**

It must not label an untested fallback as experimentally verified.

### 7.8 Recalculation

Changing either endpoint should trigger recommendation recalculation.

If the user previously selected a manual override, AudioHole should preserve it when still valid or clearly explain why it no longer applies.

### 7.9 Visible Recommendation Explanation

Every automatically selected audio format must display a concise explanation directly beneath its dropdown.

The user should not need to hover over an icon to discover why AudioHole selected a particular configuration.

Example:

```text id="r2c1f9"
Audio Format                  ⓘ
┌────────────────────────────────────────────┐
│ 48 kHz · Stereo · float32 (Recommended)  ▼ │
└────────────────────────────────────────────┘
✓ Verified on both endpoints.
  No sample-rate conversion required.
```

This example assumes the relevant configuration has actually been verified.

Other possible explanations include:

**Recommended**

"Compatible with both endpoints. Format conversion required."

**Recommended**

"Verified during Advanced Scan. Avoids unnecessary sample-rate conversion."

**Default**

"Conservative compatibility setting. Advanced probing has not been performed."

**User Selected**

"Manual override. The recommended configuration is 48 kHz, stereo, float32."

The actual explanation must reflect the evidence available for the selected devices.

### 7.10 Explanation Requirements

The visible explanation should:

- Be concise.
- State the primary reason for the selection.
- Distinguish verified evidence from reported capabilities.
- Identify important conversion requirements.
- Indicate uncertainty when evidence is incomplete.
- Update when either endpoint changes.
- Update when a manual override is selected.

Detailed technical reasoning should remain accessible through a tooltip or expanded information panel.

### 7.11 No False Confidence

AudioHole must not claim that a format is verified, conversion-free, or stable without supporting evidence.

---

## 8. Buffer Size Selection

Each directional path should have a buffer-size dropdown.

### 8.1 Design Goal

The default should balance low latency against stability.

The smallest technically available buffer is not necessarily the best default.

### 8.2 Recommended Buffer Selection

Where reliable probe data exists, AudioHole should prefer a low-latency buffer configuration with an appropriate stability margin.

This may correspond to the second or third smallest viable option, but must not be implemented as an unconditional positional rule.

### 8.3 Why a Fixed Ranking Is Insufficient

Available buffer sizes may not be evenly spaced.

Some devices expose fixed periods, while others expose ranges or mode-dependent constraints.

A configuration that initializes successfully is not necessarily stable during sustained operation.

### 8.4 Verified Stability

AudioHole should distinguish:

- Buffer accepted during initialization.
- Stream successfully started.
- Stream operated successfully during a test.
- Buffer demonstrated acceptable stability under defined conditions.

A short probe must not be represented as proof of long-term reliability.

### 8.5 Fallback

When no useful stability evidence exists, AudioHole should choose a conservative default based on operating mode and known endpoint constraints.

### 8.6 Buffer Dropdown

The dropdown should present human-readable options, preferably showing time in milliseconds.

Example:

- 3 ms — Low latency, increased dropout risk.
- 6 ms — Recommended.
- 10 ms — Stability-focused.

These values are illustrative; actual choices must come from the audio backend and endpoint constraints.

### 8.7 User Override

Users may choose another supported buffer configuration.

AudioHole should warn when a choice is likely to increase dropouts, latency, or incompatibility.

### 8.8 Dynamic Recommendation

If later probing provides better evidence, AudioHole may offer an improved recommendation.

It must not silently overwrite a user's explicit manual selection.

### 8.9 Visible Buffer Recommendation Explanation

The selected buffer configuration should display a concise explanation directly beneath the dropdown.

Example:

```text id="az9xg4"
Buffer Size                   ⓘ
┌────────────────────────────────────────────┐
│ 6 ms (Recommended)                       ▼ │
└────────────────────────────────────────────┘
✓ Passed available stream tests.
  Provides a margin above the minimum period.
```

This example assumes those tests were actually performed.

Other possible explanations include:

**Recommended**

"Lowest tested configuration with the selected stability margin."

**Default**

"Conservative buffer setting. No stability measurements available."

**User Selected**

"Low-latency override. Smaller buffers may increase audio dropouts."

**User Selected**

"Stability-focused override. Higher buffering increases latency."

### 8.10 Explanation Requirements

The visible explanation should distinguish measured evidence from conservative assumptions.

A buffer that merely initializes successfully must not be described as proven stable.

---

## 9. Defaults and Recommendations

All automatically chosen technical settings should be clearly identified.

### 9.1 Default

Use **Default** when a value is the standard application behavior or conservative fallback.

### 9.2 Recommended

Use **Recommended** when AudioHole has a reasoned preference based on device capabilities, probing, or known operating characteristics.

### 9.3 User Selected

A manual override should remain visibly distinguishable from an automatic recommendation.

### 9.4 Recommendation Presentation

A recommended setting should provide:

- The selected value.
- A Default or Recommended label.
- A short visible explanation.
- Additional technical detail through a tooltip.

### 9.5 Tooltip Explanation

Tooltips should provide more detailed reasoning.

For example:

"Both endpoints accepted this operating configuration during Advanced Scan. The selected sample rate avoids additional sample-rate conversion. This does not guarantee uninterrupted operation under every system load."

The actual explanation must reflect the available evidence.

### 9.6 No False Confidence

AudioHole must not present an unverified configuration as proven stable.

---

## 10. Warnings and Technical Tooltips

Technical controls should provide hover tooltips or equivalent accessible explanations.

### 10.1 Tooltip Coverage

Examples include:

- Sample rate.
- Sample format.
- Channel count.
- Shared mode.
- Exclusive mode.
- Buffer size.
- Latency.
- Resampling.
- Clock drift.
- Failover.
- Device fingerprint.
- Driver information.
- Scan evidence.
- Endpoint availability.

### 10.2 Warning Indicator

Settings likely to produce unfavorable results should display a caution indicator.

The warning should explain the specific concern.

### 10.3 Warning Examples

Potential warnings include:

- Extremely small buffer with limited stability evidence.
- Exclusive mode may prevent other applications from using the device.
- High sample rate may increase processing requirements.
- Format requires resampling.
- Device capability is untested.
- Device identity is ambiguous.
- Selected endpoint is unavailable.
- Driver capabilities have changed since the last probe.

### 10.4 Severity

Warnings should distinguish:

- Informational.
- Caution.
- Error or unsupported configuration.

### 10.5 Accessibility

Warnings must not depend on color alone.

Tooltips should also be accessible through keyboard focus or an equivalent interaction.

### 10.6 Nonblocking Behavior

Cautionary warnings should generally be nonblocking.

Known invalid or unsafe configurations may prevent activation.

### 10.7 Visible Versus Hover Information

Visible recommendation explanations and technical tooltips serve different purposes.

**Visible explanations answer:** Why did AudioHole choose this setting?

**Tooltips answer:** What does this setting mean, and what are its technical implications?

Neither should replace the other.

---

## 11. Advanced Hole Settings

The basic hole editor should remain compact.

Advanced settings should be accessible through an expandable section.

Potential advanced controls include:

- Shared or exclusive operating mode.
- Explicit format selection.
- Channel mapping.
- Conversion preferences.
- Buffering details.
- Latency preference.
- Failover candidates.
- Failure policy.
- Recovery behavior.

Advanced controls must not be required for a typical two-device connection.

---

## 12. Devices Tab

The Devices tab is the primary interface for inspecting and organizing discovered audio endpoints.

### 12.1 Device Categories

Devices should be grouped by direction:

- Recording.
- Playback.

### 12.2 Device List

Each entry should show:

- User-assigned display name.
- Windows-reported name.
- Availability.
- Scan status.
- Relevant warnings.

### 12.3 Device Details

Selecting a device should open a detailed view containing:

- Display name.
- Windows name.
- Editable notes.
- Reported capabilities.
- Probed capabilities.
- Scan timestamps.
- Device fingerprint.
- Driver information.
- Current local binding.
- Relevant diagnostic history.

### 12.4 Device Notes

Each device record contains an editable Notes field.

The GUI must expose it directly.

Notes should be preserved through rescans, driver updates, and device reenumeration.

### 12.5 Renaming Devices

Renaming a device in the GUI must follow the filename synchronization rules in `CONFIG_FORMAT.md`.

Its stable logical ID must remain unchanged.

### 12.6 Missing Devices

Previously discovered devices should remain visible even when disconnected.

The user should be able to inspect their saved capabilities and notes.

### 12.7 Device Identity

Advanced device details should expose readable fingerprint information.

Ambiguous device matches must provide an understandable confirmation workflow.

### 12.8 Driver Information

Driver information should be presented as diagnostic context, not as a device-management interface.

Unavailable driver fields must be clearly labeled.

---

## 13. Scanning Tab

Scanning is independent of profile editing.

The Scanning tab should support device discovery, capability inspection, and experimental probing.

### 13.1 Primary Controls

The tab should provide:

- Easy Scan.
- Advanced Scan.
- Scan selected device.
- Update existing scan data.
- Full rescan.
- Force re-probe, where applicable.
- Cancel current scan.

The exact grouping of controls may be refined during GUI implementation.

### 13.2 Easy Scan

Easy Scan should gather nonintrusive Windows-reported information.

It should not deliberately open experimental audio streams.

### 13.3 Advanced Scan

Advanced Scan may open test streams and evaluate endpoint behavior.

It requires explicit user consent and an explanation of potential disruption.

### 13.4 Advanced Scan Warning

Before starting Advanced Scan, AudioHole should explain that testing may:

- Temporarily occupy audio endpoints.
- Interrupt active audio.
- Produce initialization failures.
- Exercise exclusive-mode capabilities.
- Take longer than Easy Scan.

The user should be able to cancel.

### 13.5 Scan Scope

Users should be able to scan:

- All devices.
- Recording devices.
- Playback devices.
- A selected device.

### 13.6 Scan Progress

The GUI should show:

- Current device.
- Current test or phase.
- Completed tests.
- Remaining work when determinable.
- Overall progress when meaningful.
- Current errors or warnings.

Progress indicators must not invent precise percentages when the workload is not known.

---

## 14. Integrated Verbose Scan Console

The Scanning tab should contain an integrated, scrollable, CLI-style diagnostic console.

This console is a graphical presentation of structured scan events, not a separate command-line process.

### 14.1 Console Goals

The console should make device discovery and probing understandable to technical users.

It should expose useful evidence without requiring external debugging tools.

### 14.2 Example Output

```text id="c0swf7"
[INFO] Easy Scan started
[INFO] Enumerating recording endpoints
[FOUND] RØDECaster Pro II USB Capture
[INFO] Reading endpoint properties
[INFO] Reported channels: 2
[INFO] Reported default format: 48000 Hz
[INFO] Reading driver information
[WARN] Driver date unavailable
[INFO] Device record updated
[INFO] Easy Scan completed
```

This is illustrative output.

Actual logs must reflect real scan results.

### 14.3 Advanced Scan Output

Advanced probing should report:

- Endpoint.
- Operating mode.
- Requested format.
- Initialization result.
- Stream-start result.
- Verification result.
- HRESULT or relevant Windows error code.
- Elapsed test time.
- Reason for failure or inconclusive result.

### 14.4 Console Controls

The console should support:

- Auto-scroll.
- Pause display.
- Clear visible output.
- Copy selected text.
- Copy all visible text.
- Save or export logs.
- Filter by severity.
- Filter by device where practical.

Clearing the visible console must not silently delete persistent diagnostic history.

### 14.5 Structured Events

Console lines should be rendered from structured diagnostic events.

The same events should be usable by logs and future diagnostic tools.

### 14.6 Performance

Console rendering must not block audio processing or scanning workers.

Large volumes of output should be buffered and displayed efficiently.

### 14.7 Scan History

The user should be able to inspect previous scan results.

Scan history should include timestamps and distinguish Easy Scan from Advanced Scan.

---

## 15. Settings Tab

Settings should be independent of profile configuration.

### 15.1 Startup & Activation

AudioHole must support configurable automatic startup and delayed routing activation.

These settings control when AudioHole launches and when it attempts to activate a profile. They must not be confused with device recovery or failover behavior.

**Startup Settings**

| Setting | Options | Default |
|---|---|---|
| Start AudioHole with Windows | On / Off | Off |
| Open main window at startup | On / Off | Off when autostart is enabled |
| Startup profile | Last active / Specific profile | Last active |
| Routing activation | Manual / Immediate / Delayed | Manual |
| Activation delay | Configurable duration | 10 seconds when Delayed is selected |

The activation-delay control should appear only when Delayed is selected.

The GUI should provide convenient presets of 5, 10, 15, 30, and 60 seconds, plus a custom duration. Custom values should be validated against a documented supported range.

**Activation Behavior**

1. The startup delay begins when the AudioHole application starts, not when Windows begins booting.
2. During the countdown, routing remains inactive and the tray displays the pending activation and remaining time.
3. Selecting **Start Routing** during the countdown bypasses the remaining delay and attempts activation immediately.
4. Selecting **Stop Routing** cancels the countdown and disarms the pending activation.
5. Closing the main GUI does not cancel the countdown.
6. When the timer expires, AudioHole attempts to activate the selected startup profile.
7. If required devices are unavailable, a valid profile enters Ready and waits for them.
8. The delay applies once per application launch, not after device disconnections or subsequent recovery attempts.
9. The countdown must not block GUI interaction, tray controls, or application initialization.
10. If no valid startup profile can be resolved, AudioHole must report the problem rather than silently activating an unrelated profile.

**Status Presentation**

During delayed activation, the GUI should display a clear pending-startup indicator, such as:

`Startup scheduled — Activating VR Audio in 15 seconds`

The countdown is a startup substate, not a new canonical routing state. Until activation begins, no audio streams should be reported as running.

**Manual Activation**

When Manual is selected, AudioHole launches without automatically arming a profile. The user must explicitly select Start Routing.

**Immediate Activation**

When Immediate is selected, AudioHole attempts activation as soon as application initialization and required configuration loading are complete.

**Delayed Activation**

When Delayed is selected, AudioHole waits for the configured duration before attempting activation.

This is useful when Windows audio endpoints, USB interfaces, wireless devices, or other supporting software may not be ready immediately after login.

**Recovery Independence**

The startup delay must not apply to:

- Reconnecting a disconnected device.
- Recovering an interrupted stream.
- Switching to a configured fallback device.
- Returning from a fallback to a preferred device.

These operations follow their respective recovery and failover policies.

**Configuration**

Startup preferences must be stored in the human-readable settings defined by `CONFIG_FORMAT.md`. The GUI and tray must use the same activation controller and countdown state.
### 15.1.1 Profile Changes During Delayed Startup

Selecting another profile during the startup countdown preserves the existing deadline and changes only the pending activation target. The GUI updates the displayed target immediately; selection alone does not activate routing or restart the delay. For example: `Startup scheduled — Activating Desktop Audio in 15 seconds`.

Start Routing Now bypasses the remaining delay for the current pending target. Stop Routing or Cancel Startup disarms it. If the pending profile is invalid or deleted, report an error rather than silently activate a different profile.

The GUI distinguishes the profile being edited, the pending activation target, and the profile in an active routing session. It reads pending state from the controller rather than running its own countdown.


### 15.1.2 Personality

Settings → Personality presents exactly one mutually exclusive selector:

- **Normal — Default Audio Engineer** (`default`)
- **Vanilla — Strictly informational** (`vanilla`)
- **UwU — I know what you are** (`uwu`)

Default: Normal. Changing the selection takes effect immediately for newly presented application-authored copy and currently rendered mode-sensitive interface text, without restarting routing. The selector replaces any proposed Vanilla Mode toggle.

Vanilla presents strictly informational language and hides the DFA button. Normal and UwU display DFA in Settings → Advanced. When Vanilla is selected, show a factual note: "Vanilla Mode disables personality-driven messages and hides the DFA button. Audio routing is unchanged." Do not require confirmation.

The DFA button is a placebo: it may acknowledge a click visually but must issue no routing, configuration, scan, repair, or diagnostic commands. Its tooltip in Normal is "Working as intended." Any UwU variant must be authored, not generated by transforming arbitrary text.

Critical warnings, data-integrity issues, safety messages, device names, technical identifiers, logs, and user-authored content must remain clear and unmodified in every mode. Refer to `PERSONALITY.md` for complete guidance.

### 15.2 Notifications

Controls should include:

- Notification level.
- Tray status indication.
- Taskbar status indication.
- Informational notifications.
- Warning and error notifications.

These controls must remain consistent with the canonical settings schema.

### 15.3 Audio Defaults

Potential controls:

- Default operating mode.
- Default latency preference.
- Recommended buffer policy.
- Default failure behavior.

Application defaults should not silently overwrite explicit per-hole preferences.

### 15.4 Scanning

Potential controls:

- Scan history retention.
- Diagnostic verbosity.
- Advanced Scan preferences.
- Probe-result handling.

### 15.5 External Control

If implemented, optional API controls may include:

- Enable external control.
- Selected transport.
- Access permissions.
- Connection status.

External control should be disabled by default unless a later security review changes that decision.

### 15.6 Introduction

Settings must include:

**Run Introduction Again**

### 15.7 Advanced Settings

Rarely used technical settings should be grouped separately.

The ordinary Settings tab should not become a dumping ground for implementation details.

---

## 16. Live Routing Diagnostics

Scanning diagnostics and live routing diagnostics serve different purposes.

### 16.1 Scan Diagnostics

Explain device discovery and capability testing.

### 16.2 Routing Diagnostics

Explain active stream behavior.

Relevant information may include:

- Active profile.
- Hole status.
- Directional path status.
- Resolved endpoints.
- Operating format.
- Buffer configuration.
- Stream errors.
- Recovery attempts.
- Failover transitions.
- Available latency measurements.
- Clock-drift correction status.

### 16.3 Availability

Routing diagnostics should remain accessible while audio is running.

### 16.4 Presentation

A live console or expandable diagnostics panel should use the same structured logging system as scanning.

### 16.5 Export

Users should be able to export relevant diagnostics without exposing unrelated personal information unnecessarily.

---

## 17. Profile and Hole Status

The GUI must use the profile state model established in `PROFILES.md`.

### 17.1 Profile States

- Stopped.
- Ready.
- Starting.
- Running.
- Degraded.
- Recovering.
- Failed.

### 17.2 Ready

Ready means the profile is armed and waiting for required devices or services.

It is not an error.

### 17.3 Degraded

Degraded means some requested paths are operating while others are unavailable or failed.

### 17.4 Failed

Failed means routing cannot currently operate because of an error.

The profile remains armed for recovery unless explicitly stopped.

### 17.5 Hole Status

Each hole should show its own state.

Duplex holes should show the health of both directions.

### 17.6 Desired Versus Actual

The GUI must distinguish configured endpoints from currently resolved or fallback endpoints.

---

## 18. Applying Changes

Editing a running profile must not unexpectedly interrupt audio.

### 18.1 Staged Routing Changes

Changes to endpoints, formats, buffers, and hole enablement should be staged.

### 18.2 Apply

The user selects Apply to commit routing changes.

### 18.3 Revert

The user may discard unapplied changes.

### 18.4 Minimal Disruption

Applying changes should preserve compatible running paths wherever possible.

### 18.5 Disruptive Changes

If applying a change requires interrupting an active stream, AudioHole should communicate that consequence.

### 18.6 Failed Apply

If a change cannot be applied, AudioHole should preserve the previous working configuration where technically possible.

---

## 19. Profile Switching

The GUI must implement the transactional switching behavior specified in `PROFILES.md`.

### 19.1 Default Behavior

Switching to an unavailable destination profile should not replace a functioning profile with silence.

### 19.2 Failed Switch

The GUI should:

- Preserve the previous active profile.
- Explain why the switch failed.
- Show the destination profile as not activated.
- Provide a deliberate option to arm the destination in Ready state.

### 19.3 Explicit Ready Activation

The user may intentionally activate an unavailable profile and wait for its devices.

This action must be clearly distinguished from an ordinary successful switch.

### 19.4 Profile Selection

Viewing or editing a profile must not automatically switch active routing.

---

## 20. Confirmation and Warning Behavior

Confirmations should be used sparingly.

### 20.1 Confirmation Examples

- Delete profile.
- Delete active hole.
- Replace device identity association.
- Run potentially disruptive Advanced Scan.
- Apply a change that requires interrupting active audio.
- Switch profiles when exclusive-mode resources prevent seamless preparation.

### 20.2 Avoid Excessive Dialogs

Routine operations should not require repeated modal confirmation.

### 20.3 Warning Placement

Warnings should appear near the setting or operation they concern.

A warning triangle with a hover explanation is appropriate for technical configuration risks.

### 20.4 Clear Consequences

Warnings should explain what may happen, not merely state that a setting is advanced.

---

## 21. Accessibility and Usability

### 21.1 Keyboard Navigation

Primary controls should be keyboard accessible.

### 21.2 Tooltips

Technical tooltips must be accessible through keyboard focus or equivalent interaction.

### 21.3 Color

Status and warnings must not rely on color alone.

### 21.4 Scaling

The GUI should support ordinary Windows display scaling.

### 21.5 Readability

Technical identifiers should remain readable and copyable.

### 21.6 Device Names

Long device names should not be silently truncated without a way to inspect the complete value.

### 21.7 Consistent Language

Use the same terminology across GUI, configuration files, logs, and API documentation.

---

## 22. Window Lifecycle

The GUI is not the audio engine.

### 22.1 Closing the Window

Closing the main window should hide it to the system tray by default.

Routing continues.

### 22.2 Reopening

The main window can be reopened through the tray.

### 22.3 Explicit Exit

Explicitly exiting AudioHole stops routing and terminates the application.

### 22.4 Minimized Startup

When configured, AudioHole may start minimized without displaying the main GUI.

### 22.5 Tray Responsibilities

Detailed tray-menu behavior, icon states, tooltips, and Windows toast behavior belong in `TRAY.md`.

The GUI must consume the same authoritative status model.

---

## 23. Configuration Transparency

The GUI must operate on the human-readable configuration formats defined in `CONFIG_FORMAT.md`.

### 23.1 No Hidden Configuration

User-facing settings must not exist only in opaque internal state.

### 23.2 Manual Editing

External edits should be detected and validated where practical.

### 23.3 Notes Preservation

Device, profile, and hole notes must survive ordinary editing, rescanning, and configuration migration.

### 23.4 Filename Synchronization

Renaming a device or profile through the GUI must follow the established filename synchronization rules.

### 23.5 Error Handling

Invalid JSON or conflicting edits must be reported rather than silently overwritten.

---

## 24. GUI Acceptance Tests

### 24.1 First Launch

1. Launch AudioHole without existing configuration.
2. Verify the introduction appears.
3. Skip the introduction.
4. Verify the application remains usable.
5. Rerun the introduction from Settings.

### 24.2 Guided Setup

1. Run the introduction.
2. Perform Easy Scan.
3. Review discovered devices.
4. Create a profile.
5. Add a simplex hole.
6. Select source and destination.
7. Accept recommended settings.
8. Activate the profile.

### 24.3 Simplex Creation

1. Select + Add Hole.
2. Choose Simplex.
3. Select Device A.
4. Select Device B.
5. Verify format recommendation appears.
6. Verify buffer recommendation appears.
7. Verify both recommendations have visible explanations.
8. Save and activate.

### 24.4 Duplex Creation

1. Add a duplex hole.
2. Configure the forward path.
3. Configure the reverse path.
4. Verify each direction has independent settings.
5. Activate.
6. Verify both directions are represented in status.

### 24.5 Format Recommendation

1. Select two devices with known compatible capabilities.
2. Verify a recommended format is selected.
3. Verify the visible explanation reflects actual evidence.
4. Inspect the technical tooltip.
5. Override the format.
6. Verify the manual choice is identified.
7. Verify the recommendation remains discoverable.

### 24.6 Conservative Fallback

1. Select devices with incomplete capability evidence.
2. Verify AudioHole chooses a conservative fallback.
3. Verify it is labeled Default rather than experimentally verified.
4. Verify the visible explanation identifies the uncertainty.
5. Verify the user can inspect further technical details.

### 24.7 Buffer Recommendation

1. Select devices with useful probe data.
2. Verify the recommended buffer reflects stability and latency considerations.
3. Verify the visible explanation states the basis for selection.
4. Select a more aggressive buffer.
5. Verify a relevant caution appears when warranted.
6. Inspect the technical tooltip.

### 24.8 Device Notes

1. Select a device.
2. Edit its notes.
3. Save.
4. Rescan the device.
5. Verify notes remain unchanged.

### 24.9 Profile Notes

1. Select a profile.
2. Edit its notes.
3. Save.
4. Rename the profile.
5. Verify notes remain unchanged.

### 24.10 Scan Console

1. Open Scanning.
2. Start Easy Scan.
3. Verify live console output.
4. Filter output by severity.
5. Copy output.
6. Export logs.
7. Verify persistent diagnostic history remains intact.

### 24.11 Advanced Scan

1. Select Advanced Scan.
2. Verify the disruption warning appears.
3. Confirm.
4. Verify progress and verbose output.
5. Cancel.
6. Verify partial results and cancellation are reported accurately.

### 24.12 Ready State

1. Activate a valid profile with an unavailable device.
2. Verify Ready is displayed.
3. Connect the device.
4. Verify the profile transitions to Running.
5. Verify the GUI updates without reopening.

### 24.13 Failed Switch

1. Activate a working profile.
2. Attempt to switch to an unavailable profile.
3. Verify the working profile remains active.
4. Verify the GUI explains the failure.
5. Verify the user can deliberately arm the destination.

### 24.14 Close to Tray

1. Activate routing.
2. Close the main window.
3. Verify audio continues.
4. Reopen the GUI.
5. Verify status remains accurate.

### 24.15 Manual Configuration Editing

1. Save a profile.
2. Modify its JSON externally.
3. Return to the GUI.
4. Verify the change is detected.
5. Verify invalid changes are not silently applied.
6. Verify unsaved GUI edits are not silently overwritten.

### 24.16 Recommendation Explanation Accuracy

1. Select devices with verified compatible formats.
2. Verify the explanation identifies verified compatibility.
3. Select devices with reported but unverified capabilities.
4. Verify the explanation no longer claims experimental verification.
5. Select a configuration requiring conversion.
6. Verify the explanation identifies conversion.
7. Change the source or destination.
8. Verify the explanation updates accordingly.

---

## 25. Open Design Decisions

### 25.1 GUI Framework

Qt, Dear ImGui, wxWidgets, and other suitable Windows desktop frameworks remain under consideration.

The choice should account for:

- Native Windows behavior.
- Tray integration.
- Accessibility.
- High-DPI support.
- Licensing.
- Development complexity.
- Long-term maintenance.

### 25.2 Exact Buffer Recommendation Algorithm

The algorithm should use verified stability evidence where available and conservative defaults otherwise.

It must not assume the second or third smallest buffer is always appropriate.

### 25.3 Format Recommendation Scoring

The exact priority among verified compatibility, conversion cost, latency, channel requirements, and sample rate should be established during audio-backend design.

### 25.4 Duplex Convenience Selection

Determine how best to suggest corresponding capture and playback endpoints without assuming physical devices expose symmetric interfaces.

### 25.5 Routing Diagnostics Placement

Determine whether live routing diagnostics should use an expandable panel, a secondary view, or both.

### 25.6 Advanced Scan Presentation

Determine how much probe detail belongs in the console versus structured result tables.

These choices should be resolved without compromising the core interaction model.

---

## 26. Guiding Principles

1. **Adding a hole should be fast and understandable.**
2. **Source and destination selection are the central interaction.**
3. **Recommended formats and buffers should populate automatically.**
4. **Recommendations must be evidence-based and explainable.**
5. **A concise explanation of each recommendation must be visible without hovering.**
6. **Conservative defaults are preferable to unsupported confidence.**
7. **Users may override supported technical settings.**
8. **Risky settings receive contextual warnings and tooltips.**
9. **Technical controls should explain themselves.**
10. **Devices, Scanning, and Settings remain independent of profile editing.**
11. **Scanning includes integrated, verbose, human-readable diagnostics.**
12. **First-time users receive an optional guided introduction.**
13. **The introduction can be rerun without resetting configuration.**
14. **Device, profile, and hole notes are first-class editable fields.**
15. **The GUI distinguishes desired routing from actual routing.**
16. **Closing the GUI does not stop AudioHole.**
17. **All persistent configuration remains transparent and manually editable.**
18. **AudioHole terminology should describe its actual functionality without implying association with unrelated audio-routing products.**

**AudioHole should make connecting audio devices feel like plugging in a physical cable. Everything more complicated should be available when needed, explained when encountered, and otherwise stay out of the way.**
---

## Temporary Signal Activity Monitoring (v0.1)

The hole setup/troubleshooting interface shall offer an on-demand **Show Signal Activity** control. It is off by default and automatically turns off when the panel closes. This is a temporary source-side signal-presence indicator, **not** a mixer, calibrated level meter, recording facility, or continuous dashboard.

For each active directional path (forward and reverse independently in duplex holes), show the source endpoint name and a simple responsive activity indicator. A small flickering bar is acceptable; calibrated dBFS values, RMS, peak hold, channel-by-channel metering, and adjustable scales are not required. The indicator answers only whether non-silent samples are arriving at AudioHole from that source.

The UI must distinguish **Signal detected**, **No signal detected**, and **Unavailable/not monitoring** as appropriate. No signal detected is not proof that a device is disconnected, and detected input does not prove audible destination output. Device connection and routing health are displayed separately. Never imply otherwise in any personality mode.

Monitoring must not initiate an additional capture session or change routing, gain, formats, buffers, or latency. No samples are saved or logged. A modest UI update rate (approximately 10–15 Hz) is sufficient.


---

## Test Beep in Hole Setup (v0.1)

Beside the temporary **Show Signal Activity** control, provide a **Test Beep** button associated with a clearly identified **destination playback endpoint**. The UI must show the exact target name and, for duplex holes, the selected forward/reverse destination before the user clicks. Do not present a single ambiguous beep button for multiple outputs.

On click, play one short, quiet test beep (default approximately 1 kHz, -30 dBFS, one second, with brief fades). Show **Playing test beep…** while active and allow immediate **Stop Test Beep**. The button must not play continuously, automatically repeat, or alter the saved profile, normal routing, or device volume. A beep request targeting a capture-only or unavailable endpoint must fail clearly, not redirect elsewhere.

If an endpoint is already engaged by a routing session, use a safe supported playback/injection path or report that testing is unavailable while the route is active. Do not steal exclusive access or interrupt normal program audio without explicit user consent. When routing is stopped, a short-lived playback session may be used if the endpoint supports it. Cancel on Stop Routing, profile switch, endpoint disappearance, panel closure, or application exit.

This is a **destination audibility check**, distinct from the passive source signal-presence indicator. A heard beep confirms the selected playback endpoint can emit the test signal under the test conditions; it does not prove end-to-end routing. All personality modes may vary nonessential surrounding copy, but the target name, active status, and warnings must remain unambiguous.

## Personality message coverage

All common main-window, setup, diagnostics, profile, settings, and Test Beep user-facing messages must use the authored mode variants specified in `PERSONALITY.md` §16. UwU Mode must be conspicuous in routine workflows, not confined to its Settings label. Technical controls, endpoint names, warning content, and accessibility labels remain intelligible and unmodified.
