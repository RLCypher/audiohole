# Preview Build persistence (provisional v2)

The owner authorized real isolated Test persistence while production schemas remain
Open (O-004/O-006). This provider is **not a production format, historical importer,
or resolution of those decisions**. See the separate
[historical schema review](docs/schema-review/REVIEW.md) for candidate production
records and owner questions. Historical files are never automatically imported.

## Modes and roots

Normal startup offers **Preview Mode** and **Test Mode** in a Preview Build dialog.
Both Settings checkboxes default enabled. Turning off the dialog starts the saved
next-launch mode; changing mode in Settings never swaps an active backend.

For this Preview Build only, wxStandardPaths' per-user *local* AudioHole directory
contains `PreviewBuild/preferences.json`, `PreviewBuild/test/configuration.json`
and a reserved `PreviewBuild/preview/` namespace. The absolute Test path is visible
in Settings. No production root is read or written. Root/portable deployment for
production remains O-006. Smoke tests use private temporary roots or memory-only
preferences and never user data.

Preview uses the original simulated devices/profiles and session edits, with
silent Test Beep, simulated scans/failures and no hardware access. It writes no
profile/device files, including into Test. Test starts empty on a new root and
loads only its own records. It supports profile CRUD, independent directional
settings and saved device label/notes edits. Device records can be added/edited
externally while AudioHole is stopped. These aggregated legacy device entries remain
unverified. The later [scanner integration](SCANNER_TEST_MODE.md) adds separate real
device-v2 records; they are never flattened into this file.
Test disables route/monitor/beep/live profile Apply and omits Simulate Errors entirely.

Startup preferences, theme, selected personality stable ID, the presentation-only
`hold_expressions` boolean (optional, default false), notification setting,
preview startup selection/delay/target are stored separately. A missing personality
does not erase the desired saved ID. DFA value/history stays transient. Startup
preview preferences do not automatically resume a route after restart. Close still
hides to tray; explicit Exit stops and closes. Restart confirms unsaved profile
edits, destroys the frame/timer/tray, releases the instance guard/COM, then launches
the absolute executable. Its child waits for the parent process to exit before
acquiring the guard or loading files. Launch failure leaves saved files intact.
There is no process detach/IPC service or restart of pending routing operations.

## Replaceable boundary

`ConfigurationPersistence::load(ConfigurationRecords&)` and
`save(const ConfigurationRecords&)` separate serialization from the configuration
controller and GUI. `ConfigurationController` publishes saved records only after
a successful provider commit. Failed disk writes preserve the saved snapshot and
report an error. Preview keeps `SimulatedController`. The application-controller
decorator fronts either backend; GUI controls never parse JSON.

A future production serializer can replace the provider. Richer domain records
will also be needed for production fingerprint/evidence/inheritance/request
concepts; the current fixture structs do not specify those schemas.

## Test v2 shape and limits

One UTF-8 two-space-indented document, final newline:

```json
{
  "format": "audiohole-test-configuration",
  "schema_version": 2,
  "profiles": [],
  "devices": []
}
```

Arrays use the existing structs:

- Device: `id`, `display_name`, `windows_name`, `notes`, `capture`, `availability`,
  `formats` (complete `rate`, `channels`, `representation` tuples).
- Profile: `display_name`, `notes`, `id`, `holes`.
- Hole: `display_name`, `notes`, `id`, `enabled`, `duplex`, `forward`, `reverse`.
- Direction: readable `source_name`, stable `source` ID, readable `destination_name`,
  stable `destination` ID and `settings` containing
  `automatic`, `format`, `buffer_ms`, `mode`. Both directions always round-trip,
  including an unused reverse draft; no collapsing duplex settings.

IDs are nonempty and unique within each record kind (hole IDs across profiles).
Reads never regenerate them. New/duplicate records get fresh IDs. Unresolved
references and empty endpoint choices are retained as drafts, never guessed or
activated. `availability` explicitly accepts available, unavailable, ambiguous,
untested, unknown. These are current model spellings; they are not a new production
status vocabulary. Empty formats means **no recorded tuples**, not unsupported.
This aggregated legacy provider cannot establish tested support. Additional capability
evidence in this file can round-trip as unknown extension data but is not interpreted;
the separate scanner record/evidence view has its own documented limits. Current primitive
fields cannot express every production status object; this is a documented limit.

Maximum document 2 MiB, nesting16; devices512, profiles256, holes/profile128,
formats/device256. IDs128 UTF-8 bytes, notes/names64 KiB; format rate1..768000,
channels1..256, buffer1..100 ms. Bounds are test-provider validation limits, not
hardware capabilities. Invalid fractional/overflow numeric fields and duplicate
JSON keys/IDs/format tuples are rejected. Invalid/newer documents block saves and
remain untouched. Startup reports the problem; repair externally and restart.

Unknown object fields survive edits recursively; record arrays match stable IDs,
format arrays match their complete tuple. Deliberately deleting a record deletes
its extensions too. Recognized field values and ordered arrays retain semantics;
whitespace/key order is not byte-preserved. External changes are detected against
the exact bytes loaded, so they cannot silently be overwritten by a stale editor.

Saves validate before writing. A unique exclusive temporary file in the same
directory is written and flushed, followed by an external-change recheck and
atomic replacement (`MoveFileExW` with replacement/write-through on Windows,
rename on POSIX). Original remains on validation/write/replace failure. There is
no delete-before-rename. Direct symlink files/parents are rejected. This is
practical single-file protection: it does not provide a filesystem compare-and-swap
against an arbitrary external writer racing in the final recheck/replace window,
a cross-file transaction, a production backup policy or a power-loss guarantee.
Future production storage must address those separately.

Device filename/display-name synchronization cannot exist in this aggregated Test
v1 document; editing `display_name` works and preserves IDs. Per-device files,
external filename renames, Windows-name collision rules, matching/provenance,
audio_defaults inheritance, approved request semantics and migration remain future
production requirements. They are retained in CONFIG_FORMAT and the review, not
silently replaced by this adapter.

## Validation

Portable tests cover real file round trips, reconstructed controllers/providers,
Unicode/notes/independent duplex values, stable IDs, external edits/unknown fields,
malformed/newer/duplicate/fractional/overflow files, failed saves, preference
restart/defaults, Preview/Test separation and production sentinels. Native
`Gui.TestModeSmoke` exercises Test GUI Save and reconstructs a disk-backed
controller in an isolated root, without hardware. It does not prove OS-process
restart/Explorer/tray behavior. Use MANUAL_GUI_CHECKLIST for that Windows gate.


The small buffer Apply now saves just the selected saved direction's preference,
including in Test Mode. It enables for valid changed values and disables after
success; other unsaved profile edits remain staged. Saved Test values round-trip
through the same provider with no schema change. It never reports a Test engine
buffer as applied. Preview additionally reports simulated per-direction confirmation.

Optional device boolean `hidden` defaults to false in legacy Test v1 entries.
Scanner-owned visibility instead lives in each canonical device JSON, as documented
in [SCANNER_TEST_MODE.md](SCANNER_TEST_MODE.md). No production schema is finalized.


## Scanner-backed configuration increment

The owner clarified that profiles carry routing intent; discovery, capability
analysis and preparation belong on the application control side, usable by GUI
and a future CLI. The engine must still open/initialize requested streams and
report actual runtime outcomes. No real routing engine is introduced here.

The shared `configuration_devices` projection offers only scanner-owned Test
records which are not hidden. Preview retains its fixtures. Saved hidden or
missing bindings remain intact and visible; free-text searches never substitute
identities. Duplicate device names are disambiguated with logical IDs. Readable
name hints refresh on full Save, but never select a device or override an ID.
Fingerprints and machine-local Windows bindings remain in their authoritative
scanner stores, rather than stale copies in each profile. Back up those portable
device records alongside profiles to preserve identity across machines.

The worker projects exact `format_queries` into typed format/mode/support data.
Only affirmative S_OK support counts; S_FALSE closest offers, rejected/unknown
queries and a mix/configured format alone cannot authorize a request. Exact
source/destination descriptors must match in the same mode. Valid/container bits,
channel count, mask, tag, subformat, block alignment and byte rate are preserved.
No Cartesian products, guessed stereo layouts or promotion of short Deep tests
to proven route reliability occurs. Automatic prefers common Shared tuples, with 48 kHz float first within that
mode; otherwise a common Exclusive tuple can be prepared. Explicit overrides are retained when evidence
changes and visibly marked as not supported by current evidence.

`prepare_profile` builds a separate typed `PreparedProfile` with only enabled
paths, hole IDs, stable source/destination IDs, complete formats, concrete mode
and requested buffer duration. Preparation rejects missing/ambiguous/unavailable
bindings or incompatible requests and never publishes a partial result. It does
not scan, serialize, touch widgets or open audio. The future supervisor must turn
IDs into unambiguous local bindings before engine initialization. Saved notes,
display hints, disabled/unused drafts and scan matrices never enter this request.

### Provisional v2 format and compatibility

Version 2 preserves the aggregate Test file and replaceable provider; it does not
implement a final production per-profile directory layout. Profiles are emitted
before legacy device entries; names and notes precede identity and directions.
Each format now has optional complete `wave` detail; `wave: null` explicitly means
an exact Windows descriptor is unknown (legacy/fixture or incomplete draft), not
that arbitrary Windows parameters are supported. Inside a known descriptor,
unknown channel masks use `{ "status": "unknown" }`; numeric zero remains distinct.
Automatic's saved format is the last selected candidate/default, not a runtime
negotiation or a guarantee: preparation resolves against current scanned evidence.
No matching tuple leaves the draft saveable and the editor explains the gap.
See [the sample](docs/test-profile-example.json) for the emitted relevance order.

Both v1 and v2 load. Merely loading v1 never rewrites it. Its first explicit save
preserves its exact original bytes in `configuration.json.v1.bak` before emitting
v2. A conflicting existing backup blocks the upgrade rather than being replaced.
Older v1-only applications reject v2, protecting its exact-descriptor semantics.
Unsupported newer versions and malformed descriptors block saves; unknown fields
still survive recognized edits. Full Save refreshes display hints/cached automatic
candidates; the small buffer Apply intentionally changes only its target buffer.

This remains an owner-authorized **provisional Test** increment. Production
O-004/O-006 remain open: endpoint-independent formats/modes/periods, conversion and
channel maps, inheritance, failover and final storage layout need their existing
approval/validation gates. Current same-tuple/same-mode choices cover a conservative
subset; differing channel layouts such as 10-channel RODE output versus a stereo
source require explicit mapping/conversion work rather than fabricated compatibility.
Buffer milliseconds remain a requested preference; no hardware period, latency or
stability recommendation is inferred from that value. Preview behavior is retained.
