# Preview Build persistence (provisional v3)

Current Test profile storage uses individual record v2 and transfer wrapper v6. See
[PROFILE_BUILDER.md](PROFILE_BUILDER.md) for the current contract; earlier version
sections below remain compatibility history. Bundle envelope version remains 1.


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

## Test v3 shape and limits

One UTF-8 two-space-indented document, final newline:

```json
{
  "format": "audiohole-test-configuration",
  "schema_version": 3,
  "profiles": [],
  "devices": []
}
```

Arrays use the existing structs:

- Device: `id`, `display_name`, `windows_name`, `notes`, `capture`, `availability`,
  `formats` (complete `rate`, `channels`, `representation` tuples).
- Profile: `display_name`, `notes`, `id`, `holes`.
- Hole: `display_name`, `notes`, `id`, `advanced_tuning`, `routing_engine`, `enabled`, `duplex`, `forward`, `reverse`.
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
channels1..256, buffer0..100 ms (0 requests Automatic). Bounds are test-provider validation limits, not
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

Versions 1, 2 and 3 load. Loading never rewrites a document. The first explicit
save of a legacy document preserves its exact original bytes in
`configuration.json.v1.bak` or `.v2.bak` before emitting v3. A conflicting existing backup blocks the upgrade rather than being replaced.
Older applications reject v3, protecting the tuning-policy and Automatic-buffer semantics.
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

## Advanced Tuning policy (provisional Test v3)

Each hole has an explicit boolean `advanced_tuning` near its identity, before its
forward/reverse assignments. New holes use false. False resolves both directions
with Automatic format, mode and buffer; stored manual fields do not participate
in routing validation/preparation and are not erased. True makes each direction's
`settings` eligible; format `automatic: true`, mode `Automatic`, and `buffer_ms: 0`
remain independently Automatic. Unsupported applicable overrides fail validation
without falling back silently. Buffer 0 requests selection, not a zero-length
stream. Preview uses a labelled simulated 6-ms choice; Test has no applied engine
value. Preparation carries a zero Automatic buffer request to the future engine.

The existing `forward.settings` and `reverse.settings` objects are retained as the
direction-specific advanced data, avoiding an unnecessary nesting migration. This
is the provisional equivalent of the amendment's illustrative `advanced_settings`
object, not approval of production field names or nesting. Manual format bytes,
notes, IDs and unknown fields survive toggling, switching and Test restart. Cached
formats marked Automatic may refresh from effective-policy evidence on a full
save; they are not manual overrides or runtime confirmations.

For legacy v1/v2 holes missing the flag, loading sets it true, preserving the old
behavior where saved tuning always participated. An explicitly present boolean is
honored; v3 requires the flag and rejects missing/wrong-type values. No load-time
migration occurs. Explicit save emits v3 with a non-overwriting legacy backup;
external-edit and conflicting-backup protection remain. Bundle merges normalize
legacy policies before combining records while preserving unknown data and IDs.

Checkbox changes stage editor policy only. Save does not reconfigure a live copy;
profile Apply/Activate remains explicit. A directional buffer Apply cannot opt an
automatic live route into Advanced Tuning or bypass an unapplied policy change.
Production O-004/O-006 and actual negotiation/synchronization remain Open.

### Routing Engine request

Test v3 holes also store `routing_engine: "automatic"` next to
`advanced_tuning`. The Advanced Tuning selector displays **Routing Engine** with
**Automatic** as its sole offered option. It is a per-hole request shared by its
two directions, not an initialized engine or performance claim. Missing fields
load as `automatic`, including legacy files. Nonempty strings up to 64 UTF-8 bytes
round-trip unchanged, preserving future engine identifiers. A choice unavailable
in this build is not offered as a functioning option: its saved value is explained
and retained until the user explicitly chooses Automatic. It blocks activation
when Advanced Tuning is enabled; with tuning disabled it remains stored and the
effective request is Automatic. Wrong types/empty values reject the document
without modifying it. Profile bundle transfer retains the selector and policy.
No permanent engine implementation or public production schema is selected.


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
