# Historical configuration review and candidate schema

Status: **proposal for review; not an approved production format or migration**.
Inputs: user-supplied `Example device schema.json` (1,863 bytes) and
`Example profile schema.json` (3,090 bytes), both tagged string version `"0.1"`.
These are example instances, not JSON Schema validation documents or proof of
past runtime behavior. No importer runs and no existing user files are changed.

## Evidence and current implementation

Compared with [CONFIG_FORMAT](../specifications/CONFIG_FORMAT.md) §§2–12, 16–23 and its
reconciliation notes; [DEVICE_PROBING](../specifications/DEVICE_PROBING.md) §§2–8, 10–13 and
execution-boundary amendment; [PROFILES](../specifications/PROFILES.md) directional settings,
activation, inheritance, recovery and manual editing; [API](../specifications/API.md)
command serialization/state contracts; [ARCHITECTURE](../specifications/ARCHITECTURE.md);
[DECISIONS](../specifications/DECISIONS.md) D-053 and O-001–O-006; and the actual
`src/model/controller.h`, controller implementations, profile/device editors and
scan controls. The current branch has a simulated scanner command interface
`scan(enabled, advanced, scope, selected_device)`, progress and events. It has
**no production scanner result DTO, hardware fingerprint resolver or engine
telemetry interface to serialize**. The primitive `Endpoint::formats` list cannot
express the evidence required by the specifications. Scanner design prose is
stronger than the current fixture structs; neither is an approved serializer.
No other agent's scanner workspace was inspected or modified.

Current Test Mode serialization is a separate, explicitly named
`audiohole-test-configuration` v1 adapter around the existing structs. It is
useful for restart tests, **not** the candidate format below. In particular it
currently uses one configuration document and does not implement per-device
external filename rename behavior, endpoint evidence, inheritance, or production
matching. Substituting the future serializer also requires richer domain DTOs;
an isolated interface prevents GUI JSON dependencies but cannot conjure missing
model concepts.

## Legacy device inventory and mapping

Disposition is about the concept. “Revise” does not grant automatic conversion.
Paths under `identity.fingerprint`, `reported_capabilities`, `probed_capabilities`,
`scan_history`, `driver` and `metadata` follow existing document conventions.

| Legacy field(s) | Disposition | Candidate representation and reason |
|---|---|---|
| `schema_version` (`"0.1"`) | Revise | Integer `schema_version` inside `metadata`, plus `record_type`; old version is an import signature only. Never numeric-coerce arbitrary strings. |
| `display_name` | Retain | Human name, synchronized with device filename; never a reference key. |
| `windows_name` | Retain | Last Windows-reported label, independent of editable name. A missing observation uses a status object. |
| `notes` | Retain | Literal UTF-8 multiline user text, preserved across rescans/renames. |
| `logical_id` | Retain/revise location | `identity.logical_id`; immutable, database-unique, independent of file/name/binding. Existing nonempty IDs survive import. |
| `identity.endpoint_id` | Revise | Machine-local `device_bindings.json` entry keyed by logical ID; stale/missing bindings never establish identity. |
| `identity.container_id` | Revise | Optional observed identity hint with provenance. Stability/portability must be tested, not assumed. Proposed fingerprint extension only after resolver contract. |
| `identity.hardware_id` | Revise | Optional fingerprint evidence; singular legacy value may need multiple hardware IDs. Unknown cardinality remains unresolved. |
| `identity.interface_path` | Revise | Machine-local binding/diagnostic hint; not permanent portable identity. |
| `identity.manufacturer`, `product`, `serial_number` | Retain/revise | Fingerprint fields with value-or-status types. Serial namespace/provenance matters; manufacturer `not_tested` does not prove a failed lookup. Never synthesize a value. |
| `device.direction` | Retain/revise | `identity.direction`: canonical `capture`/`render`; legacy `playback` requires explicit vocabulary conversion. |
| `device.connection` | Revise | Optional reported transport evidence (`usb` etc.) with source; not the physical route or identity key. Enumeration vocabulary needs scanner agreement. |
| `device.state` | Unresolved/revise ownership | Probably endpoint presence/readiness, but no behavior evidence. A new runtime availability observation is separate; do not translate `not_tested` to connected/disconnected. |
| `device.is_default` | Revise | Machine-local runtime observation by endpoint role, not one portable boolean (console/communications/multimedia may differ). |
| `capabilities.recording.status`, `.playback.status` | Revise | One endpoint direction plus evidence collections. Opposite-direction `not_applicable` trees are redundant. Do not infer duplex hardware capability. |
| `capabilities.{recording,playback}.{shared_mode,exclusive_mode}.status` | Revise | Mode-specific reported/probed evidence with provenance and explicit states. “Can initialize” is distinct from verified operation. |
| `capabilities.{recording,playback}.{shared_mode,exclusive_mode}.formats` | Revise | Complete tested/reported tuples with mode/context, not independent rate/channel sets or a union of support claims. `not_tested` becomes a typed collection status. |
| `timing.clock_domain` | Unresolved | Source and semantics unknown; retain only as evidence after clock-source contract. Equal rates are not a shared clock. |
| `timing.minimum_buffer_frames`, `default_buffer_frames` | Revise | Endpoint/mode/format-specific reported period or tested constraints with units and provenance. A single device-wide value is insufficient. |
| `timing.measured_latency_ms` | Revise | Timestamped measurement with method, scope, uncertainty, configuration and environment, stored in probe history or runtime telemetry. Never a routing request. |
| `timing.drift_ppm` | Revise | Relative-clock observation for a configured pair over an interval; not an intrinsic timeless device property. Sign/reference convention unresolved. |
| `profiling.{easy_mode,advanced_mode}.status` | Revise | Scan result summaries with separate Easy/Advanced provenance; no automatic “supported” promotion. |
| `profiling.{easy_mode,advanced_mode}.last_run` | Retain/revise | `scan_history.last_easy_scan` / `last_advanced_scan`, UTC timestamp or status. Preserve last successful result separately from latest failed attempt. |
| `legacy_compatibility.friendlyName`, `.endpointGUID`, `.deviceType`, `.sharedFormats` | Remove canonical copies | Abandoned aliases duplicate other values. A future opt-in importer may inspect them and report conflicts; do not retain two authorities. Empty `sharedFormats` proves neither unsupported nor untested. |

## Legacy profile inventory and mapping

| Legacy field(s) | Disposition | Candidate representation and reason |
|---|---|---|
| `schema_version` (`"0.1"`) | Revise | Integer `metadata.schema_version` and `record_type:"profile"`. No automatic acceptance as modern v1. |
| `profileName` | Retain/revise | `display_name`, filename rules separate from stable profile ID (missing in legacy). |
| `display_description`, `description` | Unresolved/revise | Likely short description versus long notes. Recommend one `notes` field unless owner needs a distinct summary; don't silently concatenate/drop conflicting content. |
| `totalHoles` | Remove | Derived from `holes.length`; enabled count is a different derived quantity. Reject contradictory import diagnostics rather than trusting this count. |
| `firstCreated`, `lastEdited` | Retain/revise | Optional metadata UTC timestamps or explicit status; do not invent historic dates during conversion. |
| `lastRun` | Revise | Local operational history/event; a copied profile must not imply it ran on the new machine or should resume. |
| `autoStart` | Revise | Application startup preference referencing stable profile ID, startup mode and delay; does not authorize pending operations to resume. Legacy boolean alone cannot express Ready/delayed behavior. |
| `holes[]` | Retain | Ordered logical holes, stable hole IDs added; one duplex hole remains one hole. Drafts may be incomplete. |
| `holes[].holeName` | Retain/revise | Hole `display_name`; add independent `notes`, neither used as identity. |
| `holes[].enabled` | Retain | Desired configuration; no inference that it is currently running. Enforce <=8 enabled holes at activation, not by deleting saved drafts. |
| `holes[].protocol` | Revise | WASAPI is backend infrastructure, shared/exclusive are per-endpoint requests. Split endpoint mode request from backend selection. Final request fields O-004. |
| `holes[].formatNegotiation` | Revise | Desired selection policy at direction scope; duplicated by `format.selection`. One authority, explicit inheritance. Enum/default unresolved O-004. |
| `holes[].masterClock` | Unresolved | Could denote scheduling clock or clock correction authority. Existing destination pacing constraints do not prove historic meaning; do not convert `target` into a new sync policy. |
| `routing.mode` | Retain/revise | Simplex/duplex shape with independent forward/reverse; exact incomplete/coupled representation remains O-004. |
| `routing.direction` | Remove after explicit mapping | `source_to_target` redundant in named forward path. Reject unknown directions; don't guess reversed endpoints. |
| `routing.status` | Remove from preferences | Runtime supervisor state/evidence belongs in snapshots/events, not saved authorization. |
| `format.selection` | Revise | Same decision as `formatNegotiation`; conflicting old values require user resolution. |
| `format.sample_rate_hz`, `.sample_format`, `.channels` | Retain/revise | Complete requested per-endpoint tuples (units/layout/container bits/valid bits), distinct from negotiated values. `not_tested` is evidence, not an automatic selection. O-004. |
| `format.resampling` | Unresolved | Could be preference or observation. Separate desired synchronization/conversion preference from actual processing; O-005, no invented enum. |
| `format.verification` | Revise | Reference to evidence for full configuration, not a profile-wide guarantee. |
| `source.logical_id`, `target.logical_id` | Retain | Canonical direction references (`source_device_id`, `destination_device_id`); no lookup by mutable display name. |
| `source.display_name`, `target.display_name` | Remove canonical copies | Join device names by ID for display. An import report can retain original labels for conflict review. |
| `source.endpoint_id`, `target.endpoint_id` | Remove from portable profile | Move observed bindings to machine-local store; never overwrite logical references with endpoint IDs. |
| `source.direction`, `target.direction` | Remove duplicate | Validate referenced device capture/render direction; compatibility import checks rather than second authority. |
| `latency.mode` | Revise | Explicit directional request policy; relation to format/buffer Automatic needs O-004. |
| `latency.target_ms` | Retain/revise | Desired target, not guarantee. Define scope (application vs endpoint vs end-to-end), limits and rounding before serialization. |
| `latency.buffer_frames` | Revise | Distinguish endpoint period from application queue, source versus destination, requested versus actual. Unknown is not zero. O-004. |
| `latency.measured_ms` | Revise | Runtime/probe measurement with scope/method, not preference. |
| `clock_sync.strategy`, `.drift_compensation` | Unresolved | O-005 remains open; no permanent destination-paced/automatic serialized policy inferred from old spelling. |
| `clock_sync.measured_drift_ppm` | Revise | Runtime/probe pair measurement with reference, interval and uncertainty. |
| `advanced.{source,target}.exclusive_mode` | Revise | Explicit endpoint mode request; conflicts with `protocol` and `in/out.exclusiveMode` must be reported. |
| `advanced.{source,target}.buffer_frames` | Unresolved | `0` could mean default/automatic/invalid. Don't translate it until meaning is decided. Proposed period/queue requests distinguish units and selection. |
| `advanced.{source,target}.dynamic_control` | Unresolved | No current documented equivalent; could involve gain, buffering or other behavior. Preserve in archaeology/import report only. |
| `advanced.{source,target}.gain_control` | Unresolved | “endpoint” does not establish volume versus stream gain semantics. No new gain implementation authorized. |
| `advanced.{source,target}.midi_control` | Remove from current schema | No supported v0.1 MIDI requirement. If historically operational, owner can identify a future extension; no silent enablement. |
| `advanced.{source,target}.safety_limit_percent` | Unresolved | 85/100 could be gain, resource budget or another limit. Do not infer a limiter or clamp current audio. |
| `in/out.display_name`, `.endpointGUID`, `.mode` | Remove duplicate canonical copies | Same identity/binding issues as source/target. Use only an opt-in conflict-aware importer. |
| `in/out.channels`, `.format`, `.sampleRate` | Revise, conflicting evidence | Concrete `2/s16/48000` competes with main `format`'s `not_tested`. No precedence inferred. Future import preview must ask which is intended. |
| `in/out.buffer`, `.exclusiveMode`, `.dynamicControl`, `.gainControl`, `.midiControl`, `.safetyLimit` | Remove duplicate trees; resolve concepts above | Cannot discard relevant buffer/mode concepts, but two authoritative spellings are unnecessary. Ambiguous controls remain unresolved; preserve originals outside canonical writes. |

## Missing requirements and contradictions

- Stable profile and hole IDs; independent reverse configuration; hole notes;
  drafts with missing endpoint choices; directional inheritance (`audio_defaults`
  versus explicit override); channel layouts/maps; potentially different source
  and destination formats, endpoint modes and periods; application queue bounds.
- Fingerprint evidence with provenance, uniqueness, drift/revalidation and manual
  binding explanations; identical-device ambiguity and replacement associations.
- Driver/environment context; actual test stages (reported, accepted, operationally
  verified, failed, inconclusive), bounded test durations, retries, cancellation,
  timestamps, HRESULT/error context, and stale evidence. Three independent lists
  of rates/formats/channels cannot assert their Cartesian product works.
- Failover candidates, precedence and recovery intent (exact placement O-004),
  coupled duplex policy, transaction/rollback requirements, and desired versus
  effective state. Missing fields are not permission to invent these policies.
- Startup mode/delay and explicit Stop cancellation; preferences separated from
  capability evidence; reproducible GUI/future CLI parsing; version compatibility,
  unknown-field retention, corruption/conflict handling, read-only/newer files,
  multi-file rename recovery, backups, bounded parsing, collision validation.
- Local endpoint defaults, bindings and last-run observations separated from
  portable user data. Disconnect does not delete a device or its evidence.
- The samples conflict internally: `WASAPI_EXCLUSIVE` plus Automatic format;
  target/source flags repeat protocol; concrete legacy formats coexist with
  `not_tested`; `totalHoles` could drift; no unique profile/hole identity. These
  observations identify conflicts, not proof which old field won at runtime.

## Candidate schema and validation contract

The adjacent sample JSON documents are **candidate core v1**, not an importer or
an approved new production schema. They deliberately leave O-004/O-005 request
structures out. `forward`/`reverse` and empty draft references are a *proposed*
shape requiring approval; do not treat the sample as resolving those decisions.
Two type identifiers distinguish profile/device documents; integer version is
inside `metadata` to follow CONFIG_FORMAT examples. A separate local binding
schema has its own type/version. Do not support both root and nested versions.

Prefer existing ordinary-value-or-status-object convention, rather than adding
an elaborate wrapper to every scalar. Technical status objects have required
`status` from CONFIG_FORMAT's vocabulary and optional human-readable `reason`;
concrete values must pass the field's normal type/range checks. Empty notes are
valid; empty holes means a draft profile; an empty evidence array means a known
empty result only when its container status says so. No undocumented null/zero
sentinels. `not_tested` and `unknown` are not synonyms for `unsupported`.

Candidate core required shapes:

| Document | Required fields / constraints |
|---|---|
| Device | `display_name`, `windows_name` (text or status), `notes`, `identity` (`logical_id`, `direction`, `fingerprint`), `reported_capabilities`, `probed_capabilities`, `scan_history`, `driver`, `metadata` (`record_type:"audio_device"`, integer `schema_version:1`). Device direction exactly capture/render. |
| Profile | `display_name`, `notes`, `profile_id`, `holes`, `metadata` (`record_type:"profile"`, version1). Each hole needs stable `hole_id`, display/notes, enabled, routing_mode, forward; reverse only duplex. This draft shape is proposed, not approved. |
| Local bindings | `metadata` (`record_type:"device_bindings"`, version1), `bindings` keyed by stable ID; each contains endpoint ID/status and observation context. No profile preferences or permanent IDs generated here. |
| Settings | Separate from these documents; no legacy `autoStart` copied into each profile. Existing accepted startup/personality/interface concepts remain; exact storage root O-006 and close/tray false behavior O-003 remain undecided. |

Identifiers are nonempty immutable strings (do not require names or opaque
hashes); duplicate IDs are conflicts. Filenames derive from display names using
existing Windows rules, validated case-insensitively, including reserved names,
trailing dots/spaces and case-only renames. External filename stem wins on reload
for **display only** (already established in CONFIG_FORMAT §5.8), while record ID
preserves all references. Do not rewrite the renamed file merely to load it.
Simultaneous GUI/file edits require a conflict result and explicit reload/merge;
never silently pick the latest timestamp. Two files with one ID are quarantined
as a conflict; unrelated valid records remain loadable. GUI rename must preserve
the original if rename/content update fails. It needs a recovery journal or
carefully staged transaction; a single-file atomic replace is insufficient.

Evidence records should name mode, complete tuple, test stage/result, observation
UTC time, scan/test ID, configuration, driver/OS context, and error/measurement
metadata where available. `unsupported` requires evidence under the recorded
conditions. Merely failing to open a busy endpoint is inconclusive/failed, not
proof of universal unsupported status. Proposed richer evidence payload details
must be aligned with the actual scanner DTO when it exists. Samples intentionally
use explicit untested/unavailable states instead of fabricating evidence.

Round-trip means semantic preservation of recognized and unrecognized fields,
Unicode, notes, stable IDs, explicit statuses, order of ordered arrays and absent
inheritable overrides. It does not promise byte-identical whitespace/key order.
Generated files should follow human-relevant ordering in CONFIG_FORMAT, two-space
indentation, UTF-8/final newline. Unknown extension values are retained and never
executed. Unknown enum values in operationally meaningful fields prevent activation;
unknown fields must not silently enable a feature. Unsupported major versions
are read-only or rejected with originals untouched; no opportunistic downgrade.
Prefer additive optional fields within a major version, with documented defaults
only where approved. A version increment plus explicit migration is required for
changed meaning, renamed fields or required additions.

The future GUI and CLI must use the same domain validation/persistence service,
not independently parse these documents. Device evidence is scanner-owned;
user labels/notes/preferences are not overwritten during rescans. Runtime values,
current bindings and capability evidence do not rewrite requested preferences.
The Test provider's interface is a temporary stepping stone: richer record types
will be needed before any production serializer can implement this proposal.

## Compatibility and migration plan (not implemented)

1. Work on a separate copy under a user-chosen import root; inventory originals,
   versions, duplicate identities, name collisions and conflicting duplicates.
   Keep untouched backups with a manifest; no in-place historical migration.
2. Parse both old instances strictly and emit a human-readable dry-run mapping,
   unresolved values and suggested modern records. `"0.1"` is not automatically
   modern v1. Do not silently prefer `in/out` over `source/target` or vice versa.
3. Reuse valid stable device IDs. Missing profile/hole IDs need one-time generated
   IDs recorded in the import report and output, never regenerated on each load.
   Duplicate conflicting IDs require user resolution, not deduplication by name.
4. Preserve ambiguous legacy values in the import report/archive. They are not
   executable extensions. Do not copy obsolete aliases into the new canonical
   document. Require decisions before translating zero/default or sync/gain limits.
5. Normalize statuses only with known semantics; preserve missing/not_applicable
   timestamps explicitly. Old `playback` may map to `render` after direction
   validation. No capability becomes verified solely because it was in old JSON.
6. Validate the whole proposed import, referential integrity and filename rules;
   show a reviewable diff. Only explicitly approved imports write a new root.
   Commit with recovery support; generate summaries only from validated records.
   Never auto-start routing as an import side effect.
7. Future migration tests: external/GUI rename and case collisions; restart and
   unknown-field round trips; absent versus explicit overrides; malformed/newer
   records alongside valid ones; duplicate IDs; conflicts at write/rename;
   interrupted multi-file commit; Windows reinstall/stale bindings; identical
   devices; explicit unknown/unsupported; no Cartesian-product assumptions.

The provisional Test v1 adapter remains separately versioned. It is not silently
migrated into these samples, and these historical files are not loaded by it.

## Specific decisions for the owner

These questions do not block isolated Test persistence or independent GUI work.
Recommendations below are proposals, not accepted defaults.

1. **O-004 request shape:** prefer explicit per-endpoint format/mode/period
   requests within each direction, with separate application-queue preferences,
   or direction-level preferences expanded during negotiation? Recommend the
   former for truthful asymmetric source/destination representation. Approve
   exact fields/units/enums only after engine/scanner DTO review.
2. **Drafts/inheritance/duplex/failover (O-004):** approve a named forward/reverse
   draft shape, omitted override = inherit, explicit unconfigured endpoint status,
   and logical-ID failover lists? Coupled policy values and placement still need
   separate approval; samples below illustrate drafts only.
3. **Legacy controls:** what did `dynamic_control`, `gain_control:"endpoint"`,
   `safety_limit_percent`/`safetyLimit`, `masterClock`, and zero buffers actually
   do? If behavior cannot be recovered, recommend excluding these controls from
   initial production configuration while retaining their archaeology report.
4. **Descriptions:** do you need a short `display_description` separate from
   multiline notes? Recommend one notes field until there is a concrete GUI need.
5. **Old conflicting format settings:** should a future import ask per conflict,
   or adopt one documented source after validating the old implementation?
   Recommend explicit review; current evidence cannot justify precedence.
6. **O-005:** which desired synchronization/conversion policies should eventually
   be public, and what are their authorized failure/recovery semantics? No legacy
   `automatic` string resolves this. Continue existing bounded experiments first.
7. **O-006 storage/portability and integrity:** approve exact roots and portable
   activation, unwritable-root behavior, backup retention and multi-file recovery
   strategy before production writes. Recommend separate portable records and
   disposable machine-local bindings; filename rename semantics are already set.
8. **Scanner contract:** which agent/branch will own the next authoritative
   fingerprint/evidence DTO review? Finalize provenance, test result stages,
   period units and drift/latency scope with that interface rather than guessing.

No approval is requested to continue the already authorized isolated adapter.

Machine-readable Draft 2020-12 **candidate core** schemas accompany each example
(`*.candidate.schema.json`). The three examples were validated against them.
These intentionally permissive extension boundaries describe only the core shape,
not a complete production validator. Uniqueness of IDs, reference resolution,
filename rules, evidence semantics, inheritance and activation limits require
shared domain validation. Neither these files nor their schema version is loaded
by the running application. Approval of one does not imply approval of O-004/O-005
payloads or an automatic migration. `supported`/`unsupported` are valid explicit
information states even though these unknown-data examples supply no invented
support evidence.

### O-004 candidate request payload, separate from the core

`direction-requests.candidate.json` illustrates the explicit per-endpoint option
from question 1, **without approving its fields or enums**. It proposes requested
format and endpoint period separately for source/destination, and a separate
application queue. A manual period/queue would use an explicit unit and bounds;
these must be agreed with the engine rather than interpreting legacy `buffer:0`.
Automatic selection is explicit. Absent directional override would inherit a
hole `audio_defaults` request; an explicit Automatic override would cancel a
manual inherited request. This distinction must survive round trips. The example
is not copied into the core profile or the Test adapter. Channel-map and failover
structure remain intentionally unresolved; an unknown channel layout may block
activation rather than being guessed. Asymmetric requests do not assert that
conversion is available. Actual negotiated formats, periods, processing,
latency and clock health are runtime observations, never written back as requests.

Historical inputs are retained byte-for-byte under `legacy/` for traceability only:

- device.example.json SHA-256 `5971e085dae645e57e5d4960d4477a023d085aee90c37ebcf060b2e5c4b560cb`
- profile.example.json SHA-256 `0f7dacabc627847b375e8180b6c2d62db1440ba118ab1a5469014a24d5efb7ea`

These copies do not become configuration defaults or approved schemas.


Review validation: all retained historical and proposed JSON documents parse.
Three candidate core schemas pass Draft2020-12 schema checking and their respective
example validation. No runtime serializer, migration or scanner behavior is derived
from this validation. The separate implementation has93 passing portable tests in
each of Debug and Release, plus a successful Windows cross-build; native Windows
execution remains a separate acceptance gate.
