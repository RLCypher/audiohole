# AudioHole v0.1 — Device Discovery and Capability Probing

**Document:** `DEVICE_PROBING.md`  
**Status:** Initial design draft  
**Applies to:** AudioHole v0.1  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `CONFIG_FORMAT.md`, `AGENTS.md`

---

## 1. Purpose

AudioHole must identify the audio formats and operating parameters supported by connected devices, including capabilities that Windows or device drivers do not explicitly advertise.

**The primary objective is to identify the most compatible operating configuration between connected devices while minimizing unnecessary format conversion, resampling, buffering, and processing overhead.**

Whenever practical, AudioHole should connect devices using matching sample rates, sample representations, bit depths, and channel configurations.

A direct format match is preferred because it can eliminate unnecessary conversion stages.

However, matching nominal formats does not guarantee that independent devices share a clock. Clock-drift compensation may still be necessary.

The capability probing system must distinguish reported capabilities from experimentally accepted and operationally verified capabilities.

Its findings must remain transparent, persistent, and manually inspectable.

---

## 2. Operating Modes

AudioHole shall provide two levels of capability discovery.

### 2.1 Easy Mode — Reported Capabilities

Easy Mode is the default.

It shall:

- Enumerate Windows audio endpoints.
- Read capabilities exposed by Windows and available driver interfaces.
- Collect relevant endpoint properties.
- Record shared-mode mix formats.
- Query supported formats using nonintrusive capability interfaces where available.
- Record available device identification information.
- Save the results without opening audio streams for disruptive testing.

Easy Mode shall not generate audible test signals or perform extended stream testing.

It shall not assume that every driver-reported format has been operationally verified.

Easy Mode should be fast, minimally disruptive, and suitable for routine device updates.

### 2.2 Advanced Mode — Experimental Capability Probing

Advanced Mode shall actively test candidate audio configurations, including common and plausible configurations that are not explicitly advertised.

It may:

- Attempt shared-mode and exclusive-mode stream initialization.
- Test candidate sample rates.
- Test sample representations and bit depths.
- Test channel counts and channel layouts.
- Test supported buffer sizes and periods.
- Start streams to evaluate runtime behavior.
- Conduct bounded operational tests.
- Measure stream stability and error conditions.

Advanced Mode must never equate successful initialization with confirmed operational compatibility.

It shall record the evidence obtained for each candidate configuration.

Advanced Mode requires explicit user authorization.

---

## 3. Device Readiness and Safety

### 3.1 Required User Notice

Before Advanced Mode begins, AudioHole shall display a warning explaining that active capability testing may temporarily interfere with normal audio operation.

The warning must instruct users to:

- Connect all devices they intend to test.
- Ensure those devices are powered on.
- Ensure wireless or streamed devices have an active connection when applicable.
- Avoid relying on the tested devices for important audio during the scan.
- Lower speaker and headphone volume to a safe level.
- Be aware that testing may produce unexpected noises, clicks, pops, silence, or brief interruptions.

Suggested warning:

**Advanced Audio Device Probing**

AudioHole is about to test audio configurations beyond those explicitly reported by Windows.

This process may temporarily interrupt audio playback or recording, take exclusive control of devices, or produce unexpected and potentially unpleasant sounds.

Before continuing, make sure your devices are connected, powered on, and ready for testing. Reduce speaker and headphone volume to a safe level.

Advanced probing may take several minutes depending on the number of devices and configurations tested.

**Proceed only when you are ready for possible audio disruption.**

### 3.2 Authorization

Advanced probing must require explicit confirmation.

Easy Mode does not require this confirmation unless an operation would itself be disruptive.

### 3.3 Safe Test Behavior

AudioHole must not automatically emit full-scale tones, broadband noise, or other potentially loud signals.

Where possible, testing should use silence or controlled low-level signals.

Any audible signal test must be separately authorized, clearly identified, and subject to conservative output-level limits.

A driver or device may still produce unexpected noise despite application-level safeguards.

### 3.4 Cancellation

The user must be able to cancel probing.

Cancellation shall:

- Stop scheduling new tests.
- Attempt to terminate the active test safely.
- Release resources.
- Preserve completed results.
- Mark interrupted results appropriately.

Tests that cannot be safely interrupted immediately must have bounded execution time and explicit timeout handling.

---

## 4. Scan Scope and Update Modes

### 4.1 Full Scan

A full scan shall enumerate all currently discoverable endpoints and collect their reported capabilities.

In Advanced Mode, eligible devices may additionally undergo experimental probing.

### 4.2 Update Scan

An update scan shall identify changes since the previous scan.

It shall:

1. Enumerate currently discoverable devices.
2. Match devices against saved records using durable identity information.
3. Acknowledge recognized devices.
4. Identify newly discovered devices.
5. Identify missing or unavailable devices.
6. Detect relevant changes in known devices.
7. Probe new devices according to the selected mode.
8. Reuse valid existing results for unchanged devices.
9. Save the updated device inventory.

Known devices shall not be unnecessarily subjected to repeated Advanced Mode testing.

### 4.3 Change Detection

Relevant changes may include:

- New hardware.
- Removed hardware.
- Endpoint reenumeration.
- Driver changes.
- Firmware changes where detectable.
- Endpoint property changes.
- Audio format changes.
- Transport or interface changes.
- User-requested reprofiling.

A change in a temporary Windows endpoint identifier alone must not automatically invalidate a known device's complete capability history.

### 4.4 Forced Reprobe

Users shall be able to select an individual device and force a fresh capability test.

Users shall also be able to invalidate stale results without deleting the device's identity or aliases.

### 4.5 Missing Devices

Missing devices shall remain in the saved database.

Their records must be marked unavailable rather than deleted automatically.

Experimental probing must not proceed against a device that cannot be opened or whose required transport is unavailable.

---

## 5. Capability Sources and Evidence

AudioHole shall maintain a distinction between the source of a capability claim and the confidence of its operational verification.

### 5.1 Reported Capabilities

Reported capabilities originate from Windows, available driver interfaces, or device-provided information.

These receive priority during automatic format selection.

Reported capabilities are authoritative evidence of what the software interface advertises, but they are not proof of stable real-world operation.

### 5.2 Experimentally Accepted Capabilities

An experimentally accepted capability is a configuration for which an attempted API operation succeeded.

This may include successful stream initialization or startup.

Accepted configurations must not automatically be labeled verified.

### 5.3 Operationally Verified Capabilities

A capability may be marked operationally verified only after completing defined runtime checks.

Verification evidence may include:

- Successful stream initialization.
- Successful stream startup.
- Successful buffer processing over a defined duration.
- Absence of observed stream errors.
- Acceptable buffer behavior.
- Valid capture data where independently assessable.
- Successful signal transfer where an authorized test arrangement permits verification.

The verification record must state what was actually tested.

For example, successful rendering of silent buffers does not prove that a loudspeaker produced audible output.

Likewise, successful capture of silent data does not prove that a microphone is functioning correctly.

### 5.4 Failed Capabilities

Failed attempts shall be recorded with:

- Requested configuration.
- Test stage.
- Error code.
- Human-readable error explanation where available.
- Relevant device and driver information.
- Timestamp.
- Timeout or cancellation status.

A failed test must not permanently blacklist a format across driver or hardware changes.

### 5.5 Inconclusive Results

Tests with insufficient evidence shall be marked inconclusive.

Examples include:

- Stream initialization succeeded but startup failed.
- Stream startup succeeded but runtime behavior was unstable.
- Capture produced silence with no independent signal reference.
- A test timed out.
- The device disconnected.
- Another application prevented exclusive access.

Inconclusive results must not be silently promoted to verified status.

---

## 6. Candidate Format Generation

Advanced Mode shall generate a bounded set of candidate configurations.

### 6.1 Candidate Sample Rates

Initial candidates may include:

- 8,000 Hz
- 11,025 Hz
- 16,000 Hz
- 22,050 Hz
- 32,000 Hz
- 44,100 Hz
- 48,000 Hz
- 88,200 Hz
- 96,000 Hz
- 176,400 Hz
- 192,000 Hz

The list shall be extensible.

Not every rate must be tested against every device.

### 6.2 Candidate Sample Representations

Candidate representations may include:

- Signed integer PCM, 16-bit.
- Signed integer PCM, 24-bit packed.
- Signed integer PCM, 24 valid bits in a 32-bit container.
- Signed integer PCM, 32-bit.
- IEEE floating-point PCM, 32-bit.

Other representations may be added where relevant.

Container width and valid bit depth must be distinguished.

### 6.3 Candidate Channel Configurations

Candidates may include:

- Mono.
- Stereo.
- Quadraphonic.
- 5.1 surround.
- 7.1 surround.
- Other configurations reported by the device.

Channel count and channel layout must be treated as distinct properties.

AudioHole must not assume that an accepted channel count implies a particular speaker arrangement.

### 6.4 Candidate Buffer Configurations

Buffer testing may consider:

- Driver-reported minimum periods.
- Driver-reported default periods.
- Supported fundamental period increments.
- Common low-latency buffer sizes.
- Larger fallback buffers.

Requested buffer values must be distinguished from actual values returned by the audio API.

### 6.5 Search Strategy

The probe shall not blindly execute every possible combination.

Candidate ordering should prioritize:

1. Explicitly reported configurations.
2. Formats commonly used by the device class.
3. Configurations matching other devices in the active profile.
4. Common Windows audio formats.
5. Less common but plausible configurations.
6. Experimental combinations.

The probing system should use information from earlier failures to avoid redundant tests.

Unsupported combinations should be skipped when existing evidence makes failure sufficiently predictable.

### 6.6 Search Limits

Probing must have configurable limits for:

- Maximum test duration.
- Maximum attempts.
- Per-attempt timeout.
- Number of consecutive failures.
- Candidate format range.

The application must prevent unbounded brute-force testing.

---

## 7. False Positive Detection

False positives are a primary concern.

A successful API response is not sufficient evidence that a device can reliably use a requested configuration.

### 7.1 Verification Stages

Where applicable, tests shall progress through distinct stages:

**Stage 1 — Capability Query**

Ask the relevant Windows interface whether a configuration is supported.

**Stage 2 — Initialization**

Attempt to initialize the requested audio stream.

**Stage 3 — Startup**

Attempt to start the initialized stream.

**Stage 4 — Runtime Stability**

Operate the stream for a bounded period while monitoring errors and timing behavior.

**Stage 5 — Signal Verification**

Where a suitable authorized test arrangement exists, verify that a known signal can pass through the configured path.

Each stage shall produce an independently recorded result.

### 7.2 Shared-Mode Conversion

Windows shared-mode audio may accept formats through software conversion.

AudioHole must distinguish:

- A format natively supported by the endpoint or driver.
- A format accepted through shared-mode conversion.
- A format accepted but not operationally verified.

Shared-mode acceptance must not be treated as evidence of native hardware support.

### 7.3 Repeated Testing

Suspicious or inconsistent results may be retested.

Repeated success increases confidence but does not prove universal stability.

Repeated failures should reduce the priority of a candidate during subsequent probing.

### 7.4 Stability Thresholds

Operational verification shall use documented minimum test durations and acceptance criteria.

Thresholds must be configurable and recorded alongside the results.

The application must not invent or infer successful signal delivery from initialization alone.

### 7.5 Verification Labels

Each result shall use a clear classification:

- `reported`
- `accepted`
- `verified`
- `failed`
- `inconclusive`
- `not_tested`

A result may retain multiple pieces of evidence, such as being both reported and verified.

The stored representation must not collapse these distinctions into one misleading boolean.

---

## 8. Format Selection and Optimization

### 8.1 Primary Objective

AudioHole should minimize unnecessary processing between the source and destination.

When devices support matching configurations, those configurations should receive preference.

### 8.2 Selection Priorities

First establish eligibility using user requirements, technical constraints, current availability, and permission for experimental capability use (Section 8.5). Authorization to run intrusive tests does not itself authorize automatic use of experimental results.

Then rank eligible configurations using applicable evidence of compatibility, verified stability, fidelity, latency, and unnecessary processing. Reported provenance and runtime verification can coexist. Reported configurations retain preference over experimental alternatives merely offering more desirable nominal specifications; neither provenance nor a short verification test establishes unconditional stability. Exact scoring remains open under D-035 and `GUI.md` Section 25.3.

Experimental formats must not automatically outrank reported formats merely because they appear to offer a more desirable specification.

### 8.3 Shared Trinity

AudioHole shall retain the concept of a **Shared Trinity**: the compatible combination of sample rate, sample representation/bit depth, and channel configuration between connected devices.

Where possible, the profiler should identify a matching Shared Trinity.

The Shared Trinity is an optimization objective, not a guarantee that resampling or drift compensation can be eliminated.

### 8.4 Conversion Reporting

When AudioHole introduces conversion, the GUI and diagnostics should identify:

- Source format.
- Destination format.
- Required conversion.
- Reason for conversion.
- Relevant latency or processing implications where measurable.

### 8.5 Experimental Format Selection

Experimentally discovered formats may be used when:

- The user explicitly permits experimental capabilities.
- The configuration has sufficient supporting evidence.
- No safety or compatibility restriction prevents its use.

Experimental results must remain distinguishable from reported capabilities in the interface and configuration.

---

## 9. Live Diagnostic Console

Advanced probing shall provide a live, verbose diagnostic readout.

This may be:

- An integrated console panel within the GUI.
- A detachable diagnostic window.
- A separate console process connected to the same diagnostic event stream.

A standalone CLI application is not required.

### 9.1 Required Information

The live readout shall show:

- Current device.
- Current test configuration.
- Test stage.
- Start and completion timestamps.
- API return values.
- Errors and warnings.
- Retry attempts.
- Verification results.
- Scan progress.
- Estimated remaining work where reasonably calculable.

### 9.2 Example Output

    [19:42:01.002] INFO  Starting advanced capability probe
    [19:42:01.018] FOUND RØDECaster Pro II Secondary
    [19:42:01.031] INFO  Endpoint: Capture
    [19:42:01.041] TEST  48000 Hz / 24-bit / Stereo
    [19:42:01.047] PASS  Format query
    [19:42:01.052] TEST  Exclusive stream initialization
    [19:42:01.061] PASS  Initialization
    [19:42:01.070] TEST  Stream startup
    [19:42:01.081] PASS  Startup
    [19:42:01.090] TEST  Runtime stability
    [19:42:04.090] PASS  No observed stream errors
    [19:42:04.091] INFO  Signal transfer not verified
    [19:42:04.092] INFO  Saving test evidence

This output is illustrative, not a claim about the actual device's capabilities.

### 9.3 Controls

The diagnostic interface shall provide:

- Start.
- Cancel.
- Pause between tests where supported.
- Verbosity selection.
- Save or export logs.
- Open saved results.
- Filter by device, severity, or test outcome.

### 9.4 Logging Architecture

The console must consume the application's shared diagnostic event stream.

It must not implement an independent device-testing engine.

Diagnostic logging must not block real-time audio callbacks.

---

## 10. Device Database Organization

Device capabilities shall be stored as human-readable JSON files.

Recording and playback endpoints shall be organized separately.

### 10.1 Directory Structure

    devices/
    ├── recording/
    │   ├── rode-rodecaster-pro-ii-secondary.json
    │   └── steam-streaming-microphone.json
    │
    ├── playback/
    │   ├── rode-rodecaster-pro-ii-secondary.json
    │   └── steam-streaming-speakers.json
    │
    ├── recording_devices.json
    └── playback_devices.json

These filenames are illustrative. Actual names must avoid collisions between devices with similar descriptions.

### 10.2 Per-Device Files

Each discovered endpoint shall have an individual JSON record containing:

- Schema version.
- Stable logical identity.
- Human-readable device name.
- Endpoint direction.
- Available identification characteristics.
- Windows-reported capabilities.
- Experimentally tested capabilities.
- Verification evidence.
- Failed and inconclusive attempts.
- Relevant driver and environment information.
- Last discovery timestamp.
- Last probe timestamp.
- Current availability status, where applicable.

### 10.3 Summary Files

`recording_devices.json` shall provide a consolidated inventory of recording devices.

`playback_devices.json` shall provide a consolidated inventory of playback devices.

Each summary shall contain the relevant device information from its corresponding directory.

Summary files must be reproducible from the individual device records.

### 10.4 Source of Truth

Individual device JSON files are the authoritative device records.

Summary files are generated views of those records.

Manual edits to authoritative device records must be respected after validation.

Regenerating summary files must not silently overwrite authoritative records.

### 10.5 Portable and Local Information

Portable device identity and capability information must remain distinguishable from machine-specific information.

Windows endpoint IDs, driver instance identifiers, and environment-dependent test results must not be treated as universally portable identities.

The final file schema and separation of machine-specific data shall be defined in `CONFIG_FORMAT.md`.

---

## 11. Example Device Record

The following illustrates the intended structure. It is not a finalized schema or an actual probe result.

    {
      "schema_version": 1,
      "device_id": "rodecaster-secondary-capture",
      "display_name": "RØDECaster Pro II Secondary",
      "direction": "recording",
      "identity": {
        "manufacturer": "RØDE",
        "product": "RØDECaster Pro II",
        "interface": "USB Secondary"
      },
      "reported_capabilities": [
        {
          "sample_rate": 48000,
          "container_bits": 32,
          "valid_bits": 24,
          "channels": 2,
          "mode": "exclusive",
          "support": "reported"
        }
      ],
      "experimental_capabilities": [
        {
          "sample_rate": 96000,
          "container_bits": 32,
          "valid_bits": 24,
          "channels": 2,
          "mode": "exclusive",
          "query_result": "accepted",
          "initialization_result": "accepted",
          "runtime_result": "inconclusive",
          "signal_verified": false,
          "experimental": true
        }
      ],
      "last_probe": null
    }

Values are placeholders and must not be interpreted as verified hardware capabilities.

The final schema must explicitly identify PCM representation, channel layout, evidence provenance, test environment, and timestamps.

---

## 12. Data Integrity and Updates

### 12.1 Atomic Writes

Device records shall be written safely to avoid corruption during interruption or application failure.

### 12.2 Preservation of User Changes

AudioHole must not silently discard manually edited aliases, notes, or configuration preferences.

Machine-generated observations and user-authored overrides should be distinguishable.

### 12.3 Historical Results

Where practical, AudioHole should preserve useful prior probe results rather than replacing them without trace.

### 12.4 Stale Results

Results collected under a different driver, firmware, or hardware environment shall be identified as potentially stale.

Stale experimental results must not automatically be treated as current verified capabilities.

### 12.5 Summary Regeneration

The recording and playback summary files shall be regenerated when authoritative device records change.

Regeneration must validate source records and report errors.

### 12.6 Duplicate Devices

Two physically distinct devices with identical names and product identifiers must not silently overwrite each other's records.

When durable hardware identity is insufficient, AudioHole must preserve ambiguity and request user confirmation where necessary.

---

## 13. Performance and Resource Management

### 13.1 Probe Isolation

Capability probing must be separate from the real-time routing engine.

### 13.2 Active Routes

Advanced probing must not silently disrupt active AudioHole routes.

If a test requires access to an endpoint currently in use, AudioHole shall request permission to suspend the affected route or skip that test.

### 13.3 Timeouts

Every active probe operation must have a bounded execution strategy.

A stalled or unresponsive driver must not leave the GUI permanently blocked.

### 13.4 Resource Cleanup

AudioHole shall release audio clients, buffers, and other resources after each test.

### 13.5 Background Activity

Capability probing must not continue consuming significant resources after completion or cancellation.

---

## 14. Acceptance Scenarios

### Scenario A — Easy First Scan

A user launches AudioHole for the first time and selects Easy Mode.

AudioHole discovers available endpoints, records reported capabilities, and creates the appropriate JSON files without generating test audio.

### Scenario B — Advanced Probe

A user selects Advanced Mode.

AudioHole presents the safety warning, obtains confirmation, tests plausible configurations, and displays detailed progress in the diagnostic console.

### Scenario C — False Positive

A device accepts initialization for a candidate format but fails during runtime testing.

AudioHole records the successful initialization and failed runtime stage separately.

The configuration is not marked operationally verified.

### Scenario D — New Device Update

A user connects a new USB audio interface and selects Update Scan.

AudioHole recognizes existing devices, acknowledges them without unnecessary retesting, and scans the newly discovered device.

### Scenario E — Device Reenumeration

A known device receives a different Windows endpoint identifier after a driver update.

AudioHole attempts to match the existing logical device identity and retains its history while marking affected capability results for review or reprobe.

### Scenario F — Missing Device

A saved device is disconnected during a scan.

AudioHole retains its JSON record, reports it as unavailable, and does not attempt active probing.

### Scenario G — Interrupted Probe

The user cancels an advanced scan.

AudioHole safely stops testing, releases resources, and preserves completed results.

### Scenario H — Experimental Compatibility

Two devices have different advertised formats but share an experimentally accepted format.

AudioHole may offer that configuration as an experimental option, clearly indicating its evidence level and potential risks.

It must not silently prefer that format over a stable reported configuration.

### Scenario I — Manual Database Edit

The user edits a device JSON file.

AudioHole validates the modification, preserves supported user-authored changes, and regenerates the appropriate summary.

### Scenario J — No Conversion Necessary

Two devices report a compatible sample rate, sample representation, and channel configuration.

AudioHole prefers that matching configuration when other operating requirements permit it.

Any remaining resampling or drift compensation must be justified and reported.

---

## 15. Definition of Done

The device discovery and capability probing subsystem is complete for v0.1 when:

- Easy Mode discovers and records reported endpoint capabilities.
- Advanced Mode performs bounded experimental tests.
- Users receive explicit safety warnings before disruptive operations.
- Devices must be connected and usable before active testing.
- Initialization success is distinguished from operational verification.
- False positives and inconclusive results are represented accurately.
- Update scans recognize known devices and identify new hardware.
- Individual device records are stored in human-readable JSON.
- Recording and playback records use separate directories.
- Summary JSON files are generated from individual records.
- Manual configuration edits are preserved appropriately.
- Probe results inform automatic format negotiation.
- Windows-reported capabilities receive priority over experimental alternatives.
- A live, verbose diagnostic interface is available.
- Probing can be cancelled safely.
- Testing does not block the real-time audio engine.
- All results include sufficient evidence to explain their classification.

**The objective is not to discover the largest number of formats a driver will accept. It is to discover which configurations can actually be used reliably, then select the configuration that connects devices with the least unnecessary processing.**

---

## Reconciled evidence and probe execution boundaries

Windows does not expose a universal exhaustive list of native hardware formats. `GetMixFormat` describes the shared engine mix format; format-query and initialization results describe the requested mode/API conditions. Shared-mode acceptance may involve conversion and never proves native hardware support. Easy Scan may legitimately report unavailable capability information rather than fabricate lists. Separate rate/format/channel lists do not prove every combination works.

Record stage outcomes separately from the overall conclusion. A failed startup stage may leave the overall compatibility conclusion inconclusive because access was busy, a device disappeared, or evidence was insufficient. Preserve the successful stages and original errors. Runtime verification is scoped to its duration, operating mode, actual format/period, and environment; it is not proof of audibility or long-term stability. Exact evidence field schemas and default durations remain open under O-004 and D-041.

Eligibility precedes ranking as in Section 8.2 and `GUI.md` Section 7.2. Applicable evidence of compatibility, verified stability, fidelity, latency, and processing cost guides recommendations. Experimental permissions and evidence limitations remain visible; no fixed buffer-position rule or automatic experimental preference is introduced.

Generation-based cancellation prevents stale results from authorizing further work or changing current routing. Stop takes precedence over pending scan operations; completed evidence may still be preserved. Stop/cancel cannot guarantee immediate interruption of every synchronous driver call. Workers and timeouts must keep the UI responsive without unsafe thread termination.

A dedicated Advanced Scan helper process remains provisional, not required. Evaluate improved user-mode fault containment against IPC/lifecycle complexity and in-process workers. A helper watchdog cannot guarantee release of a wedged driver or recovery of the OS audio stack. Adoption would be a narrowly scoped exception to normal single-process routing and must follow the recorded evaluation gate.

Test Beep is separately authorized, exact-destination playback, not implicit signal verification for a probe. Stream progress, silence, and endpoint presence cannot prove external transport activity or human audibility. Keep its existing cancellation, level, and target constraints unchanged.
