# Discoverable personality packages (package v1, dialogue v2)

Copy an entire valid folder under `personalities/` beside `audiohole.exe`, then
restart or Settings → Reload personality files. Remove the folder to uninstall;
replace it to update while retaining its stable ID. Folder name, display name,
character name and stable identity are separate. No source registration or
recompilation is required. AudioHole reads data only; it never executes package
scripts, writes dialogue, generates prose or downloads resources.

```
my-character/
  personality.json
  dialogue/responses.json
  preview/placeholder.png       # optional static preview, not animation
  avatar/registry.json          # optional, future shared engine
  avatar/pack/manifest.json     # PR4 format, future integration
  avatar/pack/...               # that personality's own assets only
```

The existing `default` (Norm), `uwu` (UwU, character Horu) and `vanilla` IDs remain
unchanged. Settings persist the stable ID. If it disappears, select an entire
available default/vanilla/uwu package in that preference order, then first ordered
custom package; never mix its dialogue or art. Keep the saved desired ID so restoring
the folder can restore selection on next launch. If no package is valid, personality
presentation is disabled while technical controls/messages remain operational.

## Manifest

See `default/personality.json` and the installable
[Visitor example](../examples/personality-package/personality.json).
Required: integer schema_version1/api_version1, unique `id`, `display_name`,
`description`, `dialogue` and a nonempty `expressions` capability array. Optional:
character_name, order(-1000..1000; default100), author/attribution extension metadata,
avatar references and preview_image. Extensions are metadata only, never commands.
At most32 packages/legacy catalogs, 64KiB manifest, nesting12; duplicate JSON keys
rejected. String limits: ID64 lowercase ASCII letters/digits/_/-, display/character
128 UTF-8 bytes, description2048. Control sequences/invalid UTF-8 are rejected.

`dialogue.responses` is required for commentary packages. `dialogue.bother` and
`dialogue.dfa` are optional paths (or null); a missing optional resource disables
that feature. These can point to the same full catalog, as shipped: responses
loads ordinary events, while bother selects only `bother` and dfa only `dfa`/`dfa_*`.
Separate catalogs are also supported and must share the same personality ID.
Every loaded response ID must be unique across the final combined package.
Malformed present resources reject the whole package, without partial loading.

Vanilla's manifest selects `dialogue.builtin_technical:true`; only its reserved ID
can do so, and external text cannot override its technical facts. Its own preview
logo is packaged; Vanilla is not a fallback resource pool for other characters.

Paths must be relative, <=240 bytes, without absolute/drive/UNC/ADS/backslash,
`.`/`..` or trailing Windows dots/spaces. Linked paths/escape attempts are rejected.
Dialogue must be JSON. Preview PNG is optional, <=4MiB, dimensions1..4096 bounded
before WIC decoding; decode failure hides it. Identity switches clear old pixels
first. Norm owns the existing temporary placeholder; Horu owns the supplied
`preview/placeholder.png` (original1199×1312 RGBA image, unchanged). Missing owned
images hide the avatar rather than borrowing another character. Bother is a hidden
avatar-click easter egg, with no visible Bother button. No approved avatar art is copied,
regenerated, moved or substituted from PR4 in this milestone.

`avatar.registry` and `avatar.manifest` resolve within the package. Missing files
report avatar unavailable. Present files require integer schema1 and matching
personality ID; registry must contain only that ID and point at the same manifest.
This is structural availability only; the shared engine must still verify styles,
frames, hashes, approval and decoded art. Public `Personality` metadata exposes
resolved paths/root, configured expressions, events/categories and availability.
`load_directory` returns validation diagnostics. Invalid packages never stop routing.

PR4's Repository expects a registry root, not a lone manifest. Future migration can
supply an isolated per-package avatar registry mapping the stable ID to the owned
`pack/manifest.json`, preserving manifests/assets byte-for-byte. Asset paths remain
relative to that pack. Runtime stays shared outside packages; PR4's present common
registry layout is inspected only, not changed. Coordinate migration with its owner.
See [capability report](../AVATAR_CAPABILITY_REPORT.md) and
[control handoff](../INTERNAL_CONTROL_CONTRACT.md).

## Response data

Shipped `dialogue/responses.json` catalogs use:

```json
{
  "schema_version": 2,
  "id": "my_voice",
  "display_name": "My voice",
  "events": {
    "ready": [
      {"id":"my_ready_001", "category":"status", "text":"Ready when you are.", "expression":"happy", "fallback_expression":"idle", "weight":2}
    ]
  }
}
```

Every applicable authored response has stable id, category, text and preferred
expression; subcategory, fallback_expression and weight are optional. Built-in
metadata references approved statics in each character's default PR4 style. No
animation plays here. Packaged expressions/fallbacks must belong to the declared
20-state capability subset; declarations do not manufacture missing artwork.
Unknown expression, duplicate IDs/text or missing metadata rejects a v2 catalog.
All existing Norm/Horu texts, weights, Unicode and pool order are preserved.

At most256KiB per catalog,128 pools,32 variants/pool,2048 UTF-8 bytes/variant,
weights integer1..100(default1), nesting12. Root/event/response identifiers use the
same ID rules. Unknown catalog fields are rejected. Unknown event IDs are retained
for future builds. Technical messages, warnings, controls and diagnostics always
come from application state/compiled information, never package assertions.

Selection retains existing weighted choice after excluding the immediately previous
variant when alternatives exist. Repeated reads of one occurrence return the same
copy without consuming randomness. `Messages::show` returns personality/response
IDs, category/subcategory, text and preferred/fallback expressions. Stable authored
response ID differs from a per-click occurrence ID used by a future avatar adapter.
History is bounded. Missing optional commentary shows the same operational technical
information with no borrowed character or visual reaction.

Both built-in Bother libraries contain exactly30:10 general,10 factual tips,5 easter
eggs,5 unmistakably fictional facts. Avatar click uses this picker, a350ms
monotonic cooldown and operational-warning guard. Bother shows only the authored
reply; its technical/status line is blank and hidden to preserve vertical space.
The field heading uses character_name (Horu/Norm); Vanilla uses Status. Selector
labels and stable IDs remain unchanged. Bother has no controller,
DFA, scan or configuration side effects. Vanilla has no Bother/DFA feature.

DFA button uses `dfa`; ordinary settled values use that pool. Existing threshold
keys: dfa_minimum(0.00), dfa_nice(0.69/6.90), dfa_dank(4.20), dfa_metal(6.66),
dfa_six_seven(6.70), dfa_maximum(11.00); owner-approved `dfa_<hundredths>` eggs retain
their exact text. Raw decimals are range-checked before half-up rounding; negative
and above-11 inputs use dfa_negative/dfa_above_maximum without changing value.
DFA remains presentation-only. Vanilla/missing-DFA packages hide those controls.

## Legacy compatibility

Flat schema1/2 catalogs still load through discovery or Load custom personality.
V1 pools contain text strings or text/weight objects; absent face metadata means no
visual reaction. Loading never rewrites or invents legacy expression metadata.
Legacy catalogs remain selectable by stable ID, with missing avatar disabled.
Do not retain flat default.json/uwu.json beside migrated same-ID folder packages:
all duplicate identities are rejected. Extract new builds into a fresh folder.
The package migration is built into this source tree, not a destructive rewrite of
user data. Full reload replaces optional catalogs, including externally loaded ones.

Selected ID/theme now persist in isolated Preview Build preferences. DFA value and
history remain transient. These presentation formats do not approve production
profile/device schemas or change any Open engine/storage decisions.


## Integrated runtime

The approved assets now live inside each built-in package's avatar/pack directory,
with a single-ID avatar/registry.json. The main GUI uses the shared Session/Panel
engine and the manifest's default style. Metadata exposes avatar_style and approved
avatar_states; the engine still validates actual resources before display. Approved
manifests/art were relocated unchanged. Avatar manifests have a separate2MiB bound.
Earlier placeholder-only/no-playback notes above describe the pre-integration stage;
see [current integration](../AVATAR_INTEGRATION.md). Optional/missing resources never
borrow another character, and a missing avatar does not disable valid dialogue.
