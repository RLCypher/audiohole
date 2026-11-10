# AudioHole v0.1 — Configuration Format

**Document:** `CONFIG_FORMAT.md`  
**Status:** Proposed configuration contract  
**Format:** UTF-8 JSON  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `DEVICE_PROBING.md`, `PROFILES.md`, `GUI.md`

---

## 1. Purpose

This document defines how AudioHole stores application settings, device information, capability evidence, profiles, and persistent configuration.

AudioHole's configuration must be:

- Human-readable.
- Logically organized.
- Manually editable.
- Portable between Windows installations and computers.
- Stable across device reenumeration and driver updates.
- Explicit about unknown, unsupported, or unavailable information.
- Safe to validate, migrate, and update.
- Consistent between the GUI and filesystem.

**The filesystem is a supported user interface, not an undocumented implementation detail.**

A user should be able to open any configuration file in a text editor, understand its contents, make reasonable changes, and expect AudioHole to handle those changes predictably.

No proprietary editor, opaque database, or hidden registry state should be necessary to interpret the portable configuration.

---

## 2. General Configuration Principles

### 2.1 Human-Relevant Information First

JSON fields should be ordered by their relevance to a human reader.

For device records:

1. Display name.
2. Windows-reported name.
3. User notes.
4. Reported capabilities.
5. Probed capabilities.
6. Scan information.
7. Device identity and fingerprint.
8. Driver information.
9. Metadata.

The first information visible when opening a device record should explain what the device is and what it can do.

Implementation details belong farther down.

### 2.2 Naming Conventions

Use descriptive `snake_case` field names.

Prefer:

```json
{
  "sample_rate_hz": 48000,
  "channel_count": 2,
  "exclusive_mode": "supported"
}
```

Avoid unexplained abbreviations, undocumented numeric enums, and positional arrays whose meaning depends on their ordering.

### 2.3 Formatting

AudioHole-generated JSON should use:

- UTF-8 encoding.
- Two-space indentation.
- Consistent field ordering.
- Readable line breaks.
- Standard JSON syntax.
- A final newline.

AudioHole should avoid unnecessary rewriting of files.

Although JSON object key order has no semantic significance, AudioHole's serializer must preserve the documented presentation order for readability.

### 2.4 Standard JSON

Authoritative configuration uses standard JSON.

JSON comments are not supported in v0.1.

User annotations belong in designated `notes` fields.

Unknown extension fields should be preserved where safely possible, but must not be interpreted as application instructions unless recognized by the schema.

### 2.5 No Ambiguous Blank Values

Except for intentionally optional user-authored text, missing technical information must be represented explicitly.

AudioHole must distinguish:

- Information that is unavailable.
- Capabilities that are unsupported.
- Capabilities that have not been tested.
- Tests that produced inconclusive results.
- Fields that do not apply.
- Operations that failed.

An empty string, empty object, or empty array must not ambiguously represent any of these conditions.

---

## 3. Standardized Information States

AudioHole must use a consistent vocabulary throughout its configuration and diagnostic files.

### 3.1 Standard States

| State | Meaning |
|---|---|
| `available` | A resource or capability is currently available |
| `supported` | Evidence establishes that a capability is supported |
| `unsupported` | Evidence establishes that a capability is not supported |
| `unavailable` | Information or a resource could not currently be obtained |
| `not_tested` | No experimental test has been performed |
| `inconclusive` | Testing did not establish a reliable conclusion |
| `not_applicable` | The field does not apply |
| `failed` | An attempted operation failed |
| `unknown` | The value cannot currently be determined |

### 3.2 Unsupported Is Not Unavailable

These states must not be treated as interchangeable.

Examples:

- Windows does not report native sample rates: `unavailable`.
- AudioHole has not tested 192 kHz: `not_tested`.
- A valid test conclusively rejects 192 kHz under specified conditions: `unsupported`.
- A test times out without a reliable result: `inconclusive`.
- A device is temporarily disconnected: `unavailable`.

A failed test under one operating mode does not establish that the same format is unsupported under every operating mode.

### 3.3 Missing Values

When a field normally contains a value or collection, use a standardized status object if the information is unavailable.

```json
{
  "sample_rates_hz": {
    "status": "unavailable"
  }
}
```

When known, use the ordinary value:

```json
{
  "sample_rates_hz": [44100, 48000, 96000]
}
```

Additional explanatory information may be included:

```json
{
  "sample_rates_hz": {
    "status": "unavailable",
    "reason": "Driver did not expose this information"
  }
}
```

The `reason` field is informational.

Program logic must evaluate the standardized `status` field rather than parsing explanatory text.

### 3.4 Status-Only Fields

Fields that directly describe a capability or operational state may use standardized status strings.

```json
{
  "exclusive_mode": "not_tested"
}
```

Each schema must specify whether a field accepts:

- A concrete value.
- A status string.
- A status object.
- A documented combination of these representations.

### 3.5 Empty Collections

Empty arrays are valid only when they have a documented meaning.

For example, a newly created profile may contain zero configured holes.

An empty capability list must not mean that the capability was never queried.

### 3.6 Notes Exception

The `notes` field may contain an empty string.

```json
{
  "notes": ""
}
```

This means the user has not entered any notes.

It has no diagnostic or operational significance.

### 3.7 Shared Parser Rules

All AudioHole components must use the same status definitions.

Subsystems must not invent alternative spellings such as `N/A`, `none`, `not supported`, or `unknown_value`.

JSON `null` must not serve as an undocumented substitute for an availability state.

Unknown information must never silently become a plausible technical default.

---

## 4. Configuration Directory Structure

AudioHole should maintain a clearly identifiable portable configuration root.

Proposed structure:

```text
AudioHole/
├── settings.json
├── profiles/
│   ├── Default.json
│   ├── VR Audio.json
│   └── Desktop Audio.json
├── devices/
│   ├── recording/
│   │   ├── Main Mixer.json
│   │   └── Steam Streaming Microphone.json
│   ├── playback/
│   │   ├── Steam Streaming Audio.json
│   │   └── Main Speakers.json
│   ├── recording_devices.json
│   └── playback_devices.json
├── probe_history/
│   └── ...
└── logs/
    └── ...
```

Machine-local runtime information should be stored separately:

```text
AudioHole-Local/
├── endpoint_cache.json
├── device_bindings.json
├── runtime_state.json
└── diagnostics/
```

The exact Windows storage locations remain an implementation decision.

### 4.1 Portable Data

Portable data includes:

- Application preferences.
- Profiles.
- Logical device identities.
- Display names.
- User notes.
- Device fingerprints.
- Reported capabilities.
- Probed capabilities and relevant history.
- Failover configuration.
- User-defined routing preferences.

### 4.2 Machine-Local Data

Machine-local data may include:

- Current Windows endpoint IDs.
- Local device-binding caches.
- USB connection paths.
- Temporary stream information.
- Runtime health information.
- Machine-specific diagnostic details.

Deleting machine-local caches must not destroy portable configuration.

### 4.3 Generated Files

Generated files must be distinguishable from authoritative files.

AudioHole must be able to regenerate consolidated device summaries from individual device records.

---

## 5. Device Names and Filenames

Each recognized audio endpoint should have an individual device record.

Recording and playback records are stored separately.

### 5.1 Display Name

The primary human-facing name is:

`display_name`

It defaults to the Windows-reported endpoint name when the device is first discovered.

The user may change it.

### 5.2 Windows Name

The field:

`windows_name`

stores the name reported by Windows.

Changing the display name must not change the Windows name.

If Windows later reports a different name for the same recognized endpoint, AudioHole may update `windows_name` while preserving the user-selected display name.

### 5.3 Notes

The field:

`notes`

stores arbitrary user-authored text.

Notes:

- May be empty.
- May contain multiple lines.
- May be edited through the GUI.
- May be edited directly in JSON.
- Must be preserved across rescans.
- Must not affect device matching.
- Must not affect routing.
- Must not affect capability evaluation.
- Must not be interpreted as application commands.

### 5.4 Filename Convention

A device record's filename should match its display name wherever Windows filesystem rules permit.

Example:

```text
RØDECaster Pro II USB.json
```

If the user renames the file to:

```text
Main Mixer.json
```

AudioHole should recognize the existing record and update:

```json
{
  "display_name": "Main Mixer"
}
```

The logical device identity remains unchanged.

### 5.5 GUI Renaming

Renaming a device through the GUI updates:

- `display_name`.
- The corresponding JSON filename.

It must not change:

- `windows_name`.
- Stable logical identity.
- Profile references.
- Capability evidence.
- User notes.

### 5.6 Filename Collisions

Two devices may have identical Windows-reported names.

AudioHole must never overwrite one device record with another.

Use readable disambiguating suffixes where necessary.

```text
USB Audio Device.json
USB Audio Device (2).json
```

The stored display name and filename should remain consistent.

### 5.7 Invalid Filename Characters

Windows filename restrictions must be handled predictably.

AudioHole should:

- Validate user-entered names.
- Explain invalid characters.
- Offer safe substitutions where appropriate.
- Prevent collisions.
- Avoid silent truncation or overwriting.

### 5.8 External Rename Detection

When a device file is renamed outside the GUI, AudioHole should recognize it using its internal logical ID.

The filename is authoritative for the display name, but not for device identity.

A malformed record or duplicate logical ID must produce a conflict rather than an automatic association.

---

## 6. Device Record Schema

A device record describes one logical Windows audio endpoint.

The same general structure applies to recording and playback devices.

Example:

**`devices/recording/Main Mixer.json`**

```json
{
  "display_name": "Main Mixer",
  "windows_name": "RØDECaster Pro II USB",
  "notes": "Primary hardware mixer. Used for VR audio and desktop monitoring.",

  "reported_capabilities": {
    "sample_rates_hz": {
      "status": "unavailable"
    },
    "sample_formats": {
      "status": "unavailable"
    },
    "channel_counts": {
      "status": "unavailable"
    },
    "shared_mode": "available",
    "exclusive_mode": "unknown",
    "default_mix_format": {
      "status": "unavailable"
    }
  },

  "probed_capabilities": {
    "scan_status": "not_tested",
    "verified_configurations": {
      "status": "not_tested"
    },
    "failed_configurations": {
      "status": "not_tested"
    },
    "inconclusive_configurations": {
      "status": "not_tested"
    }
  },

  "scan_history": {
    "first_scanned": {
      "status": "not_tested"
    },
    "last_scanned": {
      "status": "not_tested"
    },
    "last_successful_scan": {
      "status": "not_tested"
    },
    "last_easy_scan": {
      "status": "not_tested"
    },
    "last_advanced_scan": {
      "status": "not_tested"
    },
    "last_scan_type": "not_tested",
    "last_scan_result": "not_tested"
  },

  "identity": {
    "logical_id": "rodecaster-pro-ii-usb-capture",
    "direction": "capture",

    "fingerprint": {
      "manufacturer": "RØDE",
      "product": "RØDECaster Pro II",

      "usb": {
        "vendor_id": {
          "status": "unavailable"
        },
        "product_id": {
          "status": "unavailable"
        },
        "serial_number": {
          "status": "unavailable"
        },
        "interface_number": {
          "status": "unavailable"
        }
      },

      "endpoint": {
        "interface_name": "RØDECaster Pro II USB",
        "role": {
          "status": "unknown"
        }
      }
    }
  },

  "driver": {
    "name": {
      "status": "unavailable"
    },
    "provider": {
      "status": "unavailable"
    },
    "version": {
      "status": "unavailable"
    },
    "date": {
      "status": "unavailable"
    },
    "inf_name": {
      "status": "unavailable"
    },
    "service": {
      "status": "unavailable"
    },
    "association_status": "unknown",
    "last_observed": {
      "status": "unavailable"
    }
  },

  "metadata": {
    "schema_version": 1,
    "record_type": "audio_device",
    "first_seen": {
      "status": "unavailable"
    },
    "last_seen": {
      "status": "unavailable"
    }
  }
}
```

This example illustrates the schema, not verified RØDECaster capabilities or driver information.

### 6.1 Required Top-Level Fields

Every authoritative device record must contain:

- `display_name`
- `windows_name`
- `notes`
- `reported_capabilities`
- `probed_capabilities`
- `scan_history`
- `identity`
- `driver`
- `metadata`

### 6.2 Endpoint Direction

Device direction must use:

- `capture`
- `render`

The GUI may display these as Recording and Playback.

### 6.3 Stable Logical Identity

`logical_id` is AudioHole's permanent logical reference to an endpoint.

It must:

- Be unique within the device database.
- Remain unchanged when the display name changes.
- Remain unchanged when the JSON filename changes.
- Survive copying configuration to another machine.
- Remain stable across Windows endpoint reenumeration.
- Be referenced by profiles instead of Windows endpoint IDs.

Readable identifiers are preferred.

Collisions must be detected rather than silently resolved by merging unrelated devices.

---

## 7. Reported Capabilities

Reported capabilities are obtained from Windows and device drivers without intrusive experimental testing.

They must not imply that every reported format has been operationally verified.

### 7.1 Sample Rates

Use integer Hertz values.

```json
{
  "sample_rates_hz": [44100, 48000, 96000]
}
```

### 7.2 Sample Formats

Use documented identifiers such as:

- `pcm_s16`
- `pcm_s24_packed`
- `pcm_s24_in_32`
- `pcm_s32`
- `float32`

Additional identifiers may be introduced through schema updates.

Storage width and valid-bit differences must remain distinguishable.

### 7.3 Channel Information

Represent channel counts and layouts separately.

```json
{
  "channel_counts": [1, 2, 6],
  "channel_layouts": [
    "mono",
    "stereo",
    "5.1"
  ]
}
```

Channel layout must not be inferred solely from channel count when Windows does not establish it.

### 7.4 Shared and Exclusive Modes

Shared-mode and exclusive-mode support must be represented separately.

Different modes may expose different supported formats, periods, and initialization behavior.

---

## 8. Probed Capabilities

Detailed probing behavior is defined in `DEVICE_PROBING.md`.

AudioHole must distinguish:

- Reported capabilities.
- Formats accepted during negotiation.
- Formats verified through successful streaming.
- Failed configurations.
- Inconclusive tests.
- Untested configurations.

### 8.1 Tested Configurations

A probed configuration should identify exactly what was tested.

```json
{
  "mode": "exclusive",
  "sample_rate_hz": 48000,
  "sample_format": "float32",
  "channel_count": 2,
  "channel_layout": "stereo",
  "buffer_frames": 240,
  "result": "verified"
}
```

This is illustrative, not a claim of support by any particular device.

### 8.2 Error Information

Windows errors should retain their original numeric values.

```json
{
  "result": "failed",
  "stage": "stream_initialization",
  "error": {
    "code": "0x88890008",
    "description": "Unsupported format"
  }
}
```

Human-readable descriptions supplement the original error code.

They must not replace it.

### 8.3 Evidence Preservation

A new scan must not silently erase previous useful evidence.

Historical results should remain available for diagnosing changes in behavior.

### 8.4 Large Probe Histories

Detailed histories may be stored in separate human-readable JSON files.

The primary device record should retain a concise summary.

This prevents a heavily tested device from becoming an enormous, difficult-to-read document.

---

## 9. Scan Timestamps and History

Scan information is primarily informational and diagnostic.

Every device record should retain timestamps for:

- First scan.
- Most recent scan attempt.
- Most recent successful scan.
- Most recent Easy Scan.
- Most recent Advanced Scan.

### 9.1 Timestamp Format

Use ISO 8601 timestamps in UTC.

Example:

```json
{
  "first_scanned": "2026-10-10T21:32:18Z",
  "last_scanned": "2026-10-18T04:46:03Z",
  "last_successful_scan": "2026-10-18T04:46:03Z",
  "last_easy_scan": "2026-10-10T21:32:18Z",
  "last_advanced_scan": "2026-10-18T04:46:03Z",
  "last_scan_type": "advanced",
  "last_scan_result": "completed"
}
```

The GUI may convert timestamps to local time.

### 9.2 Timestamp Definitions

**`first_scanned`**

The first completed scan for this logical device.

**`last_scanned`**

The most recent scan attempt, including attempts that failed or were interrupted.

**`last_successful_scan`**

The most recent scan that completed successfully.

**`last_easy_scan`**

The most recent Easy Scan attempt.

**`last_advanced_scan`**

The most recent Advanced Scan attempt.

### 9.3 Scan Results

Scan results should use documented values such as:

- `completed`
- `partially_completed`
- `failed`
- `interrupted`
- `not_tested`

### 9.4 Easy Versus Advanced Scans

Easy Scans and Advanced Scans provide different kinds of evidence.

A recent Easy Scan must not imply that experimental capability tests were recently repeated.

### 9.5 Driver Changes and Probe Validity

When driver information changes, AudioHole should retain previous probe results but mark affected evidence as requiring revalidation where appropriate.

Historical success must not automatically be treated as proof that the current driver behaves identically.

---

## 10. Device Fingerprinting

AudioHole must maintain a collection of identification hints for each logical endpoint.

The goal is to recognize devices across:

- Windows reinstalls.
- Driver updates.
- USB port changes.
- Device reenumeration.
- Computer upgrades.
- Configuration transfers to other PCs.

A fingerprint is a collection of readable evidence, not necessarily a single hash.

### 10.1 Fingerprint Components

Potential identification hints include:

| Hint | Purpose |
|---|---|
| USB vendor ID | Identifies manufacturer |
| USB product ID | Identifies product |
| Hardware serial number | Distinguishes individual devices |
| Interface number | Distinguishes interfaces on composite devices |
| Manufacturer string | Additional identification evidence |
| Product string | Additional identification evidence |
| Windows endpoint name | Human-readable fallback |
| Endpoint direction | Distinguishes capture and render endpoints |
| Endpoint/interface role | Distinguishes endpoints on the same device |
| Hardware IDs | Additional Plug and Play evidence |
| Compatible IDs | Additional device-family evidence |
| Device instance information | Local or partially portable evidence |

Not every hint will be available for every device.

Unavailable hints must be represented explicitly.

### 10.2 Matching Confidence

Fingerprint evidence should be evaluated according to its reliability.

Strong evidence may include:

- Matching hardware serial number.
- Matching vendor and product IDs.
- Matching endpoint direction and interface identity.

Weaker evidence may include:

- Windows-reported name.
- Manufacturer string.
- Product description.

A single weak hint must not be sufficient to silently associate unrelated devices.

### 10.3 Multiple Endpoints

One physical device may expose multiple Windows audio endpoints.

AudioHole must distinguish the correct endpoint, not merely recognize the underlying physical device.

For example, a mixer may expose multiple recording and playback interfaces.

Matching the hardware model alone is insufficient.

### 10.4 Identical Devices

Two physically distinct devices may have identical fingerprints if neither exposes a unique serial number.

AudioHole must not silently assume they are interchangeable.

Ambiguous matches require confirmation.

### 10.5 Historical Fingerprints

AudioHole should retain useful previous fingerprint observations.

This helps distinguish legitimate changes from incorrect matches.

Examples include:

- Changed Windows endpoint ID.
- Changed interface name.
- Updated hardware identification information.
- Driver-related changes to enumeration.

Historical hints must not silently override stronger current evidence.

### 10.6 Stable Logical Identity

The logical ID must not change automatically when fingerprint information changes.

The fingerprint helps locate the device represented by that logical ID.

It does not replace the logical ID.

### 10.7 Human-Readable Match Explanations

Diagnostics should explain why a match was accepted or rejected.

Examples:

> Matched Main Mixer using USB vendor/product IDs, hardware serial number, and endpoint direction. Windows endpoint ID changed.

> Two connected devices match the saved fingerprint. Manual confirmation required.

Matching decisions must not be presented as unexplained opaque confidence scores.

---

## 11. Fingerprints Versus Local Bindings

AudioHole must distinguish portable identification evidence from machine-local endpoint associations.

### 11.1 Fingerprint

Stored in the authoritative device record.

Describes the evidence used to recognize the logical endpoint.

Portable across machines.

### 11.2 Binding

Stored in machine-local cache.

Associates a logical device ID with a currently enumerated Windows endpoint.

May include:

- Current Windows endpoint ID.
- Device instance ID.
- USB location path.
- Last successful match.
- Match confidence.
- Last observed availability.

### 11.3 Regeneration

Machine-local bindings must be reconstructable through device discovery and fingerprint matching.

Deleting a binding cache must not invalidate profiles.

### 11.4 Ambiguity

If multiple endpoints plausibly match a logical device, AudioHole must request confirmation rather than silently choosing one.

### 11.5 Device Replacement

Users should be able to explicitly associate replacement hardware with an existing logical ID.

This allows profiles to survive hardware upgrades.

Relevant capabilities must be reevaluated when replacement hardware differs.

---

## 12. Driver Information

Driver information is stored primarily for diagnostics.

**Driver information must not be treated as a permanent device identity.**

The purpose is to help investigate unexpected behavior, such as:

- Audio dropouts.
- Initialization failures.
- Unexpected format negotiation results.
- Changes in latency.
- Devices disappearing after updates.
- Previously working configurations failing.

### 12.1 Driver Fields

Where Windows exposes the information, AudioHole should record:

- Driver description or name.
- Driver provider.
- Driver version.
- Driver date.
- Installed INF filename.
- Driver service name.
- Driver association status.
- Last observation timestamp.

### 12.2 Windows Driver Discovery

AudioHole may use:

- Windows Core Audio APIs.
- Windows device property interfaces.
- SetupAPI.
- Configuration Manager APIs.

The implementation should attempt to associate an audio endpoint with its underlying Plug and Play device.

### 12.3 Driver Stack Complexity

An audio endpoint may involve:

- Microsoft class drivers.
- Manufacturer-specific drivers.
- USB audio components.
- Virtual audio drivers.
- Multiple related driver components.

AudioHole must not assume the device manufacturer is also the driver provider.

If the relevant driver association cannot be established, the record should indicate `unavailable` or `unknown` as appropriate.

### 12.4 Current Driver Information

The current driver information should appear directly in the device JSON.

Example:

```json
{
  "driver": {
    "name": "USB Audio Device",
    "provider": "Microsoft",
    "version": "10.0.x",
    "date": {
      "status": "unavailable"
    },
    "inf_name": {
      "status": "unavailable"
    },
    "service": {
      "status": "unavailable"
    },
    "association_status": "available",
    "last_observed": "2026-10-18T04:46:03Z"
  }
}
```

The example is illustrative.

### 12.5 Driver History

When a driver change is detected, AudioHole should preserve:

- Previous driver information.
- Newly observed driver information.
- When the change was first observed.
- Relevant scan results associated with each driver.

Detailed history may be stored in a separate JSON file.

### 12.6 Driver Changes Must Not Break Identity

Updating a driver must not automatically cause AudioHole to treat an existing device as new hardware.

Driver information may contribute diagnostic context, but should not be a required identity match.

### 12.7 Diagnostic Correlation

Diagnostics may report:

> Driver version changed since the last successful Advanced Scan. Previous probe results may require revalidation.

This establishes a useful correlation without claiming that the driver update caused a problem.

AudioHole is not intended to become a driver management system.

---

## 13. Consolidated Device Summaries

AudioHole should generate:

```text
devices/recording_devices.json
devices/playback_devices.json
```

These provide aggregate views of known devices.

### 13.1 Source of Truth

Individual device records are authoritative.

Consolidated summaries are generated from those records.

### 13.2 Summary Contents

Summaries should prioritize:

- Display name.
- Windows name.
- Logical ID.
- Direction.
- Current availability.
- Reported capability summary.
- Probed capability summary.
- Last scan information.

### 13.3 Generated File Identification

Generated files must identify themselves clearly.

```json
{
  "generated": true,
  "source": "individual_device_records",
  "schema_version": 1,
  "devices": []
}
```

Here, an empty `devices` array means that generation completed and found zero valid records.

It must not ambiguously mean that enumeration failed or was skipped.

### 13.4 Regeneration

Missing or corrupt summaries should be regenerable without losing authoritative device information.

Manual changes to generated summaries must not overwrite individual device records.

---

## 14. Application Settings

Application-wide preferences belong in:

`settings.json`

Example:

```json
{
  "startup": {
    "start_with_windows": false,
    "profile_selection": "last_used",
    "selected_profile_id": {
      "status": "not_applicable"
    },
    "routing_activation": "manual",
    "activation_delay_seconds": {
      "status": "not_applicable"
    }
  },

  "notifications": {
    "level": "warnings_and_errors",
    "tray_status_enabled": true,
    "taskbar_status_enabled": true
  },

  "interface": {
    "close_to_tray": true,
    "open_window_on_windows_startup": false,
    "personality_mode": "default"
  },

  "metadata": {
    "schema_version": 1
  }
}
```

### 14.0.1 Personality Presentation Preference

`interface.personality_mode` is a single, mutually exclusive presentation setting. Standard IDs are `default`, `vanilla`, and `uwu`; the default is `default`. The owner-approved presentation refinement also permits a valid user-authored catalog ID; see PERSONALITY.md §17. The GUI evaluation keeps this selection session-only and does not finalize production settings serialization.

- `default`: **Normal — Default Audio Engineer**. Contextual personality; DFA visible.
- `vanilla`: **Vanilla — Strictly informational**. Neutral application-authored language; DFA hidden.
- `uwu`: **UwU — I know what you are**. Intentionally authored playful messages; DFA visible.

This setting controls presentation only. It must not alter routing, device probing, recovery, profile contents, structured diagnostics, error codes, endpoint identifiers, file paths, or user-provided text. Changes take effect without restarting routing. Unavailable or invalid selected personality files must be reported and rendered using built-in Vanilla without silently rewriting files. A valid catalog missing an event uses Vanilla for that event alone. Missing values use `default` for backward compatibility. Do not add a separate `vanilla_mode` boolean.

### 14.1 Startup Profile Selection

Valid values:

- `last_used`
- `selected`

When `selected` is used, `selected_profile_id` must reference a valid profile.

### 14.2 Routing Activation

Valid values:

- `manual`
- `immediate`
- `delayed`

Delayed activation requires a valid delay value.

An explicit Stop command must cancel pending delayed activation.

### 14.3 Notifications

Valid levels:

- `all_events`
- `warnings_and_errors`
- `errors_only`

Default:

`warnings_and_errors`

### 14.4 Windows Startup Registration

The application preference expresses the user's desired startup behavior.

AudioHole must verify whether the corresponding Windows startup registration actually exists.

A JSON setting alone is not proof that startup integration is functioning.

---

### 14.5 Startup Window Preference

The GUI setting **Open main window at startup** maps to `interface.open_window_on_windows_startup`. Default: `false` for Windows autostart. When launched through Windows autostart, `true` displays the main window; `false` starts with the GUI hidden and tray available. A normal manual launch opens the GUI unless explicitly requested to run in the background. This setting is independent of routing activation and Windows startup registration.

The older `start_minimized_with_windows` field is superseded. Migrate its boolean value by inversion: `true` becomes `open_window_on_windows_startup: false`; `false` becomes `true`. If both fields are present and conflict, report the conflict rather than guessing. Respect existing schema migration and versioning rules.

### 14.6 Delayed Activation Configuration

When `startup.routing_activation` is `delayed`, `startup.activation_delay_seconds` contains a positive integer. The delayed-mode default is 10 seconds; GUI presets are 5, 10, 15, 30, and 60 seconds, plus a custom value whose supported range remains open under D-042. For other activation modes the field may use the standard `not_applicable` status object.

The delay starts on application launch, not Windows boot, and applies once per launch. A pending countdown is transient controller state and must not be restored as an armed timer from `runtime_state.json`. GUI and tray read the same authoritative pending-activation status.


## 15. Profile File Structure

Profiles are stored as individual human-readable JSON files. Detailed behavior is defined in `PROFILES.md`.

Example: **`profiles/VR Audio.json`**

```json
{
  "display_name": "VR Audio",
  "notes": "RØDECaster audio routing for Steam Frame.",
  "profile_id": "vr-audio",
  "holes": [
    {
      "hole_id": "main-vr-audio",
      "display_name": "Main VR Audio",
      "notes": "",
      "mode": "duplex",
      "enabled": true,
      "audio_defaults": {
        "operating_mode": "automatic",
        "format_selection": "automatic",
        "latency_preference": "low"
      },
      "forward": {
        "source_device_id": "rodecaster-pro-ii-usb-capture",
        "destination_device_id": "steam-streaming-playback",
        "audio": {
          "latency_preference": "low"
        }
      },
      "reverse": {
        "source_device_id": "steam-streaming-microphone",
        "destination_device_id": "rodecaster-pro-ii-usb-playback",
        "audio": {
          "format_selection": "automatic"
        }
      },
      "failure_policy": {
        "partial_duplex": "keep_healthy_direction"
      }
    }
  ],
  "metadata": {
    "schema_version": 1,
    "record_type": "profile"
  }
}
```

### 15.0.1 Directional Audio Settings and Inheritance

Each directional path owns its effective audio settings. The optional hole-level `audio_defaults` object supplies inherited values, not a single mandatory duplex configuration. For each recognized field, resolution order is: explicit `forward.audio` or `reverse.audio`, then hole-level `audio_defaults`, then application defaults, then documented automatic selection. A directional override affects only its own path. An omitted override means inheritance; it does not mean a capability is unavailable or unsupported.

Direction-specific audio fields may include operating mode, format selection, sample rate, sample representation, channels, buffer preferences, and latency preference, subject to the separately documented field schemas. The example uses existing illustrative preference names; it does not define new concrete sample-format or buffer schemas. A simplex hole follows the same rule for its active direction and marks its unused reverse direction `not_applicable`.

Serialization must preserve the distinction between explicitly selected values and inherited values. GUI editing of one direction must not silently rewrite the other. User overrides must not silently revert to automatic selection. A previously saved hole-level `audio` object is a legacy representation; migration may map it to `audio_defaults` only when unambiguous, must preserve any directional overrides, and must report conflicts rather than guess. Any formal schema-version change follows the existing migration rules.

### 15.1 Profile Identity

`profile_id` is stable and independent of the profile filename.

Renaming a profile must not invalidate startup settings or references.

### 15.2 Hole Identity

`hole_id` is stable within its profile.

Renaming a hole must not change its identifier.

### 15.3 Device References

Profiles must reference devices using stable logical IDs.

They must not depend on:

- Device filenames.
- Windows endpoint IDs.
- USB port positions.
- Current enumeration order.

### 15.4 Simplex and Duplex

A hole is one logical connection, either simplex or duplex.

A duplex hole contains two associated directional paths.

A simplex hole contains one active direction.

Unused directions should explicitly indicate `not_applicable`.

```json
{
  "mode": "simplex",
  "forward": {
    "source_device_id": "usb-capture",
    "destination_device_id": "main-speakers"
  },
  "reverse": {
    "status": "not_applicable"
  }
}
```

### 15.5 Connection Capacity

The active-hole limit is:

- 1–4 holes: Normal operating range.
- 5–8 holes: Extended range with a nonblocking performance advisory.
- More than 8 active holes: Not supported in v0.1.

A simplex hole and a duplex hole each count as one logical hole.

Saved inactive holes may exceed the active capacity limit. Capacity counts enabled holes in the armed plan, including waiting or recovering holes; a duplex hole counts once. This policy does not establish a universally achievable hardware performance level.

---

## 16. Failover Configuration

Failover references portable logical device IDs.

Example:

```json
{
  "failover": {
    "policy": "retain_current",
    "candidates": [
      {
        "device_id": "steam-streaming-playback",
        "priority": 1
      },
      {
        "device_id": "wireless-headset-playback",
        "priority": 2
      }
    ]
  }
}
```

Supported policies should include:

- `prefer_highest_priority`
- `retain_current`
- `manual`

Missing devices must remain referenced rather than being silently deleted.

The routing supervisor determines which candidates are usable.

Windows endpoint enumeration alone must not be treated as proof that a wireless or remote transport is actually connected.

---

## 17. Runtime State

Runtime state must remain separate from user-authored configuration.

It may include:

- Last active profile ID.
- Whether routing was requested.
- Current endpoint bindings.
- Current hole states.
- Recent failures.
- Temporary recovery information.

### 17.1 Last Used Profile

The last used profile must be tracked by stable `profile_id`.

### 17.2 Startup Versus Recovery

Stored runtime state must not override explicit startup preferences.

In particular:

- Manual startup must not automatically start routing.
- Explicit Stop must cancel pending activation.
- Recovery must not restart intentionally stopped routing.

### 17.3 Persistence

Runtime state may be machine-local.

It must not become the only source required to reconstruct user profiles.

---

## 18. Manual Editing and Synchronization

Users may edit configuration files directly.

AudioHole must treat this as a supported workflow.

### 18.1 External Change Detection

Where practical, AudioHole should detect:

- Modified files.
- Added files.
- Deleted files.
- Renamed files.

Changes must be validated before being applied.

### 18.2 Active Configuration

Editing a profile file must not silently interrupt active audio.

AudioHole should distinguish:

- Saved configuration.
- Loaded configuration.
- Active runtime topology.

Detailed apply/reload behavior belongs in `PROFILES.md`.

### 18.3 Conflict Handling

If a file changes externally while the GUI has unsaved edits, AudioHole must not silently overwrite either version.

The user should receive an understandable conflict-resolution option.

### 18.4 Invalid Files

Malformed files should produce useful diagnostics including:

- Filename.
- JSON parsing location.
- Invalid field.
- Expected type or value.
- Suggested correction.

Do not silently replace malformed user files with defaults.

---

## 19. Safe Writes and Backups

Configuration writes must minimize the risk of data loss.

### 19.1 Atomic Replacement

Where practical:

1. Serialize the new configuration.
2. Write to a temporary file.
3. Validate the serialized result.
4. Safely replace the destination.
5. Preserve the previous version when appropriate.

### 19.2 Renaming

File renames must not overwrite existing records.

A failed rename should preserve the original file.

### 19.3 Portable Backups

AudioHole should support exporting a portable configuration backup containing:

- Application preferences.
- Profiles.
- Device records.
- Fingerprints.
- User notes.
- Relevant capability evidence.

Machine-local caches must not be required for restoration.

### 19.4 Recovery

A failed write must not leave an apparently valid but truncated configuration file.

---

## 20. Schema Versioning and Migration

Every authoritative configuration file must identify its schema version.

```json
{
  "metadata": {
    "schema_version": 1
  }
}
```

### 20.1 Validation

AudioHole must validate:

- Required fields.
- Data types.
- Enumerated values.
- Logical references.
- Unique identifiers.
- Supported schema versions.
- Cross-file relationships.

### 20.2 Migration

Migrations should preserve:

- Display names.
- Windows names.
- Notes.
- Logical IDs.
- Profiles.
- Fingerprints.
- Driver history.
- Relevant capability evidence.

### 20.3 Unsupported Future Schemas

An older AudioHole version must not silently rewrite files using a newer unsupported schema.

It should report the incompatibility and avoid destructive changes.

### 20.4 Unknown Fields

Unknown fields should be preserved where safely possible.

Unknown operational settings must not be silently interpreted as valid instructions.

---

## 21. Validation and Error Handling

Configuration errors should use consistent categories.

Suggested categories:

- `syntax_error`
- `schema_error`
- `invalid_value`
- `missing_reference`
- `duplicate_identifier`
- `filename_conflict`
- `unsupported_schema`
- `identity_ambiguity`
- `write_failure`

Errors must contain enough context for a human to correct them.

Example:

```json
{
  "category": "missing_reference",
  "file": "profiles/VR Audio.json",
  "field": "holes[0].forward.source_device_id",
  "value": "rodecaster-pro-ii-usb-capture",
  "message": "The referenced logical device could not be found."
}
```

These structured records supplement normal user-facing explanations.

---

## 22. GUI Integration

The GUI must use the same configuration model as the JSON files.

It must not maintain an incompatible parallel representation.

### 22.1 Device Editor

The device editor should prominently display:

- Display name.
- Windows name.
- Notes.
- Reported capabilities.
- Probed capabilities.
- Scan timestamps.

Advanced sections should expose:

- Fingerprint details.
- Driver information.
- Logical identity.
- Diagnostic history.
- Current endpoint binding.

### 22.2 Display Name Editing

Changing a display name through the GUI should rename the corresponding JSON file.

Filename conflicts and invalid characters must be validated before committing the change.

### 22.3 Notes Editing

Notes should use a multiline text field.

Their contents must be preserved without interpretation.

### 22.4 Unavailable Information

The GUI must translate standardized states into clear human-readable labels.

Examples:

- `unavailable` → Unavailable.
- `unsupported` → Unsupported.
- `not_tested` → Not tested.
- `inconclusive` → Inconclusive.
- `not_applicable` → Not applicable.
- `unknown` → Unknown.

Unavailable technical information must not appear as an unexplained blank field.

### 22.5 Driver Diagnostics

The GUI should allow users to inspect current driver information and relevant historical changes.

Driver information is diagnostic context, not a driver management interface.

### 22.6 Match Explanations

The GUI and diagnostics should be able to explain why a device was matched automatically or why manual confirmation is required.

---

## 23. Configuration Acceptance Tests

### 23.1 External Device Rename

1. Create a device record.
2. Rename its JSON file externally.
3. Reload the device database.
4. Verify `display_name` matches the new filename.
5. Verify `logical_id` remains unchanged.
6. Verify profiles remain valid.

### 23.2 GUI Device Rename

1. Rename a device through the GUI.
2. Verify the JSON filename changes.
3. Verify `display_name` changes.
4. Verify `windows_name` remains unchanged.
5. Verify notes and profiles remain intact.

### 23.3 Clean Windows Installation

1. Export portable configuration.
2. Install AudioHole on a clean Windows installation.
3. Import the configuration.
4. Connect the original devices.
5. Verify fingerprint-based recognition.
6. Verify profiles resolve without old Windows endpoint IDs.

### 23.4 Device Replacement

1. Create a profile referencing a logical device.
2. Replace the physical hardware.
3. Associate the replacement with the existing logical ID.
4. Revalidate relevant capabilities.
5. Verify the profile does not require reconstruction.

### 23.5 Identical Hardware

1. Connect two devices with identical model identifiers.
2. Verify AudioHole detects ambiguous matches when unique evidence is unavailable.
3. Verify it does not silently bind the wrong device.
4. Confirm a user-selected binding can be retained where appropriate.

### 23.6 Unknown Capabilities

1. Discover a device that does not report a requested capability.
2. Verify the field explicitly indicates `unavailable`.
3. Verify the GUI displays the corresponding label.
4. Verify no subsystem treats the unknown value as a supported capability.

### 23.7 Notes Preservation

1. Enter multiline notes.
2. Save and reload.
3. Perform an Update Scan.
4. Rename the device.
5. Export and import configuration.
6. Verify notes remain unchanged.

### 23.8 Scan History

1. Perform an Easy Scan.
2. Verify scan timestamps.
3. Perform an Advanced Scan.
4. Verify both scan types remain distinguishable.
5. Simulate a failed scan.
6. Verify the last successful scan remains identifiable.

### 23.9 Driver Change

1. Record current driver information.
2. Simulate or detect a driver version change.
3. Verify previous driver information is retained.
4. Verify the device retains its logical ID.
5. Verify affected probe evidence is flagged for revalidation where appropriate.

### 23.10 Invalid Configuration

1. Introduce malformed JSON.
2. Launch AudioHole.
3. Verify the error is clearly reported.
4. Verify the malformed file is not silently overwritten.
5. Verify unrelated valid configuration remains usable.

### 23.11 Duplicate Logical IDs

1. Create two records with the same logical ID.
2. Reload the database.
3. Verify AudioHole reports the conflict.
4. Verify neither record is silently substituted for the other.

---

## 24. Responsibilities of Related Documents

**`SPEC.md`** defines what AudioHole must do.

**`ARCHITECTURE.md`** defines the major components and their interactions, including device matching and runtime routing.

**`DEVICE_PROBING.md`** defines device discovery, experimental testing, and capability evidence.

**`CONFIG_FORMAT.md`** defines persistent file formats, naming rules, status conventions, identities, fingerprints, and serialization requirements.

**`PROFILES.md`** defines profile lifecycle, editing, activation, switching, and recovery behavior.

**`GUI.md`** defines how configuration and runtime information are presented to users.

**`DECISIONS.md`** records accepted design decisions and unresolved alternatives.

**`ROADMAP.md`** defines implementation milestones and acceptance gates.

**`AGENTS.md`** provides standing instructions to Codex and other development agents.

---

## 25. Guiding Principles

1. **Put human-relevant information first.**
2. **Use `display_name` for the user-facing name and filename.**
3. **Preserve `windows_name` as the Windows-reported name.**
4. **Provide an editable `notes` field near the top of device records.**
5. **Never confuse unavailable information with unsupported functionality.**
6. **Never use ambiguous blank technical fields.**
7. **Use multiple readable identification hints rather than relying on a single opaque fingerprint.**
8. **Keep stable logical IDs independent of filenames, drivers, and Windows endpoint IDs.**
9. **Preserve scan timestamps and relevant capability history.**
10. **Record driver information for diagnostics without making it part of permanent device identity.**
11. **Treat manual file editing as a supported workflow.**
12. **Keep portable configuration separate from regenerable machine-local state.**
13. **Never silently destroy user-authored configuration.**

**AudioHole's configuration should be understandable to someone who has never read its source code, and portable enough that moving to another computer does not mean rebuilding the entire audio setup.**

---

## Reconciliation notes — unresolved production schemas

The examples above retain their documented illustrative status. Owner approval of architectural direction does not finalize concrete fields that were previously left open. O-004 in `DECISIONS.md` must be resolved before production serialization of endpoint-specific format/mode/period requests, application queues, channel maps, incomplete holes, coupled duplex values, or failover placement. Do not infer new enums, defaults, or field locations from prose recommendations.

Preserve `audio_defaults` inheritance and explicit directional overrides. A direction can negotiate different source and destination formats or periods; requested preferences and effective runtime values must be distinguishable. The current one-direction preference examples do not prove identical endpoint parameters. Reported lists of rates, formats, and channels do not establish support for their Cartesian product. Evidence concerns a complete configuration in its recorded mode and environment.

Preserve standard JSON, explicit standardized information states, notes, stable logical IDs, unknown-field handling, and filename/display-name synchronization. An omitted inheritable override is different from an unavailable capability. Draft unconfigured holes remain permitted; their final field representation belongs to O-004.

Per-user configuration, portable deployment support, and separation from machine-local bindings are accepted. Exact directories, root selection, portable activation, and unwritable-root behavior remain Open under O-006. O-003 leaves the nondefault meaning of `close_to_tray` and `tray_status_enabled` unresolved; their illustrative presence does not authorize changing established close-to-tray behavior or hiding operational controls.

Capability provenance and verification remain separate. Store actual test context and preserve stale history without treating it as a current guarantee. Current fallback and processing observations belong to runtime status; they do not silently rewrite preferred endpoint requests or invent a persistent direct-transfer policy (O-001/O-005). Pending operations and countdowns are not reconstructed as authorization to resume routing.

Single-file atomic replacement does not imply a cross-file transaction. Preserve originals on failed renames and detect external-write conflicts. Case-insensitive collisions, reserved Windows names, case-only renames, and duplicate logical IDs require validation under existing naming rules. Summaries remain derived from validated authoritative records, never a source that overwrites those records. Exact implementation and backup policies remain subject to the existing integrity requirements.

### Provisional synchronization concepts — no schema addition

D-034 and O-005 remain Open, and O-004 still gates production persistence details. The proposed Automatic prototype default does not add a persistent default, field, or enum. Preserve D-053 inheritance and directional independence; any eventual preference schema requires separate approval.

Selected preference, actual processing, observed synchronization health, and endpoint mode are distinct concepts. Actual processing may be unchanged transfer, application-managed adaptive resampling, or Windows-managed rate adjustment; these descriptions are not accepted wire/persistence identifiers. Runtime mismatch/confidence, occupancy, ratio, transitions, discontinuities/recovery, and measured/estimated latency must remain distinguishable without inventing fields here. Retain standardized information states for unavailable/unknown observations.

Strict preservation is internal/experimental, not a required public v0.1 setting. Its candidate behavior rejects incompatible formats, forbids intentional resampling or frame alteration/insertion/duplication/dropping, and requires reporting plus explicit restart/rebuffer instead of silent compensation. No third public correction policy or finalized synchronization schema is introduced. See `DECISIONS.md` O-005 and `ROADMAP.md` → Expanded synchronization experiments.

### Approved personality data boundary

Optional commentary files use the dedicated presentation-only format documented in
[personalities/README.md](personalities/README.md). They do not replace portable
profile/device JSON, machine-local bindings, or production settings. The companion
`personalities/` directory is relative to the executable for this GUI testing build;
this does not resolve O-006 storage roots for production routing configuration.
Theme and selected personality ID now persist in isolated Preview Build preferences; DFA slider/history remains local session presentation state.
The owner-requested **Hold expressions between events** checkbox also persists
there as optional boolean `hold_expressions` (default `false`) in preference schema
version 1. Older files need no migration; invalid types are rejected and externally
edited files retain the existing conflict protections. This is a presentation-only
Preview Build preference, not a production device/profile schema decision.


### Owner-authorized provisional Test persistence

The GUI refinement milestone explicitly authorizes a replaceable, versioned JSON
provider around current in-memory records, isolated from Preview and production.
See [PROVISIONAL_TEST_PERSISTENCE.md](PROVISIONAL_TEST_PERSISTENCE.md) for actual
implemented format, roots, limits, atomic replacement/conflict behavior and gaps.
Test Mode performs real disk load/save across restarts. This does not resolve
O-004/O-006, implement the production per-device filename contract, or define
capability/engine DTOs. A future production provider also needs richer domain
records. The GUI contains no JSON serialization logic.

The supplied historical examples are reviewed in
[docs/schema-review/REVIEW.md](docs/schema-review/REVIEW.md), with field dispositions,
gaps, candidate core schemas/samples, compatibility plan and owner questions.
Those files are a **proposal**, not approved production schemas. They do not alter
the current Test provider, trigger an import or authorize destructive migrations.
Existing identity, filename, provenance, status and inheritance requirements above
remain in force for future production work.

### Owner-authorized scanner integration in Test Mode

[SCANNER_TEST_MODE.md](SCANNER_TEST_MODE.md) supersedes the earlier statement that
Test Mode has no scanner or per-device filename implementation. Real scanner
records use the handed-off device-v2 format in isolated `test/devices/` folders;
profiles/legacy devices retain their replaceable provisional provider. Machine-local
bindings/session evidence are separate. Optional boolean `hidden` defaults to false
and is preserved beside names/identity in each scanner record. This is the
owner-requested device visibility preference, not a production profile schema
approval. Filename rename preserves logical identity. No destructive migration or
resolution of O-004/O-006 is authorized by this increment.

The Devices editor can now save scanner-owned name/notes changes through an
isolated `EditMetadata` operation. The canonical filename remains the display-name
authority; identity, hidden status, unknown fields and references are preserved.
The service checks the originally loaded path/bytes, refuses filename collisions,
and atomically replaces contents with a checked rename rollback on write failure.
An interrupted rename/content sequence is recoverable by Reload, but is not a
multi-file transaction. See SCANNER_TEST_MODE.md for the failure contract. Scan
duration controls display decimal seconds; existing JSON/request fields remain
integer milliseconds. No device/profile schema migration is introduced.

### Current GUI refinement preferences and scan observations

The isolated build-preference document (schema 1) adds optional integer
`animation_loops` (1–20, default 1) and boolean `loop_until_event` (default false).
Existing `hold_expressions` remains independent and remembered when continuous
looping temporarily takes precedence. Invalid types/ranges preserve the original
file; older documents need no migration. These remain provisional build preferences.

Scan requests retain integer milliseconds: `system_timeout_ms: 0` now explicitly
means unlimited **overall** Deep batch time. Individual attempt/device limits remain
finite. Attempt observations may include `initial_checkpoint_ms` and `worker_wall_ms`;
the bounded session journal includes monotonic elapsed/stage and final-checkpoint
wall times. These are measured processing timings, not audio latency guarantees.
No profile/device format is finalized. Explicit Clear device list intentionally
removes scanner identities/data; saved profile references remain and may require
rebinding. See GUI_REVISION_NOTES.md for confirmation and failure behavior.


### Scanner-backed provisional Test profiles (v2)

The owner clarified the control-side preparation boundary and minimal routing
intent: readable hole/profile names and notes, stable devices and format/settings.
The Test provider now writes ordered schema 2 and reads 1/2; first explicit v1 save
preserves an exact `.v1.bak` before upgrading. This adds readable device-name hints
and complete Windows descriptors without embedding scan histories or local bindings.
Names never override logical identity. Scanned, nonhidden devices supply new
choices; existing hidden/missing references remain preserved. Exact tuple/mode
matching happens in the shared control model, with a separate minimal typed
prepared request for a future engine. See
[PROVISIONAL_TEST_PERSISTENCE.md](PROVISIONAL_TEST_PERSISTENCE.md#scanner-backed-configuration-increment)
and [the Test example](docs/test-profile-example.json). Production O-004/O-006,
endpoint-specific negotiation and final file layout remain unresolved. No load-time
or destructive production migration is introduced.

### Provisional device/profile transfer bundles

Owner-authorized Test bundle transport v1 wraps the existing canonical Test
configuration (reader 1/2/3) and scanner device-v2 records. It preserves identities,
notes, hidden state and unknown fields; local Windows bindings and startup
permission are excluded. Imports preview and reject conflicting records, then
atomically commit profile references after new device records. This is not a final
production schema or a cross-file transaction. See
[CONFIGURATION_BUNDLES.md](CONFIGURATION_BUNDLES.md) for exact contents and failure
behavior, and the control-side audio-engine handoff boundary.


## Owner-authorized Advanced Tuning amendment

A per-hole Advanced Tuning policy gates retained manual overrides. New holes are
Automatic; disabling tuning ignores overrides without deleting them. Enabling it
makes explicit overrides eligible, while each Automatic parameter stays managed.
Validate applicable overrides before activation and preserve independent duplex
settings. Checkbox changes do not themselves apply or start routing. Requested,
saved and confirmed runtime values are distinct.

`advanced_tuning` and an `advanced_settings` object in the owner's example express
policy and readable organization, not final production serialization. O-004/O-006
remain Open. The implemented Test-only v3 compatibility rule and current directional
nesting are specified in [PROVISIONAL_TEST_PERSISTENCE.md](PROVISIONAL_TEST_PERSISTENCE.md).
Legacy manual profiles are not silently converted to Automatic. Stable logical
IDs, unknown fields and inactive overrides must survive future migrations.


The owner additionally authorizes a Routing Engine request in profile JSON for
future engine comparison. The current Test field is per-hole `routing_engine`,
default `automatic`, with only Automatic offered. This is a policy placeholder,
not selection of a permanent backend or a completed benchmark mechanism.


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
