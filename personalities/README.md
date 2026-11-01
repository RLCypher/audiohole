# AudioHole authored personalities — format version 1

Vanilla's stable informational messages are compiled into AudioHole. Normal
(`default.json`, the default selection) and UwU (`uwu.json`) contain optional
commentary only. Custom files are data, not programs: AudioHole never executes
scripts, generates prose, filters arbitrary text, or fetches content at runtime.

Copy a JSON file, give it a unique `id` and `display_name`, and edit its `events`.
Place it in `personalities/` beside `audiohole.exe`, then use Settings → Reload
personality files, or choose Load custom personality to load a file elsewhere.
Editing a file does not change an active catalog until an explicit load/reload.
Edits are made by you or your external generator; AudioHole never writes these files.

```json
{
  "schema_version": 1,
  "id": "my_voice",
  "display_name": "My voice",
  "events": {
    "ready": ["Ready when you are.", {"text": "Let’s get connected.", "weight": 2}],
    "dfa": ["Did that fix it?", "Oughtta do the trick."],
    "dfa_six_seven": ["SIX SEVEN!"]
  }
}
```

Omitting an event is supported: that event shows Vanilla's stable message while
other events keep the chosen voice. Deleting an entire file or loading a malformed
file makes that selection fall back entirely to Vanilla, with an explanation.
The `vanilla` ID is reserved and cannot be replaced. Files never override technical
facts, diagnostics, warnings, control/accessibility labels, or recovery instructions.
AudioHole still shows the built-in technical message separately from commentary.

`default.json` documents current event keys. New event keys can be authored for
future builds; an older build ignores those keys until it knows the event.
Use the existing key names, not translations of them. DFA-specific keys:

| Key | Value / action |
|---|---|
| `dfa` | Button click |
| `dfa_minimum` | 0.00 |
| `dfa_nice` | 0.69 or 6.90 |
| `dfa_dank` | 4.20 |
| `dfa_metal` | 6.66 |
| `dfa_six_seven` | 6.70 |
| `dfa_maximum` | 11.00 |

These acknowledgments are placebo commentary only. DFA controls are hidden when
Vanilla is active. Changed, settled ordinary slider/numeric values select from the same `dfa` pool
as button clicks. Threshold-specific pools take precedence.

UTF-8 JSON only. Limits: 256 KiB per file; at most 32 optional catalogs per session;
128 event pools per file; 32 distinct variants per pool; 2048 UTF-8 bytes per variant.
IDs/event keys contain 1–64 lowercase ASCII letters, digits, `_`, or `-`.
Display names contain at most 128 UTF-8 bytes. Control characters and empty strings
are rejected. Each pool is a nonempty array of strings or `{ "text", "weight" }`
objects; weight is optional, defaults to 1, and must be an integer from 1 through
100. Duplicate object keys, duplicate catalog IDs in a directory, nesting beyond 12 levels, and duplicate text within a pool are rejected. Unknown root/object fields are rejected.
Selection avoids immediate repetition when alternatives exist; weights apply after
that exclusion. Tests inject selection or seeds, without frequency promises.

Keep authored text relevant to the event and consistent with PERSONALITY.md.
This is a presentation format only; it does not settle production routing/profile
schemas or the remaining Open owner decisions. Preferences remain session-only
in this GUI evaluation build. Full reload replaces optional catalogs, including
custom files previously loaded from outside this directory.

Expanded DFA events use `dfa_<integer hundredths>`: for example, `dfa_134` matches
1.34, including raw input 1.337 after decimal half-up normalization. The 26 new keys
and exact owner-authored Normal/UwU strings are in the supplied JSON catalogs.
`dfa_negative` and `dfa_above_maximum` acknowledge rejected raw numeric input;
these are not valid values and never replace the last valid session value. Existing
keys/pools are unchanged. Missing custom-catalog entries still use neutral fallback.
Vanilla does not expose DFA or process its validation. No new file schema is added.
