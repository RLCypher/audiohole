# Profile builder and engine handoff — provisional Test v4

Owner-authorized November 10, 2026. This implements portable routing intent and
GUI/storage changes. It does not select a streaming backend, implement conversion
or synchronization, or claim successful hardware routing. Test Mode has no audio
engine. These rules supersede earlier Test v3 descriptions where stated below.

## Saved intent

Each profile retains its display name, notes, stable ID and ordered holes. Each
hole retains its name/notes/ID, enabled flag, return-channel flag (`duplex`),
`routing_engine`, override policy (`advanced_tuning`), and independent `forward`
and `reverse` directions. The visible caption is Return; the JSON key remains
`reverse`. Source/destination IDs refer to the device registry. Names are readable
hints; renames do not change identities. Hidden devices are excluded from new
choices while existing references survive.

Basic mode requires endpoint assignments and simplex/return selection only.
No successful Deep test or identical source/destination format is required to
prepare Automatic intent. Disconnected/unresolved references remain saveable;
validity, availability and operational success are separate. Runtime initialization
still rejects unresolved bindings and must report actual failures.

Version 4 extends `settings` with `preferences`:

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
  "channel_map": [
    { "source": 0, "destination": 2 },
    { "source": 1, "destination": 3 }
  ]
}
```

`common`, `source`, and `destination` accept the same five preference keys.
Source/destination values override common values independently. Missing, null,
empty-string or `Automatic` values mean no override at that scope. They inherit
common values or Automatic policy. Buffer zero also means Automatic for legacy
compatibility; never allocate a zero-sized buffer. Canonical output writes null
for automatic known properties. Invalid types, fractional integers, out-of-range
values, malformed maps and unknown sample-format/mode identifiers are rejected.
Rates: 1–768000 Hz; channel counts: 1–256; explicit buffers: 1–100 ms. These are
serialization bounds, not claims about hardware support.

Sample-format IDs: `float32`, `pcm_s16`, `pcm_s24_packed`, `pcm_s24_in_32`, `pcm_s32`.
The last two 24-bit forms have different transport containers. Native tags, GUIDs,
block alignment and byte rates can be derived by `derive_pcm_format`; channel
layout remains unknown unless established separately. Users do not supply these
transport details to express a preference.

The legacy `automatic` flag covers the legacy whole-format preset only; it is
not a direction-wide mode/buffer switch. Legacy `format` is advisory when true;
when false it remains an exact retained override. In v4, omitted settings and an omitted unused Return direction use Automatic draft defaults.
GUI v4 edits use per-property
preferences and set that flag true. `settings.mode` and `settings.buffer_ms` are
retained compatibility preferences, used when the corresponding common preference
is absent. GUI edits keep them consistent. More specific endpoint preferences
remain independent and round-trip even though the GUI currently edits common
preferences, endpoint channel counts and mapping. Cached format/WAVE values must not force negotiation.

## Channel rules

JSON indexes are zero-based interleaved PCM frame indexes. The GUI uses one-based
numbers (`1:3, 2:4`) and translates them explicitly. One output can have one source;
a source can feed multiple outputs. This is selection/copy, not summing/mixing.
Unmapped outputs are silent. An empty map uses matching frame indexes up to the
smaller endpoint channel count and silences additional outputs; it does not infer
hardware bus labels, duplicate mono to stereo, or downmix multichannel audio.
Runtime must check every explicit index against the actual negotiated channel
counts before starting. Endpoint-specific channel counts can differ.

## Advanced controls and application

“Show advanced tuning” only shows/hides controls. It does not change overrides,
mark the draft dirty, or affect a running route. “Use advanced overrides” retains
v3's separate policy: off ignores retained manual preferences; on makes them
eligible. “Reset to Automatic” clears that direction's format/mode/buffer/map and
endpoint preferences explicitly. It leaves endpoint assignments and the other
direction intact. Routing Engine remains an independent per-hole backend request;
only `automatic` is currently available. Save stages configuration; Apply remains
explicit. Automatic access initially requests Shared and does not silently fall
back to Exclusive in Test preparation.

`PreparedProfile` is built on the control side. Its Test paths explicitly mark
`runtime_negotiation: true` and carry preferences/channel policy. The legacy
`format` member is only a candidate for these paths, never a mandatory engine
format. The engine resolves capture/playback independently, applies supported
conversion/mapping and its buffering/synchronization policy, then publishes actual
formats, periods, backend/modes and explanations as runtime status. JSON parsing,
registry lookup and scan processing stay outside real-time callbacks. Known
explicit rejection is reported; absent/inconclusive evidence is not a rejection.
No full engine-status implementation is introduced by this GUI increment.

## Named profile files and migration

Application Test storage now uses `profiles/<display name>.json`. Each file uses
the v4 Test envelope with exactly one profile and an empty devices array. A hidden
`profiles/.state.json` preserves aggregate extensions and legacy fixture devices;
scanner device records remain authoritative in `devices/`. Filenames sanitize
Windows-forbidden characters and reserved names; collisions are rejected rather
than overwriting another profile. A rename replaces the filename while retaining
IDs, notes, references and unknown extension data. Newly created profiles/holes
use the next available `(1)`, `(2)`, etc. name suffix.

Existing v1/v2/v3 aggregate configuration loads without rewriting. The first
explicit save writes named v4 files and preserves the exact aggregate under
`local/legacy-profiles/configuration.json`; older `.v1.bak`, `.v2.bak`, `.v3.bak`
files are moved there when a matching safe backup can be made. Conflicting
migration backups block the save. Legacy manual tuning stays active if it was
active; false gates remain false. Legacy explicit format fields become common
preferences. IDs, notes and unknown data survive. Newer or malformed data blocks
saving. Old builds cannot read the new v4 files.

Each save stages a complete sibling folder, rechecks the loaded file set/bytes,
then swaps directory names. `.profiles-previous` is the recoverable preceding
generation; a later save archives it under `local/profile-backups/`. An interrupted
swap restores the previous folder on load if the current folder is absent.
Abandoned staging folders are archived on the next save. This is recoverable
multi-file replacement, not an OS-level compare-and-swap against an external
writer racing the final recheck, or a guarantee against power loss. Links and
Windows reparse points are rejected. Files are not deleted before a complete
replacement has been staged.

Profile exports retain the aggregate `configuration.json` transfer payload for
bundle compatibility. Exports assemble named files; imports validate and preview
conflicts, import referenced device/history records, and commit named profiles
last. A failed profile stage rolls back newly created device/history files.
Imported profiles do not start routing or authorize Windows startup.
