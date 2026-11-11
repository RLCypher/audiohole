# Profile builder and engine handoff

This is the current development contract, authorized by the owner after review
with the engine agent. It replaces the earlier Test v4 contract. The owner will
regenerate development profiles; compatibility with older development builds is
not an acceptance requirement. Production storage decisions O-004/O-006 and the
streaming backend remain open. Test Mode does not route audio.

## Canonical storage boundary

`profiles/<display name>.json` is an **individual profile record**:

```json
{
  "format": "audiohole-profile",
  "schema_version": 1,
  "profile": {
    "display_name": "Desktop audio",
    "notes": "User notes",
    "id": "stable-profile-id",
    "holes": []
  }
}
```

See [the complete example](docs/test-profile-example.json). Record version 1 is
independent of the **aggregate transfer wrapper** used inside profile bundles:
`audiohole-test-configuration`, `schema_version: 5`, `profiles: [...]`,
`devices: []`. The wrapper can also carry `profile_record_extensions`, an opaque
map of individual-envelope extensions keyed by profile ID. It is transfer
metadata, not routing intent. `validate_profile_record` validates individual
records; `validate_test_document` validates the aggregate. They are not
interchangeable. Unknown/newer record versions reject without rewriting files.

Profiles retain names, notes, stable IDs and ordered holes. Each hole retains
its name/notes/ID, `enabled`, `duplex`, `advanced_tuning`, `routing_engine`, and
independent `forward` and `reverse` directions. The GUI calls `reverse` Return.
Logical device IDs reference the separate scanned registry; names are readable
hints. Hidden devices are excluded from new choices, but existing references
survive hiding, disconnection and renaming. Profiles do not embed scan matrices,
current Windows bindings, negotiated formats, health or actual buffer values.

Basic configuration needs endpoint assignments and simplex/return selection
only. Missing devices can remain in valid saved drafts. Scan/deep-test success
is not a prerequisite for saving or preparing Automatic intent. Starting a route
still requires unambiguous local bindings and successful runtime initialization.

## Authoritative preferences

Each direction's `settings` writes **only `preferences`**, plus retained opaque
extensions. No duplicate `automatic`, `format`, `mode` or `buffer_ms` settings are
emitted. Current preferences never fall back to those obsolete fields, including
when properties or the entire preferences object are omitted.

```json
"preferences": {
  "common": {
    "sample_rate_hz": null,
    "channel_count": 2,
    "sample_format": null,
    "mode": null,
    "buffer_ms": null
  },
  "source": {},
  "destination": {},
  "channel_map": []
}
```

Each of `common`, `source`, `destination` accepts the same five keys. With
advanced overrides enabled, resolve **each property independently**:

1. Explicit endpoint value.
2. Explicit common value.
3. Application Automatic policy.

`null` means **inherit**, not unknown capability. Missing properties/objects and
empty strings or `Automatic` normalize to inheritance. Buffer zero also normalizes
to inheritance; it must never allocate a zero-sized buffer. Canonical output
writes null for inherited known properties. Rates are integer 1–768000 Hz;
channel counts integer 1–256; explicit buffers integer 1–100 milliseconds.
Access modes are `Shared` and `Exclusive`. Sample formats are `float32`,
`pcm_s16`, `pcm_s24_packed`, `pcm_s24_in_32`, `pcm_s32`. Malformed types,
fractional integers, invalid enums and out-of-range values reject separately from
Automatic. These are serialization limits, not promises about hardware support.

Explicit settings are **requirements**, with no silent fallback. Explain known
unsupported tuples and runtime initialization failures. Missing/inconclusive scan
evidence is not rejection. Native tags, GUIDs, block alignment and byte rate are
derived and validated from effective formats; users do not enter them. Channel
layout remains unknown until independently established.

## Independent policy controls

- **Use advanced overrides** (`advanced_tuning`): off retains saved preferences
  and maps but resolves all of them automatically. On applies saved values, with
  unspecified values inheriting. This includes channel mapping.
- **Show advanced tuning**: visibility only; showing/hiding never changes intent
  or a running route.
- **Reset to Automatic**: explicitly clears the selected direction's preferences
  and mapping, preserving assignments and the sibling direction.
- **Enabled**: enables the hole independently of overrides.
- **Routing Engine**: independent per-hole backend request. Only `automatic` is
  available. An unknown saved engine remains visible as unavailable and prevents
  activating that enabled hole even with overrides off. Choosing Automatic is
  explicit; no fallback engine is silently substituted.

Save persists intent. Apply remains a separate operation. Enter in an editable
configuration field saves through its existing Save action, never live Apply.
Notes are multiline and Enter inserts a newline. Directional buffer Apply saves
only that buffer preference, preserving other staged edits and the sibling.

## Channel mapping

`channel_count` is stream width, not channel selection. Maps use zero-based PCM
frame indexes in JSON. The GUI uses one-based pairs, e.g. `1:3, 2:4`.

- Empty `channel_map: []` means **automatic default index mapping**, not no audio:
  connect matching indexes up to the smaller channel count, silence additional
  outputs, and discard unmatched source channels.
- A nonempty map connects the specified indexes; unmapped outputs are silent and
  unmatched sources are discarded. One output has one source; a source may feed
  several outputs. This does not sum/mix channels.
- Indexes must be 0–255 and fit explicit channel counts; runtime validates them
  against negotiated counts. There are at most 256 routes.

This does not identify physical mixer buses, duplicate mono automatically, or
infer a multichannel downmix. Forward and Return, and each endpoint's requirements,
remain independent through preparation, failure and recovery.

## Engine boundary

`prepare_profile` builds typed requests on the control thread, leaving its output
unchanged on failure. Test requests carry `runtime_negotiation: true`, effective
`preferences`, inherited `source_requirements` / `destination_requirements`, and
`channel_policy` (`default_index_mapping` or `explicit_index_mapping`). Transient
legacy UI projection fields and the prepared `format` candidate are advisory;
only effective preferences impose requirements. Do not persist these candidates.

Automatic initially favors shared access and conservative engine-selected
buffering. It never silently escalates to disruptive exclusive access. The engine
must resolve endpoints independently, apply its supported conversion/mapping and
clock-synchronization policy, and publish actual formats, buffers, modes, backend,
health and selection explanations through runtime status. No engine or conversion
algorithm is selected by this GUI work. JSON, registry/scanner work, allocation
and GUI updates stay outside real-time callbacks.

## Filenames and preservation

Display names determine filenames; IDs determine identity. Windows-forbidden
characters/reserved names are sanitized. Filename collisions reject instead of
overwriting records. Renames preserve IDs, references, notes and unknown nested
extensions. New Profile/New Hole names receive the next free `(1)`, `(2)`, etc.

`.state.json` preserves aggregate extensions and legacy fixture data. Envelope
extensions survive rename and bundle transfer through the ID-keyed transport map.
A save stages a complete sibling folder, rechecks original bytes/file sets, then
swaps directories. The previous generation is recoverable; older generations are
archived under `local/profile-backups/`. Links/reparse points and conflicting
external edits reject. This is recoverable multi-file replacement, not a promise
of an OS-level compare-and-swap or power-loss atomicity.

The existing reader still accepts development aggregate versions 1–4. On an
explicit save, old manual settings translate once into preferences; Automatic's
cached format is never promoted to an override. Current preferences never consult
legacy fields. Original aggregates are archived under `local/legacy-profiles/`.
Unknown old format annotations move under `legacy_format_extensions`, with no
operational meaning; conflicting annotations block replacement. This existing
read path is a convenience, not a commitment to support older development builds.
