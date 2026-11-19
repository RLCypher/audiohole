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

The GUI has five primary tabs:

| Tab | Purpose |
|---|---|
| **Profiles** | Create, configure, and activate audio connections |
| **Plug Board** | Assign applications to playback sockets and manage routing boards |
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

Show where to find the Profiles, Plug Board, Devices, Scanning, and Settings tabs.

### 4.3 Skipping the Introduction

The introduction must be skippable.

Users must not be forced to perform a scan before accessing the application.

### 4.4 Rerunning the Introduction

Settings must include:

**Replay Tutorial**

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

First determine eligibility: honor explicit requirements and technical constraints; experimental capability use requires the permission described in `DEVICE_PROBING.md` Section 8.5. Consent to intrusive testing and permission to use experimental results are distinct.

Then rank eligible configurations using applicable evidence of compatibility, verified stability, fidelity, latency, and unnecessary processing. A reported configuration may also have runtime verification; these are not mutually exclusive categories. A verified experimental result does not bypass permission or automatically outrank a reported configuration. Use a clearly labeled conservative default when evidence is incomplete. Exact scoring remains open under D-035 and Section 25.3.

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
  No nominal-rate conversion required; clock compensation may still be needed.
```

This example assumes the relevant configuration has actually been verified.

Other possible explanations include:

**Recommended**

"Compatible with both endpoints. Format conversion required."

**Recommended**

"Verified under the recorded Advanced Scan conditions. Avoids nominal-rate conversion; independent clocks may still require compensation."

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

### Owner-authorized endpoint and format polish

Profile endpoint selectors are selection-only dropdowns. Hidden or missing saved
bindings remain visible without becoming selectable new bindings. The format
preset list includes Manual; choosing it retains typed values. Partial format
overrides display Manual. Choosing Automatic clears the visible format overrides
without changing timing, routing or an explicit channel map.

The GUI assesses independent endpoint formats against the selected backend.
A matching mono capture/stereo playback pair is a bridge candidate labeled
`1 ch (dual mono)` while retaining native capture 1 ch and playback 2 ch details.
Reported candidates are advisory, never saved intent or proof of running audio.
Explicit channel maps stay authoritative. Remaining unknown format differences
or unsupported backend representations receive specific explanations.

Single-line editors and their steppers match the themed dropdown height with
DPI-aware padding. Device Notes defaults to three complete text lines plus
padding. Light device rows use consistent selection/focus colors, preserving
keyboard focus and Windows high contrast.

Candidate matching compares sample rate, encoding, PCM precision and supported
channel counts, rather than requiring identical Windows descriptors. Channel
masks, native descriptor tags, GUID spelling, alignment and byte-rate metadata
remain endpoint details. Matching mono capture/stereo playback pairs label only
the mono endpoint `1 ch (dual mono)`; the playback card retains its stereo
candidate label. Explicit requirements and channel maps remain authoritative.

Text values and hints are vertically centered in the unified single-line fields,
including advanced tuning, device search/name, technical diagnostics search and
the Settings delay, animation-loop and DFA editors. Their native single-line
editing, caret, selection, paste and Enter behavior is retained. The DFA slider
sits to the right of the numeric editor and stepper on the same row.
Empty personality notices and local DFA feedback are hidden so they reserve no
blank rows; real notices remain visible. The main window title is AudioHole,
with the build identifier available in Update and package metadata.

Settings places Update beside Restart AudioHole (Restart Tutorial in a lesson).
Update opens an informational dialog with the current product version, build ID,
clickable AudioHole GitHub link and Close action. It follows the current app
theme and performs no update checks, downloads, restarts or configuration edits.

Confirmed Skip/End tutorial actions always restart into working mode, including
when tutorial edits are unsaved or the completion marker cannot be written. A
marker failure is reported and can cause the tutorial to return on a later
ordinary launch; it does not cancel the already-confirmed restart.

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

Settings → Personality presents one mutually exclusive selector, including these standard IDs and valid user-authored catalogs:

- **Norm** (`default`)
- **Vanilla — Strictly informational** (`vanilla`)
- **UwU** (`uwu`)

Default: Norm. Changing the selection takes effect immediately for newly presented application-authored copy and currently rendered mode-sensitive interface text, without restarting routing. The selector replaces any proposed Vanilla Mode toggle.

Vanilla presents strictly informational language and hides all DFA controls. Non-Vanilla personalities display DFA in Settings → Advanced. When Vanilla is selected, show a factual note: "Vanilla Mode disables personality-driven messages and hides all DFA controls. Audio routing is unchanged." Do not require confirmation.

The DFA button is a placebo: it may acknowledge a click visually but must issue no routing, configuration, scan, repair, or diagnostic commands. Its tooltip in Norm is "Working as intended." Any UwU variant must be authored, not generated by transforming arbitrary text.

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

**Replay Tutorial**

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
- Stopping.

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

---

## Reconciled interface contracts and open policy

D-019's eight canonical states include Stopping; resolving is internal to Starting. Capacity displays count enabled holes in the armed plan, including those waiting/recovering, while actual operating paths and measured performance remain separately visible. Eight holes is not a hardware guarantee.

wxWidgets is the leading candidate only. The D-031 prototype must demonstrate the five tabs, searchable selectors, recommendations, diagnostics, accessibility/DPI behavior, tray integration, and authored Norm/Vanilla/UwU modes before final selection.

A direction's single format/buffer control does not establish identical effective capture and render parameters. Show actual conversion/compensation and evidence limitations; exact endpoint-specific controls depend on O-004. Nominal-rate compatibility does not prove conversion-free or bit-perfect operation. O-005 leaves final direct-transfer eligibility, instability thresholds, compensation algorithms, and public controls undecided. Automatic is the proposed prototype default with proactive compensation permitted for evaluation; strict preservation remains internal/experimental. Do not expose an invented selector or silently promise unchanged samples.

O-001 leaves fallback health aggregation open; show the actual fallback regardless. O-002 leaves switch-preparation thresholds open while preserving working-routing safeguards. O-003 leaves nondefault close/tray preference behavior open; keep established defaults and explicit Stop/Exit. O-006 leaves exact portable-root selection open. Recommendations in the decision register do not authorize interface changes.

Pending worker results use generation-based stale-result rejection. Stop takes precedence over pending recovery, scans, and startup. Driver-call cancellation is not universally immediate. Test Beep cancellation stops further sample production promptly, but queued endpoint audio may remain briefly audible. Preserve exact destination selection and reject unsafe active-route conflicts rather than introducing unapproved mixing. Activity observation remains passive, temporary, source-only, and off by default.

All established personality coverage and safety/accessibility boundaries remain unchanged; no runtime text rewriting or operational difference among modes is introduced.

### Provisional synchronization presentation

Keep ordinary operation straightforward: Automatic is the proposed prototype default, not a new required public selector. Strict preservation is internal diagnostic/experimental work; public exposure requires later review. Do not add a third public mode for frame corrections or a production preference schema.

Advanced diagnostics distinguish selected preference, actual processing, observed synchronization health, and endpoint shared/exclusive mode. Present meaningful mismatch/confidence, buffer occupancy/trend, effective ratio, transitions/reasons, discontinuities/recovery, and latency evidence without exposing tuning internals to ordinary users. A route may currently copy unchanged under Automatic; that observation is not an end-to-end bit-perfect guarantee. Keep automatic-setting explanations concise, visible, and grounded in applicable evidence. Personality modes continue to alter presentation only. See `DECISIONS.md` O-005; no new controls or payloads are specified here.


### Controlled personality commentary acceptance

Apply `PERSONALITY.md` §16.6 in the existing presentation-message catalog. Norm
and UwU may vary deliberately authored optional commentary for common
noncritical event occurrences; Vanilla and all technical facts, status, warnings,
control labels, accessibility text, and recovery instructions stay deterministic.
No generated text, runtime UwU filter, or external content retrieval is added.

GUI/controller-facing tests must cover seeded/injected reproducibility,
no immediate repeats when alternatives exist, single/missing-entry fallback,
and optional weights without mandatory weighting on every event. Verify Vanilla
remains deterministic and strictly informational. Repeated
polls, telemetry updates, and unchanged refreshes retain the selected commentary
rather than emitting new flavor. Assert that variation changes no controller
commands, routing behavior, configuration, diagnostic records, or persistent
state. The DFA pool may be larger while every click remains presentation-only
and Vanilla still hides it. Manual acceptance includes checking these behaviors
while switching modes, refreshing views, and replaying simulated scenarios.

### Owner-approved presentation testing refinement (Milestone 0.1 progress)

The owner credits these GUI testing refinements as Milestone 0.1 progress on the
existing GUI evaluation branch. They extend the authorized GUI scope without
beginning Milestone 1 audio routing or waiving native Windows acceptance gates.

- Optional personalities are editable UTF-8 JSON files; Vanilla is compiled in.
  Settings supports explicit Reload and custom-file loading. Explain whole-file
  fallback and per-event fallback as specified in PERSONALITY.md §17. File reads
  never occur on polling or audio callbacks. Existing session edits remain intact.
- The Advanced button reads DFA, with a 0.00 to 11.00 slider and two-decimal
  exact entry. Apply the approved special-value responses on settlement; no flavor
  per drag tick. Hide the complete control group in Vanilla. All effects are local
  and transient, including across repeated clicks and personality switches.
- Scan lifecycle callouts use the same top technical/commentary area as other
  distinct events. Factual progress can update without commentary reselection.
- Distinguish active/inactive navigation with stronger backgrounds, bold selected
  text and an explicit selected marker, not color alone. Retain keyboard focus,
  arrow/Home/End navigation and Ctrl+Tab page cycling.
- Offer Dark, Light, and Follow System. Follow System is now the evaluation and
  final-build default (superseding the earlier Dark testing default). Changes preserve controller state and
  editor contents. Windows high-contrast settings override the application palette.

The current wxWidgets 3.2.6 implementation applies live application-control palettes
and optional supported title-bar coloring. Windows-owned dialogs and menus retain
system styling; high-DPI, native subcontrol contrast, focus cues, and high-contrast
behavior require manual Windows inspection. No runtime toolkit upgrade or production
preference-persistence schema is established. Evaluation preferences remain session-only.

### Owner-approved production presentation preview

For this testing iteration the owner requested production-style wording throughout
controls, status, diagnostics, and authored personality commentary. Replace repeated
simulation/development notes with a neutral startup notice on every interactive
launch. That notice must explicitly cover simulated devices, capabilities, routing,
scans, monitoring/recovery, silent Test Beep, and session-only edits/preferences.
It must state that no real audio/device access occurs and displayed state is not
hardware evidence. The separate Replay Tutorial action currently shows a tutorial
placeholder, not this notice. Package metadata and engineering
documentation continue to identify the simulator; unavailable measurements remain
unavailable. This presentation amendment does not authorize real audio or persistence.

Follow System is now the initial appearance selection. All action buttons have
explanatory hover help and equivalent F1 help; preserve native keyboard operation,
focus indicators, disabled states, and high contrast. Application-painted dark hot
states use a dark blue-gray surface and readable foreground. State is preceded by
a small color-coded dot: Running green, Failed red, Degraded amber, Ready blue,
transitions yellow, Stopped gray. Always retain the canonical textual state beside it;
Windows high contrast may replace the dot color with a system color.

DFA remains operationally inert and hidden in Vanilla. Its range is now 0.00–11.00;
0.00 uses “Done fucking around.” (with an authored personality counterpart). Keep the other thresholds and 23 ordinary
button responses per supplied personality. Parse the visible numeric entry on Enter,
set the slider and numeric value together, and trigger at most one threshold response
per settled change. Do not display persistent placebo/developer explanations beside
these controls. Keep their documented meaning and ownership in engineering material.

Changed, settled DFA values without a dedicated threshold response now acknowledge
through the existing button-response pool. Threshold pools take precedence; dragging
still updates the value without repeated commentary, and duplicate settlement events
remain suppressed. This owner-approved correction changes presentation only.

### Minor navigation and tutorial presentation correction

Selected navigation labels use brackets around the page name, such as `[Profiles]`,
with the existing bold/color/native selected state; inactive labels stay unbracketed.
Settings uses **Replay Tutorial**, independently of the startup testing disclosure.
Until the tutorial is implemented, this action shows a warning dialog reading:
“Tutorial not yet implemented, please enjoy this placeholder message.” It changes
no session/controller state. The simulation disclosure still appears on each launch.

### Editing and diagnostics acknowledgments

Unchecking Duplex acknowledges simplex selection without applying editor changes.
Save Device acknowledges only a successful update. Testing Level and Scope changes
acknowledge selection for the next scan without starting or changing an active scan.
Copy/Export Visible Diagnostics acknowledge only successful clipboard/file completion;
cancelled dialogs and failed writes must not display a success acknowledgment.

Scanning / Diagnostics includes **Clear diagnostics log**. This clears all retained
in-memory session events, including entries hidden by current filters, and refreshes
the console immediately. Keep filters, routing, profiles, device data, scan progress,
startup, monitoring and Test Beep state intact. Do not delete exported files or add
a replacement log entry. Future events continue to appear normally; unchanged
refreshes do not restore cleared entries. A concise technical acknowledgment and
eligible authored commentary confirm the action. Tooltips explain the clear scope.

### Owner-approved unified logo and agent placeholder

Use the immutable `assets/branding/audiohole-logo.png` in an approximately 128×128
logical-pixel upper-right `AgentDisplay`. Preserve aspect ratio and alpha; cache
scaled images, keep commentary to the left, and retain current window dimensions.
All personalities display the logo for now. The component can replace its image
later; animation, sprites, moods and speech bubbles are not implemented. See
[branding architecture](../../assets/branding/README.md) for resource/ownership contracts.

### DFA precision and validation refinement

The exact-value field accepts more than two fractional digits and normalizes on
Enter, focus loss or explicit stepping. A text-and-stepper control replaces the
native floating-point spin editor so native precision limits/clamping cannot bypass
raw range validation. Keep the 0..11 slider in hundredths; retain Up/Down and 0.01
stepper behavior. All valid stored/displayed values use two places; invalid edits
restore the previous valid value. Match the expanded authored catalog only after
normalization. See PERSONALITY.md for exact-decimal and out-of-range semantics.
When an operational warning occupies the header, retain it and show DFA feedback
locally in Settings; do not hide or replace the warning with an acknowledgment.
Vanilla hides the field, stepper, slider and local feedback and processes no DFA input.

### Owner-authorized visual refinement iteration

The supplied dark reference guides the native header, mint navigation, rounded
surfaces, capture/playback accents and status footer. The existing bindings and
simulated controller remain the functional authority. A static supplied avatar
occupies the replaceable client-area display (Vanilla retains its logo), with bounded
scrolling commentary beside it. No personality-module functionality is added.
See assets/presentation/README.md for actual source dimensions, compact layout,
truthful selected-configuration/count semantics and native validation limitations.

### GUI and personality integration preparation

The next milestone's inspection findings, implemented control refinements and
remaining dependencies are recorded in
[GUI_INTEGRATION_PREPARATION.md](GUI_INTEGRATION_PREPARATION.md).
The current application remains preview-only and session-only. Test Mode,
production persistence, package migration and avatar expression validation are
not yet implemented. Avatar playback remains outside this milestone.


## Shared avatar engine integration

[AVATAR_INTEGRATION.md](AVATAR_INTEGRATION.md) documents the integrated PR6 Session,
owned package migration, expression/baseline/priority hooks, timers, reduced motion,
shutdown and validation gates. Earlier “not integrated yet” notes describe prior
milestones and are superseded for avatar presentation. Audio routing remains the
existing Preview simulation or Test configuration-only backend.

Avatar reactions retain their final pose for two additional seconds before
returning to the current baseline. The saved Personality setting **Hold expressions
between events** (off by default) instead retains the final pose until a new event;
it still yields to inactivity sleep and operational changes. After two minutes
without application input or new reactions/operational changes, a healthy idle avatar sleeps; healthy routing
continues. Input, window restoration and new reactions wake it. Scans, transitions
and operational warnings/errors prevent sleep and can interrupt reaction holds.

While sleeping, the response area displays `zzz....` with no technical heading.
Norm/Horu may show a brief owned dream comment after a random 45–90 second quiet
interval, then return to `zzz....` after six seconds. Status and packages without
a dream pool remain quiet. This cosmetic overlay neither replaces the retained
awake response nor requests an avatar reaction or controller action. Input and
warnings remove it immediately; no dream notifications or catch-up bursts occur.

## Profile workspace polish

Save, Apply and Revert occupy a permanent action bar above both Profiles editor
panes. Save is visually primary and supports Ctrl+S while Profiles is open; the bar
identifies the edited profile and any unsaved changes. Save retains existing
configuration behavior; Apply remains a separate routing action and is unavailable
in Test Mode. Editing or scrolling cannot hide these actions.

The left column has a draggable width divider and separate native scrollable
Profiles, Notes and Holes fields, with draggable height dividers between them.
Resizing changes only session presentation state; the routing detail pane
continues to scroll independently. Inline profile/hole name fields and repeated
management buttons are removed. Each list has compact **+** and **−** controls with hover
help and descriptive accessibility names. The minus action shares the context-menu
delete command and default-No confirmation. Right-click or Shift+F10/Menu opens
Rename, Duplicate and Delete for the targeted item. Merely opening/cancelling a
menu preserves edits and restores the editor's selection. Acting on a different
profile retains the existing unsaved-navigation confirmation.

Renaming stages the display-name change for Save without changing logical IDs.
Hole duplication retains directional configuration with a fresh ID. Profile
duplication copies a saved profile with fresh profile/hole IDs; deletion retains
confirmation and active/pending-profile protection. Holes show readable names and
simplex/duplex status; endpoint details remain in the routing editor.

The initial window prefers a 1000-DIP square within the monitor's work area.
Shorter displays retain the fixed header's required width and use a shorter
window. The minimum height is 680 DIP; this is a starting-size policy, not an
aspect-ratio lock or persisted geometry. Native smoke covers field overflow,
pinned actions at compact/square/wide sizes, context targeting, identity-preserving
rename/duplication and Ctrl+S Test Mode persistence. Visual/DPI/assistive-technology
review still requires Windows.

Local verification for this refinement: 118/118 portable tests pass in Debug and
Release, including sleep timing/identity/fallback tests; the Windows GUI and model
test cross-builds pass. Native smoke assertions for the new profile layout and
context actions are compiled but have not been executed in this Linux workspace.

Windows Debug CI at `5e900a1` subsequently compiled and passed 126/127 tests. The
remaining GUI smoke assertion incorrectly measured rename's revision from before
hole navigation, which stops passive observation and advances the revision. The
check now measures rename independently and separately verifies that navigation
preserves routing and saved profiles. Its native rerun remains pending.

## Owner-authorized current GUI polish

See [GUI_REVISION_NOTES.md](GUI_REVISION_NOTES.md) for the implemented active backlog:
name-based numbered scan progress and elapsed time, unlimited overall Deep budget
at zero, confirmed scanner-data clearing, theme-matched native scrollbars/controls,
readable dark selected rows, equal Deep limit fields, bottom-aligned cyan/pink
wordmark, type-colored device cards, Material heading icons, reaction repeat controls
and local validation/export error recovery. Actual operational warnings retain
priority. Native visual/DPI/high-contrast assessment remains necessary; older
Windows theme capabilities have a native fallback. Animation pruning is deferred.

The subsequent owner-authorized follow-up makes the scan-status height draggable,
starting at three lines and retaining the chosen height across updates. Full
details remain scrollable. Shared form helpers align horizontal row contents with
consistent DPI-scaled padding. Settings presents Appearance/Tray and Startup in
two columns, followed by Personality/Animation, Advanced, then Preview Build
Environment. Reflow and vertical scrolling keep the same window size; these
presentation adjustments do not alter saved preferences or routing behavior.

## Owner-authorized Automatic / Advanced Tuning refinement

Profiles presents hole identity/notes, both directional Source/Destination cards,
and concise evidence-grounded Automatic summaries first. New holes have **Enable
Advanced Tuning** unchecked. The checkbox reveals one panel containing each
participating direction's format, buffer/Apply, operating mode and unavailable
mapping/fallback controls. There is no extra directional disclosure. Each supported
parameter offers Automatic. Turning tuning off retains manual values but excludes
them from effective policy; turning it on stages eligibility and never starts or
restarts routing. Save / Apply remain explicit. Inactive overrides are identified;
requested overrides and confirmed Preview/engine values are labelled separately.
Recommendations remain visible outside the panel; matching nominal rates do not
establish route verification. Missing evidence stays unverified/unavailable.

**Signal Testing & Monitoring** replaces Setup Tools. Its ordinary disclosure is
initially collapsed and uses no top-level resize. Expanded groups are Source
Activity (direction, observation, clear status), Test Beep (explicit destination,
fixed-width start/cancel, status), and Performance Diagnostics (unavailable values
when no measurements exist). Expanding/collapsing changes presentation only; ongoing
source observation and Test Beep persist and are indicated in the collapsed header.
Reserve the wider beep caption before the initially hidden pane's first layout;
start, cancel, completion and disclosure changes retain the same button width.
Leaving Profiles, changing the observed hole/direction, hiding/closing the window,
Stop and shutdown retain existing cancellation rules. Clearing source observation
is an explicit action and does not cancel a separate beep. No real audio operation
is added. Collapsing this disclosure is not closing the observation workspace.

Keyboard focus returns to the checkbox/disclosure before a focused child is hidden.
Native tab order follows basic routing, policy, tuning, then testing. Shared theme,
DPI sizing and scrolling are retained. Settled policy actions select one authored
reaction after 650 ms of quiet, not on telemetry/repaint. Navigation/save cancels
pending reactions. Warnings retain priority. A manual-applied reaction requires a
confirmed simulated application; merely enabling tuning, saving Test configuration,
or requesting all-Automatic parameters cannot generate it.


Advanced Tuning includes a per-hole **Routing Engine** dropdown, initially offering
only **Automatic**. The request is retained in Test profile JSON. Opening/changing
this control does not initialize, select an implementation, benchmark or start
routing. Future engine IDs are preserved as unavailable saved requests, not
silently substituted; Automatic policy ignores them while retaining them.


### Direction information and data-folder access refinement

Both simplex and duplex directions use a defined eight-line, read-only wrapped
information viewport for their policy summary and evidence explanation. Longer
text scrolls and can be copied; changing endpoints must not clip or overlap later
controls. Viewport height stays stable, while actual endpoint-detail changes
relayout the editor and its virtual extent immediately. Unchanged polling does
not reset text/scroll position or repeatedly relayout. Advanced Tuning is not a
prerequisite for correct initial layout.

Visible duplex language uses **Forward** and **Return** throughout direction and
tuning headings, monitoring/beep choices, simulated failure controls and shared
tray/taskbar status. Existing `reverse` model identifiers and JSON keys retain
compatibility; no data migration or routing change accompanies the wording.

Devices offers **Open Devices Folder**, targeting the active Test data root's
`devices` directory. Profiles offers **Open Profiles Folder**, targeting the folder
containing the shared Test `configuration.json`, not a new storage format. A click
creates a missing directory and asks Windows to open it; errors stay local and
no draft is saved or scan started. Both controls are disabled in session-only
Preview Mode, with explanatory hover help. Profiles and Holes show **−** next to
**+**, sharing confirmed deletion with keyboard/context actions. Active/pending
profile guards and staged hole removal remain unchanged.


## November 10, 2026 — owner-authorized profile builder and GUI increment

The owner authorized implementation of the pending GUI refinements, named
per-profile files and partial routing preferences after review with the audio
engine agent. [PROFILE_BUILDER.md](PROFILE_BUILDER.md) specifies the implemented
Test v4 contract, migration/backups, channel indexes/default mapping, independent
endpoint preferences, Shared-first Automatic policy and runtime handoff. It
supersedes older Test v3 persistence and advanced-control visibility descriptions.
The retained override-enable policy is distinct from showing its controls.
Production backend, conversion and synchronization algorithms remain unselected.

GUI refinements include bottom three-line Device/Profile Notes, Application & Tray
with opt-in current-user Windows startup and minimized launch, renamed Routing
Activation controls with inactive delay shown as zero, tutorial confirmation with
placeholder-only continuation, Windows-themed message dialogs, Low motion using
the first animation frame, viewport-width device names, immediate hole status
dots, Material chevrons, Enable return channel, two-line routine status summaries
(full copy in tooltips; warnings stay complete), normal monitor selector height,
and powered-on-device guidance before Deep scans. Positive not-present metadata
failures also recognize SPAPI no-such-device-instance codes E000020B/800F020B.


## Current development profile contract

[PROFILE_BUILDER.md](PROFILE_BUILDER.md) supersedes the earlier Test v4 rules:
individual `audiohole-profile` records use version 1; aggregate
`audiohole-test-configuration` transfer payloads use version 5. The ZIP bundle
manifest remains version 1. Current settings write only authoritative
`preferences`; missing/null values never revive obsolete format/mode/buffer
fields. With overrides enabled, each property inherits endpoint → common →
Automatic policy. With overrides disabled, retained settings **and mappings** are
inactive. Visibility changes neither policy nor values. Hole enable and routing
engine selection are independent. Empty maps use default index mapping, silence
extra outputs and discard unmatched sources. Explicit requirements do not fall
back silently. Native descriptors and runtime results are not saved intent.

The owner will regenerate development profiles; older-build compatibility is not
an acceptance requirement. The existing legacy reader remains a convenience.
Production O-004/O-006 and backend/conversion/synchronization decisions remain open.


## Border and Enter refinements

Application buttons, input/dropdown/list frames and rounded cards use outlines
at least two physical pixels thick, scaled from two DIPs. Integral-pixel native
outlines avoid fractional-width strokes. Windows high contrast retains native
input chrome. This change requires native visual inspection at multiple DPIs;
a successful cross-build does not establish that shimmer is eliminated.

Enter saves editable device/profile fields through their existing Save action,
without starting routing or applying live profile changes. Profile/hole rename
dialog acceptance saves the renamed draft. Notes remain multiline: Enter inserts
a newline. Endpoint search text must be resolved to a listed device before saving;
no identity is guessed. Repeated queued Enter events coalesce and cannot save a
different record after navigation. Settings entries commit their existing setting;
scan/search controls do not acquire an implicit Start or unrelated Save action.


## Owner-authorized workflow refinements

The current implementation contract is [PROFILE_BUILDER.md](PROFILE_BUILDER.md):
individual profile record **version 2**, aggregate transfer wrapper **version 6**,
ZIP manifest still version 1. Current profiles write hole-level `routing` and
`exclusive_mode_preferred`, plus per-direction preferences including reserved
`preferred_clock_device`. Supported routing values are automatic/wasapi/miniaudio/
experimental. Forced access mode takes precedence over the exclusive preference;
clock preference currently has no engine effect. Separate Forward/Return tuning
panels are disclosures only and sit beneath their respective direction groups.
This supersedes earlier record-v1/transfer-v5 and single-panel UI descriptions.
These changes do not add a streaming backend or settle production architecture.


The welcome uses the owner's supplied introduction verbatim and existing Norm
preview artwork. It's my first time opens Preview for now; Show me them holes!
opens the working Test environment; I'm not ready yet exits. All buttons and the
copy remain reachable at smaller display sizes through wrapping/scrolling. The
introductory copy does not change engine capabilities. Tooltips identify the
current development environments and unavailable streaming behavior.

`AudioHole.has-run` lives in the shared working data folder and is created after
explicit tutorial completion or skipping. The old executable-side marker is imported once. A missing marker forces the welcome even when
startup display is disabled or minimized. Cancel does not mark a first run.
Replay Tutorial confirms, confirms any unsaved editor discard, removes the marker
and restarts with the existing single-instance restart handshake. Replay starts a fresh Norm tutorial.
A non-writable data folder reports marker errors; it does not request admin
rights or silently redirect state elsewhere. Smoke launches never read or
write the installed marker. Resetting the marker leaves device/profile data alone.

Device availability transitions select external Norm/UwU commentary; Vanilla
states the device name and observed change only. The initial saved inventory,
unchanged polling, renames, hidden-status edits and clearing records are silent.
Reported not_present/unplugged transitions are disconnections; other reported
unavailability is named accordingly. This observer uses discovery/registry state,
not audio callbacks, and never claims that a device streamed successfully. Unknown
hardware enters this view through normal discovery; no intrusive auto-scan is
started to obtain a personality response. Existing operational warnings take
priority over these cosmetic callouts.


## Current directional controls and package layout

[PROFILE_BUILDER.md](PROFILE_BUILDER.md) now defines individual record v3 and
transfer wrapper v7. Each direction owns its tuning-enable and exclusive-preference
booleans. Their controls live inside/above the corresponding tuning and information
areas; there is no shared tuning master. Routing remains hole-wide. Clock intent
continues to persist without a visible implementation-status notice. Runtime
clock support is unchanged. Saved startup preferences can be edited in Test mode
without enabling unavailable backend operations.

Packages place only AudioHole.exe and README.txt at the root beside folders.
Helpers and AudioHole.has-run are in bin/; avatar-assets.json is in assets/;
third-party notices are in licenses/; documents and build-info.json are in docs/.
A present marker suppresses the welcome regardless of deprecated dialog settings.
Replay deletes it and restarts after confirmation. Exit uses Exit/Cancel. Fresh
appearance follows Windows; dropdown wheel gestures scroll pages without selecting.

### Endpoint diagnostics and routing preferences (2026-11-12)

This amendment supersedes the earlier shared Signal Testing & Monitoring pane and
Arm Ready button descriptions. Performance Diagnostics is a fixed sibling below
Profile's scrolling hole editor; scrolling Forward/Return settings does not move
it. Full path details remain in its tooltip. Save is disabled when the editor
matches the saved profile, including after undo; invalid pending input still
requires correction. Application preferences never make a profile dirty.

Each Forward/Return capture card owns **Check level / Cancel check** and a peak
meter. Each playback card owns **Test beep / Cancel beep**. Devices exposes the
same tools between Overview and Device Notes. The exact logical endpoint shown
is the target; hidden/unavailable/ambiguous bindings are never substituted.
Checking an already-assigned endpoint does not save a profile or start routing.
Only one temporary diagnostic session can run at a time. Stop, any tab change,
profile/hole/endpoint navigation, window hiding/minimizing/closing, endpoint loss
or binding replacement requests cleanup. A subsequent session waits for cleanup.

Interactive Test mode supplies a small Windows shared-mode diagnostic service:
a quiet approximately 1 kHz, -30 dBFS, one-second beep with fades, or temporary
capture peak sampling. Capture packets are discarded, never recorded. Output
uses the first two stream channels (remaining channels silent); this is not
physical mixer-bus discovery, end-to-end verification, or a latency measurement.
Preview remains explicitly simulated and silent. Native smoke modes never open
hardware streams. No routing backend is introduced by these tools. Stream event
waits poll cancellation at most every 30 ms; Windows driver activation/cleanup
calls are cooperative and do not have a hard interruption deadline. Queued audio
can remain briefly audible after cancellation.

Routing Activation adds two saved application policies:

- **Automatically start routing once devices become available** defaults on.
  Start or scheduled activation arms waiting; merely launching or selecting a
  device does not. Turning it off rejects a new unavailable activation. Stop
  disarms waiting and recovery. Existing active-route device recovery is retained.
- **Automatically activate selected profiles while routing** defaults off.
  When enabled, selecting another saved profile while armed requests activation.
  An unavailable replacement requires confirmation before replacing the old
  profile with a waiting plan. A failed switch preserves the previous plan.
  Unsaved edits still require explicit Save/Apply. This does not enable a routing
  backend in Test mode.

Endpoint lists gray out logical endpoints assigned to another active slot in the
same hole. Keyboard navigation skips them, and selection/save/import validation
reject duplicates. Distinct capture and playback logical IDs on one physical
interface are independent. Other holes may reuse endpoints. **Clear endpoint**
is always last and clears only that slot's assignment and label hint; it does
not delete the scanned device. Inactive Return assignments do not reserve slots.

### Advanced tuning and Settings polish (2026-11-12)

Advanced controls retain edits regardless of the **Use advanced tuning** switch.
For example, selecting Force exclusive mode while the switch is off saves that
choice for later use; it remains inactive until the switch is enabled. Automatic,
Force exclusive mode and Force shared mode remain independent in Forward/Return.
Changing a choice neither enables tuning nor saves/applies a live route. Invalid
editable tuning input must be corrected before Save, even while tuning is off.
This fixes the previously silent reset of mode choices made with tuning disabled.
The buffer field and its Apply button share a height and row alignment. The
unused fallback-device dropdowns are removed from both direction panels.

The Settings environment-selection panel is removed. **Restart AudioHole** is a
standalone button below Advanced, retaining confirmation for unsaved profile and
device edits. Existing saved environment preferences and welcome/replay behavior
are unchanged. Preview-to-Tutorial conversion remains a future task.

The DFA slider, exact-value editor and arrow control consume wheel gestures at
both the wx event boundary and native Windows control boundary. Gestures scroll
the Settings page without changing DFA or selecting commentary. Deliberate
slider drags, typed values, keyboard arrows and button clicks still work. The
arrow control matches the numeric editor height instead of its native oversized
default. DFA remains transient cosmetic state and is not serialized.

### Tutorial replaces Preview and header/diagnostic refinement (2026-11-13)

The user-facing Preview environment is replaced by the isolated, event-driven
Norm tutorial specified in TUTORIAL.md and the owner's version-2.4 script. The
first-run marker is written only after tutorial completion or an explicit skip.
An early exit restarts the lesson next launch; completion keeps the current
sandbox open until exit. Ordinary working-mode startup ignores legacy
preview_mode preferences. Internal fixture-only smoke coverage remains intact.

The header now enlarges the logo, two-color wordmark and subtitle. Status text
sits below them, wraps longer profile names and retains full text in its tooltip.
The personality card and equal-width action buttons share the available middle
width; the avatar is centered in the right-hand reserved region. Status/copy
length cannot resize these regions. Tutorial narration has a larger viewport and
a reserved two-choice area at Norm's feet.

Capture checks use one compact row: Check level / Cancel check with a half-width
meter beside it. Playback keeps Test beep / Cancel beep in a single row. The
extra confirmation labels underneath are removed from all Forward/Return cards
and Devices. Start/cancel/completion commentary uses the selected personality;
Vanilla retains neutral facts. Errors remain visible through the existing error dialog
and status response. Temporary streams still cancel on navigation, hide,
minimize, close, Stop or endpoint/binding change. No routing backend is added.


### Engine integration and current tutorial/UI corrections

ENGINE_INTEGRATION.md supersedes previous configuration-only limitations for
normal operation. Start/Stop use the provided engine 0.6 through the controller;
Tutorial stays fictional. Actual opened formats appear in endpoint cards only
for matching Start-time assignments. Performance Diagnostics stays pinned and
scrollable, with real configurations/access receipts/counters and preserved
scroll position. Save does not modify the live plan; explicit Apply saves and restarts with the edits.

Operation-mode header reserves five wrapped status lines and lets the personality
card fill the remaining height below the action row, leaving standard spacing
above the tabs. The proportional avatar occupies the full header height in its
right-hand region. Tutorial sizing stays independent. Settings names are Application and Tray and
Personality and Animation; Restart belongs to Application and Tray. Unsupported
automatic reconnect/live-switch controls are disabled for engine 0.6.

Tutorial script v2.5 adds confirmed right-click Rename steps for the profile and
hole, accepting valid names different from their initial defaults without reply
buttons. New choices receive Dark/Light/System colors immediately, pink outlines
matching the callouts, and no hover text. High contrast retains native styling. Click overlays
last six seconds. Simulated activation lasts ten seconds with no progress bar;
only its completion event advances. Skip becomes End tutorial on completion.
The final spoken copy has no implementation-flag wording. Emotes use a valid
player priority; intentionally static deadpan does not warn about missing animation.
Replay and Restart wait on an inherited parent-process handle after cleanup.

Tutorial selection guidance outlines the specific FakeTech device rows rather
than entire lists, following stable IDs and remaining only until their saved
renames. Click-through outlines pulse outward by 3 DIP for one second, return
over one second and rest for one second; Low motion/Windows animation preferences
retain static outlines. The final authored message explicitly announces completion
and provides an End tutorial action at Norm's feet. Skip and End confirm their
named restart actions; details and the one-time working greetings are in TUTORIAL.md.

### Current tutorial guidance and wheel polish

Device name guidance requires the intended selected device; name/Save guidance
ends on its confirmed save. Profile/hole renames outline only the stable-ID row.
Device-card guidance centers smoothly over two seconds, exclusive preference
over one second; manual wheel input cancels automatic movement. Norm's response
buttons pulse like the other outlines, except the static End tutorial button.

On Profiles, an overflowing text/list field scrolls itself while it can move in
the requested direction. At either limit, or when content already fits, the
wheel passes to the nearest scrollable ancestor that can move. Dropdown and DFA
values remain protected from wheel changes. Performance Diagnostics stays pinned.

### Current owner-requested tutorial and operation fixes

Sightseeing hides reply buttons and all unrelated outlines; only the required tab
is outlined until the user returns to the same step/branch. Talking reactions play
four to six times and return to the owned baseline, yielding to operational warnings.
Hole reveal tweens now take two seconds (one for exclusive preference) with an
11-ms sample interval. The authored dialogue and underlying scroll distances stay
unchanged. Invented-fact reactions in both Norm and Horu omit fictional disclaimers.
Disabled tutorial settings use flat gray text and disable the animation edit/arrows
explicitly. Clock and editable endpoint selectors retain a normal row height across disclosure,
theme and DPI changes.
Custom input/list borders discard copied resize pixels to avoid interior trails.
Performance Diagnostics has its own vertical sash and remains outside the hole editor.

Advanced buffer controls explicitly say Buffer request and explain driver alignment.
Apply saves and restarts with edited requests; negotiated actual buffers appear in diagnostics,
including the request and the actual frame/rate domain. Ready device-arrival behavior
and the engine audit's limitations are documented in ENGINE_INTEGRATION.md.

### Profile Save and unified Apply (2026-11-15)

All three Profile-tab Apply buttons validate/save the complete draft and apply
it, restarting the active profile after confirmed cleanup. Save only persists
intent. Apply availability is shared, including disabled controls without a
routing backend. Stop cancels a pending restart; backend failures retain the
saved edits and remain visible. Incidental panel refreshes preserve pending
text; invalid tuning blocks Save/navigation instead of restoring older values.
See the [field mapping and audit](../PROFILE_PERSISTENCE_AUDIT_2026-11-15.md).

## Owner-authorized Plug Board tab

Plug Board appears between Profiles and Devices. Embed the supplied 0.2.6
socket workspace and settings drawer without changing their layout or controls.
It follows the host appearance and personality setting. See
[PLUG_BOARD_INTEGRATION.md](PLUG_BOARD_INTEGRATION.md) for identity, policy,
visibility, tutorial and lifetime contracts.
