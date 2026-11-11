# Device and profile bundles — provisional Test transport v1

Current Test profile storage uses individual record v1 and transfer wrapper v5. See
[PROFILE_BUILDER.md](PROFILE_BUILDER.md) for the current contract; earlier version
sections below remain compatibility history. Bundle envelope version remains 1.


This implements the owner's development transfer/backup workflow. It is an envelope
around existing Test records, not approval of the final production schema, storage
roots or routing-engine API (O-004/O-006).

## GUI workflow

Devices has **Export devices… / Import devices…**; Profiles has **Export profiles… /
Import profiles…**. Test Mode uses the owned background service. Preview fixtures
cannot be imported into Test through these controls. Save or revert pending edits
first. Export uses all saved profiles and holes, not just the viewed profile.

Export destinations must be new filenames. Imports validate the complete archive
and show a default-No confirmation preview. Identical records are reused; differing
records with the same ID, duplicate hole identities or conflicting filenames reject the import. No
replacement IDs, silent note merges or destructive “replace everything” option are
provided. Resolve conflicting records explicitly before retrying.

Display names remain filename-authoritative. Names, notes, hidden status, stable
IDs, portable fingerprints, capabilities and available raw histories travel together.
Local Windows bindings, routing state, startup preferences, pending operations and
consent do not. An Update scan resolves imported identities against local hardware;
ambiguous/missing devices remain explicit. Import neither scans automatically nor
starts routing. Retained identical devices' existing local bindings stay local.

## Archive contents

```text
manifest.json
configuration.json                    # Profile bundles only
devices/recording/<display name>.json  # Authoritative device-v2 records
devices/playback/<display name>.json
probe_history/<attempt ID>.json        # Available histories for included devices
```

Device bundles include all scanner-owned devices, including hidden/unavailable
ones. Profile bundles include all saved profiles and their referenced devices,
including hidden devices already referenced by saved/draft directions. Hidden
devices remain excluded from new configurator choices. Missing device records are
listed in `unresolved_device_ids`; empty/missing bindings remain drafts. Missing
raw history files are not fabricated; embedded device evidence remains available.
This format is distinct from scan-analysis ZIPs.

Illustrative manifest:

```json
{
  "format": "audiohole-configuration-bundle",
  "schema_version": 1,
  "kind": "profiles",
  "profile_format": "audiohole-test-configuration",
  "device_schema_version": 2,
  "unresolved_device_ids": [],
  "runtime_bindings_included": false,
  "startup_authorized": false,
  "files": [
    "configuration.json",
    "devices/recording/Desk microphone.json",
    "devices/playback/Headphones.json"
  ]
}
```

`configuration.json` is the aggregate Test v4 transfer representation. Imports also
accept legacy v1/v2/v3 and normalize their retained override policies. Application
storage uses named per-profile v4 files under `profiles/`; the exporter assembles
them without including local bindings. See [PROFILE_BUILDER.md](PROFILE_BUILDER.md).

## Audio-engine handoff

Read configuration as routing **intent** on the application control side. Resolve
IDs through the device registry and local binding service; names never select
devices. `prepare_profile` demonstrates conversion into a minimal typed
`PreparedProfile`: enabled directions, hole identity, device IDs, independent
endpoint preferences, mapping and requested buffer policy. Test paths explicitly
mark runtime negotiation; their legacy format member is advisory. The engine receives prepared requests, not scan
matrices/history, and still initializes and validates streams at runtime.

Automatic settings are preferences, not negotiated values. Drafts, missing or
ambiguous devices and unknown layouts must not become guessed successful routes.
Preparation accepts independent endpoint preferences without requiring identical
cached tuples. Actual conversion, mapping and synchronization remain engine work. `pcm_s24_in_32` preserves 24 valid bits in
a 32-bit container and differs from packed 24-bit PCM. This Test build still has
no real routing backend. The bundle does not finalize production configuration.

## Validation and failure behavior

- Supported versions/kinds, listed JSON payloads, CRC/decompression and canonical
  device/profile validation are required. Duplicate entries/JSON keys, traversal,
  absolute/drive paths, links/reparse points and encrypted entries are rejected.
  Expanded size is bounded to 128 MiB/8192 files; each JSON is at most 16 MiB,
  with the existing stricter 2 MiB profile-document limit.
- Preview retains immutable archive bytes and destination preconditions. Confirmed
  import rechecks archive bytes, inventory paths and exact destination bytes. Any
  change requires a fresh preview. The GUI blocks racing profile edits, and the
  controller rejects configuration commits during bundle operations.
- Validated new device/history files are atomically written first; profiles are
  atomically committed last. An explicit legacy save retains `.v1.bak` or `.v2.bak`. Differing
  existing device/history files are never replaced. On a caught write failure or
  cancellation, newly created files are removed only if their bytes still match
  the import. Concurrently edited files are retained and reported.
- This is not a filesystem-wide transaction: a process/power failure before the
  profile commit can leave additional device records. Prior profiles remain
  intact. Check any reported residue and retry through preview. Empty staging
  directories and lock files are not authoritative configuration.
- Success reloads the provider/snapshots and ID-allocation bookkeeping. Subsequent
  saves retain external-edit detection. Import/reload never authorizes audio.

Widgets perform file selection and confirmation. ZIP parsing, validation,
serialization and writes live in `model/configuration_bundle.*`, invoked by the
owned worker and reusable by a future CLI. Tests cover round trips, restart,
unknown fields, hidden devices, conflicts, edits after preview, cancellation,
write-failure rollback, unsafe archives, idempotence and background publication.


Profile payloads now use Test configuration v3 with per-hole `advanced_tuning` and
zero meaning Automatic for buffer requests. Import accepts legacy v1/v2 and v3;
legacy missing tuning flags retain the old active-manual policy before merging.
No new bundle transport version or production persistence contract is introduced.
Retained inactive overrides travel with profiles; actual engine observations do not.

Profile payloads also retain the per-hole `routing_engine` request (default
`automatic`). Inactive future choices survive import/export; current request
preparation admits only Automatic, without initializing an engine.


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
