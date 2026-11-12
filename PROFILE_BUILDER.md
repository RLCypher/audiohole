# Profile builder and engine handoff

This is the current development contract, authorized by the owner after review
with the engine agent. It replaces the earlier Test v4 contract. The owner may
regenerate development profiles; compatibility with older development builds is
not an acceptance requirement. Production storage decisions O-004/O-006 and the
streaming backend remain open. Test Mode does not route audio.

## Canonical storage boundary

`profiles/<display name>.json` is an **individual profile record**:

```json
{
  "format": "audiohole-profile",
  "schema_version": 3,
  "profile": {
    "display_name": "Desktop audio",
    "notes": "User notes",
    "id": "stable-profile-id",
    "holes": []
  }
}
```

See [the complete example](docs/test-profile-example.json). Record version 3 is
independent of the **aggregate transfer wrapper** used inside profile bundles:
`audiohole-test-configuration`, `schema_version: 7`, `profiles: [...]`,
`devices: []`. The wrapper can also carry `profile_record_extensions`, an opaque
map of individual-envelope extensions keyed by profile ID. It is transfer
metadata, not routing intent. `validate_profile_record` validates individual
records; `validate_test_document` validates the aggregate. They are not
interchangeable. Unknown/newer record versions reject without rewriting files.

Profiles retain names, notes, stable IDs and ordered holes. Each hole retains
its name/notes/ID, `enabled`, `duplex`, `routing`, and
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
  "channel_map": [],
  "preferred_clock_device": "automatic"
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

- **Use advanced tuning** (`forward.advanced_tuning` / `reverse.advanced_tuning`):
  the first control inside each disclosure. Off retains that direction's saved
  preferences/maps but resolves them automatically. On applies its saved values,
  with unspecified values inheriting. The sibling direction is unchanged.
- **Advanced tuning**: separate collapsible panels directly beneath Forward and
  Return. Each contains only its own direction's settings. Return and its panel
  are hidden for simplex holes. Disclosures never change intent, eligibility, the
  sibling panel, a running route or the top-level window size.
- **Reset to Automatic**: explicitly clears the selected direction's preferences
  and mapping, preserving assignments and the sibling direction.
- **Enabled**: enables the hole independently of overrides.
- **Routing Engine**: independent per-hole request, serialized in `routing` as
  `automatic`, `wasapi`, `miniaudio` or `experimental`. The labels are Automatic,
  WASAPI, Miniaudio and Experimental. Unknown identifiers remain unavailable and
  block preparation of an enabled hole, independently of tuning eligibility.
  Test Mode still has no streaming backend; selection saves intent only.
- **Exclusive mode preferred**: a separate `exclusive_mode_preferred` boolean in
  each direction, default false. Its checkbox sits below that direction's device
  selectors and above its information pane. It applies in Automatic access mode. An active explicit source/destination or
  common access-mode requirement takes precedence, including Force shared mode.
  With tuning disabled, retained forced settings are inactive and the preference
  applies to Automatic. This flag is a preference, not a forced-mode requirement; a rejected exclusive
  configuration does not forbid an otherwise usable Automatic shared candidate.
- **Preferred clock device**: each direction saves `preferred_clock_device` in
  `settings.preferences`, using `automatic` (default), `capture` or `playback`.
  GUI labels are Automatic, Capture and Playback. This is reserved metadata with
  no current streaming effect. Disabling tuning excludes it from effective
  preferences while retaining the saved selection.

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
buffering. Exclusive mode preferred requests exclusive-first selection explicitly;
forced modes are requirements and supersede that preference. Unset endpoint mode
requirements remain unset; the prepared common `mode` is only an Automatic policy
candidate when no explicit mode requirement exists. The engine
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

The existing reader still accepts development aggregate versions 1–5 and
individual record version 1. Older `routing_engine` keys translate to `routing` on
an explicit save. Missing exclusive preference defaults false; missing clock
preference defaults Automatic. On an
explicit save, old manual settings translate once into preferences; Automatic's
cached format is never promoted to an override. Current preferences never consult
legacy fields. Original aggregates are archived under `local/legacy-profiles/`.
Unknown old format annotations move under `legacy_format_extensions`, with no
operational meaning; conflicting annotations block replacement. This existing
read path is a convenience, not a commitment to support older development builds.


## Current UI labels and Explorer

Access choices are Automatic, Force exclusive mode and Force shared mode. They
serialize as null, Exclusive and Shared respectively; display wording is not an
engine enum. Each direction retains independent preferences and mapping. Routing Engine remains a hole-wide selector; tuning eligibility and exclusive
preference are independent per direction.

Profile/device context menus offer Show in Explorer for saved JSON. Profiles are
located by stable ID in the current named folder, including after rename; device
paths come from the scanner's opaque edit snapshot. The Windows shell API selects
the actual file without constructing an Explorer command line. Preview fixture
records have no saved file and the action is disabled. Missing files show feedback
without creating records or guessing a filename.


## Current workflow and distribution amendment

Individual record v3 and transfer wrapper v7 move `advanced_tuning` and
`exclusive_mode_preferred` into each Forward/Return object. They are independent
of each other and of the hole's routing selector. Older development flags are
copied to both directions on read; canonical output writes only direction flags.
Newer versions and malformed booleans are rejected. The owner may regenerate old
profiles; this is not a long-term development compatibility promise.

Routing Activation preferences are editable in both environments. Test mode saves
intent for a future backend, while actions requiring a routing backend stay
unavailable. Immediately and Manual show a disabled zero delay; After Delay edits
the retained configured duration. Fresh appearance remains Follow System; existing
saved themes are preserved. Wheel gestures over dropdowns (including edit/popup
children) scroll the nearest page instead of selecting another value. Keyboard
and explicit pointer selection remain available.

Distribution root contains AudioHole.exe, README.txt and folders: bin/ (probe and
first-run marker), assets/ (including avatar-assets.json), docs/ (handoff documents
and build-info.json), licenses/ (third-party notices), personalities/ (packages).
The welcome is controlled solely by bin/AudioHole.has-run; the legacy
show_preview_dialog preference no longer overrides that flag. Replay confirms,
removes it and restarts. User Exit actions confirm with Exit/Cancel; controlled
restart and unattended smoke shutdown do not ask twice. The bottom status bar
omits the Test Mode suffix while retaining engine availability.

### Editing inactive tuning (2026-11-12)

The tuning-enable switch controls whether saved overrides are effective, not
whether edits are retained. All displayed tuning values can be edited and saved
while the switch is off; enabling it later activates those retained values.
In particular, operating-mode selections no longer reset to Automatic just
because tuning is disabled. Forward and Return remain independent. Invalid
editable input blocks Save rather than being silently discarded. No profile
schema, inheritance, access-mode enum, or routing-engine field changes follow
from this GUI correction. Unused fallback dropdown placeholders are removed.
