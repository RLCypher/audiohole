# Scanner integration in Test Mode

This increment integrates the scanner v0.4 source supplied in
`AudioHole-GUI-Agent-Handover-v0.4.zip`. The original source manifest and hashes are
recorded in [docs/scanner/imported-source.json](docs/scanner/imported-source.json).
The import was additive: root architecture/agent documents and historical hardware
examples were not substituted for the application's current requirements.

Test Mode now performs real Windows MMDevice/WASAPI discovery, persists rich
scanner records and can run consented Deep tests. It still has **no audio routing,
monitoring, Test Beep or live profile Apply backend**. Preview remains simulated
and session-only. There is no automatic fallback to simulated evidence.

## Scan operations

| Scope | Basic / Easy | Deep / Advanced |
|---|---|---|
| Full | Query reported capabilities of discovered endpoints; retain disconnected saved records | Basic Full, then bounded stream tests of selected active, unambiguous targets |
| Update | Query capabilities only for newly identified endpoints; refresh known-device names, driver/binding metadata and availability | Basic Update, then Deep only for the new selected endpoints; zero new endpoints means zero Deep tests |
| Selected device | Query the exact saved logical device selected on Devices | Basic Selected, then Deep for that same eligible target; never substitute another device |

The Devices page has separate Capture and Playback lists. Each list puts hidden
matches below `----- Hidden devices -----` only when there are hidden matches.
The divider has no logical identity and cannot be acted on. Right-click a device
(or use the Windows keyboard context-menu key / Shift+F10) for Update device,
Deep update device and Hide/Unhide. The two per-device scan actions are also
visible buttons. Deep actions share the Scanning tab's limits and consent path.

Deep defaults are shared mode, eight attempts per device, 2,000 ms stream duration,
10,000 ms attempt deadline, 120,000 ms per-device budget and 600,000 ms batch budget.
The expandable controls expose shared/exclusive/both and all those limits. The
scanner validates bounds before starting work. The attempt timeout must exceed
the requested stream duration by at least 1,000 ms. Consent identifies the exact
selected device, all eligible devices, or new devices only, and warns about sound
and disruption. Consent is per request, never a saved authorization.

**Refresh availability** enumerates identities/states, without capability queries,
new records or experimental streams. Debounced Windows device-change notifications
request this same metadata operation after any active job finishes; unattended
GUI smoke mode does not enumerate hardware. Startup only reloads saved records.
**Reload saved devices** reads external edits and filename renames without hardware
access. Ambiguous matching is reported; this increment does not offer an explicit
physical binding confirmation dialog. Resolve conflicting evidence explicitly;
no name-based automatic rebind is introduced.

## Data roots and file contracts

These paths are relative to the existing user-local `PreviewBuild` preference
root (see [PROVISIONAL_TEST_PERSISTENCE.md](PROVISIONAL_TEST_PERSISTENCE.md)):

```text
PreviewBuild/
  preferences.json                    # existing isolated build preferences
  preview/                            # separate Preview namespace
  test/
    configuration.json                # existing provisional profile/legacy device file
    devices/
      recording/<display name>.json   # authoritative scanner device-v2 records
      playback/<display name>.json
      recording_devices.json          # derived scanner summaries
      playback_devices.json
  test-local/
    device_bindings.json              # machine-local endpoint IDs, state, observation time
    sessions/<timestamp>-<id>/         # request, JSONL progress, reports, record snapshots
```

A scanner device is not flattened into the provisional `configuration.json`.
`DeviceInspection` is an immutable view beside the stable model ID; the file
serializer remains outside widgets and profile application logic. Profile saves
filter out scanner-owned devices and preserve their existing references. Legacy
Test devices remain distinguishable as unverified saved configuration.

The imported device-v2 format remains provisional for this integration. The only
additive user preference is optional boolean `hidden` (absent means false), near
the names/identity at the top of the canonical record:

```json
{
  "display_name": "Desk microphone",
  "device_name": "Original Windows name",
  "hidden": true,
  "notes": "User notes remain editable externally.",
  "logical_id": "capture-<existing stable identifier>"
}
```

This is an excerpt, not a complete record. The full reference is
[schemas/device-record-v2.schema.json](schemas/device-record-v2.schema.json).
Hiding does not change availability, delete evidence, alter live routes or change
logical identity. Hidden devices are not offered in new hole/profile or Test Beep
choices. A configured hidden/missing reference remains visible as field text and
is retained when saving unrelated edits; it is never replaced with another device.
Full/Update discovery still includes hidden hardware. Legacy Test device entries
also accept optional `hidden`; Preview changes last only for the session.

Display name is authoritative from the canonical filename stem. Rename that JSON
file externally, then Reload. Logical ID and the original Windows name remain
stable. Current Windows name/driver/state are separate observations. Scanner-owned
name/notes controls are read-only in this first integration; edit JSON notes
externally and Reload. The legacy editor remains editable. Unknown fields and
notes are preserved by checked atomic replacement; external conflicts and invalid
records fail visibly. Invalid reloads keep the prior usable inventory, explicitly
marked with a failed result, rather than replacing it with an empty inventory.
The inventory reader is bounded to 4,096 records / 128 MiB aggregate, in addition
to the imported individual JSON/depth limits.

This does not finalize production profile serialization, routing preference
schemas, storage-root selection or destructive migration. O-004/O-006 remain open.
Device-v1 normalization occurs in memory on Reload; an explicit subsequent scan or
visibility write may serialize the handed-off normalized device-v2 record with a
backup. Newer/invalid versions are refused. No bulk migration is run at startup.

## Evidence and exports

The selector on Devices exposes Overview, Identity, Reported formats, Deep evidence,
Driver, Scan history, Diagnostics, Device JSON and Local binding. Availability is
labelled **last observed** with its UTC timestamp, not a current stream guarantee.
Reported formats retain exact requested tuples and statuses; no Cartesian product
of independent lists is inferred. Unknown/unavailable/unsupported values and raw
HRESULT diagnostics remain inspectable. A driver change invalidates the currency
of old Deep evidence without discarding history.

**Known v0.4 limitation:** the handed-off Deep `verified` classification has weak
frame-progress validation. It is shown only as limited short-stream evidence, never
as Healthy/Recommended or proof of sustained operation, channel mapping, clock
synchronization, signal presence or endpoint limits. Busy/permission/policy denial
is not recast as unsupported. No recommendation is fabricated from these results.
The routing configurator's richer evidence-to-preference design is future work.

Each scan writes `request.json` (scope, depth, logical ID, budgets, consent, source,
UTC time and optional operating context), `progress.jsonl`, `basic.json`, optional
`deep.json`, `inventory.json`, canonical device snapshots, `result.json` and terminal
`completion.json`. Errors may prevent some earlier files, but terminal evidence is
attempted whenever a session directory exists. UI progress is sampled/bounded;
the separate JSONL has an explicit 64 MiB cap and reports truncation. Exports contain
identifiers, local bindings, notes and context; review before sharing. Export scan
bundle archives the most recent session to a **new** ZIP; originals and existing
archives are not overwritten. Clear diagnostics clears only the GUI log, not evidence.

## Thread/process boundary and limitations

`ScannerService` owns one joined worker and one active operation. The controller
queues typed commands, drains bounded updates on its control thread and correlates
terminal results by generation. Widgets neither enumerate hardware nor parse
scanner JSON. Overlapping operations are rejected; cancellation stays visibly busy
until cleanup completes. Shutdown requests cancellation and joins, never detaches.
Basic Windows enumeration/filesystem calls remain cooperatively cancellable and
may delay shutdown if an OS/driver call blocks; no hard Basic deadline is claimed.
Archive export is bounded by the imported ZIP limits but is not interruptible
mid-file. These limitations must be considered in native testing.

The private `audiohole-probe.exe --probe-worker` process is shipped beside the GUI.
It uses the handed-off restricted stdin/stdout protocol, Job Object cleanup and
attempt/deadline enforcement. It has no public scan command or fixture fallback.
Main GUI startup/icon resources remain unchanged. The automated scanner fixture
suite is a test-only target: **the GUI never invokes `run_test_suite`**, which
intentionally renames/omits fixture records. Packaging explicitly allows the helper
and audits both executables; test executables remain excluded.

## Validation and next hardware checks

Portable tests cover upstream scanner contracts, all six scope/depth paths using
explicitly labelled injected providers, exact targets, Update metadata semantics,
partial-enumeration absence safety, cancellation, per-record hidden preservation,
external filename/notes/unknown-field edits, malformed reload retention, real
profile/scanner separation, generation-correlated completion and non-overwriting
archive export. GUI smoke adds the two lists, hidden divider, hidden choice exclusion
and saved reference preservation. Test-mode smoke loads isolated data without
hardware scanning. A cross-build checks Windows compilation/linking only.

Native Windows CI must still run Debug/Release CTest and extracted package smoke.
Then collect in-situ evidence for Basic Full/Update/Selected, all three consented
Deep scopes, disabled/disconnected/returning devices, cancellation/close while busy,
external rename/reload, hidden persistence across restart and archive review.
Use the optional context field to identify other audio applications and physical
setup. Preserve exact tuples, driver versions, HRESULTs and observation timestamps;
do not infer unmeasured behavior from a green CI test.
