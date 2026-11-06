# Device and profile bundles — provisional Test transport v1

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

`configuration.json` reuses the Test provider's version 1/2 format; new app saves
use version 2. The legacy `devices` array is empty in exports because separate
scanner files are authoritative. Profile/hole names, notes, independent directions,
stable device IDs, readable hints and complete format descriptors round-trip,
including unknown fields. See [the sample](docs/test-profile-example.json) and
[persistence contract](PROVISIONAL_TEST_PERSISTENCE.md). Import uses the same
validation and adds profiles while preserving other local configuration.

## Audio-engine handoff

Read configuration as routing **intent** on the application control side. Resolve
IDs through the device registry and local binding service; names never select
devices. `prepare_profile` demonstrates conversion into a minimal typed
`PreparedProfile`: enabled directions, hole identity, device IDs, exact format/mode
and requested buffer duration. The engine receives prepared requests, not scan
matrices/history, and still initializes and validates streams at runtime.

Automatic settings are preferences, not negotiated values. Drafts, missing or
ambiguous devices and unknown layouts must not become guessed successful routes.
Current preparation covers exact common tuple/same-mode paths; independent endpoint
conversion/mapping remains future work. `pcm_s24_in_32` preserves 24 valid bits in
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
  atomically committed last. A v1 upgrade retains the original `.v1.bak`. Differing
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
