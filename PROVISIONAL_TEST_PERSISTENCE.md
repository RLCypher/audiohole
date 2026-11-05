# Preview Build persistence (provisional v1)

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

## Test v1 shape and limits

One UTF-8 two-space-indented document, final newline:

```json
{
  "format": "audiohole-test-configuration",
  "schema_version": 1,
  "devices": [],
  "profiles": []
}
```

Arrays use the existing structs:

- Device: `id`, `display_name`, `windows_name`, `notes`, `capture`, `availability`,
  `formats` (complete `rate`, `channels`, `representation` tuples).
- Profile: `id`, `display_name`, `notes`, `holes`.
- Hole: `id`, `display_name`, `notes`, `enabled`, `duplex`, `forward`, `reverse`.
- Direction: `source`, `destination` stable logical IDs and `settings` containing
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
