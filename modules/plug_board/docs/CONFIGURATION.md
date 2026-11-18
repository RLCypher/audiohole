# Configuration and compatibility

Module version 0.2.6 implements `audiohole-plug-board`, schema version 1. The
standalone library is separate from existing AudioHole profiles and formats.
No earlier inspection launcher wrote this file, so there is no 0.1.x library
migration. Unknown schema versions fail visibly and leave bytes untouched.

`library.json` is UTF-8 JSON, two-space indented. The supplied
[schema](library-v1.schema.json) and [example](library-v1.example.json) describe its
shape. The C++ parser also checks unique record IDs, identity conflicts, reference
integrity, System ordering, actual calendar timestamps and revision relationships.
It rejects duplicate object keys, malformed UTF-8, more than 4 MiB or 64 nesting
levels. Limits are 128 boards, 256 sockets per configuration, 20,000 application
records/profile links and 64 verified aliases per application. These are bounds,
not a measured performance claim at maximum capacity.

Each board has a working configuration and a saved checkpoint. Every working edit
is validated and persisted before native routing can be queued. Save promotes
working to saved. Save As gives the copy a new board ID and new custom socket IDs,
remaps assignments, and shares application IDs. System keeps the local socket ID
`system`, remains first and cannot be deleted. Unknown JSON members survive known
field edits, including socket endpoint changes and copied configurations.

Applications have random stable record IDs and typed verified full-image/AUMID
identities. They never match by PID, filename, label, publisher or guessed version
segments. Explicit `verified_aliases` can combine known executable identities;
manually adding one is an identity assertion. Ambiguous canonical paths are
reported without merging assignments. Changing an identity invalidates previous
process tokens and observations before another policy operation. A newly observed
application is unmanaged until an explicit move, relevant endpoint change or board
selection takes ownership. Startup/profile activation reconciles managed/assigned
intentions. System Sounds and uncertain shared/protected owners cannot be treated
as ordinary verified application policy targets.

Only logical playback device IDs appear in portable socket targets. The standalone
shim keeps exact raw Windows ID bindings in `local/devices.json`. Missing or
ambiguous bindings remain pending. It does not infer an equivalent device by
friendly name after migration. The integrated host supplies its existing logical
inventory and binding decisions instead of this standalone map.

The disk writer holds an exclusive cooperating-writer lock, checks expected bytes
before and immediately before replacement, rotates three `.bak1`–`.bak3` copies,
flushes a same-directory temporary, and atomically replaces the current file.
Windows uses `FlushFileBuffers` and write-through replacement; POSIX additionally
flushes the directory. A post-replacement flush failure retains uncertain intent
but forbids policy until durability is confirmed. These checks detect external
edits; an editor that does not honor the lock cannot participate in an atomic
compare-and-swap. Edit while the module is stopped for a conflict-free workflow.

Malformed libraries are not replaced by defaults or silently restored. The service
provides explicit Reload and confirmed Restore backup APIs for an integrating host;
the standalone app no longer exposes a Diagnostics screen. Restoration first preserves
current bytes as recovery evidence. A second instance that cannot acquire the
writer lock stays read-only. A storage failure never authorizes native writes.

`local/history.json` stores last-observed activity and historical policy state;
`local/diagnostics.json` stores up to 512 redacted events. Historical log entries
are marked when loaded. Corrupt local files remain intact and their failures are
reported; new events stay in memory when their log cannot be written. The last
128 historical events are loaded into the bounded ring. Writes coalesce on a
500 ms dirty deadline; a clean hidden module has no recurring flush timer.

UTC persists history across restarts; a monotonic clock classifies current age.
Future/invalid persisted timestamps never invent recent playback. Hiding the view
creates an observation gap and marks observations stale. The module does not scan
hidden sessions to reconstruct activity during that gap.


Version 0.2.1 preserves the same schema and data roots. Display placement for an
unassigned app can follow its observed output into an existing matching socket;
that inference is not written to `application_assignments`. Explicit assignments
continue to win. No migration or reset is needed when upgrading from 0.2.0.

Version 0.2.2 keeps this schema and the same data roots. Explicit application
assignments have no age limit. They live in each board's working configuration,
not in local activity/icon caches, and do not require the Save checkpoint action.
Closing an app, expiry of Recently Active, a module restart or missing endpoint
never deletes them. An intentional move/reset, socket removal, checkpoint revert,
board edit/removal or manual library edit can change them. Identity still requires
the same verified canonical image/AUMID or an explicit verified alias.

The socket array order is the main panel order. Pointer and keyboard reordering
persist it immediately. System is always first; custom sockets can be removed
consecutively. Internal AudioHole records from an older library are preserved in
JSON for compatibility, but hidden and excluded from policy/process monitoring.


Version 0.2.3 adds an optional socket `appearance` object, with `color` and `icon`
strings. Defaults are `default` and `none`; default-only objects are omitted.
Colors: red, rose, purple, violet, indigo, blue, sky, cyan, teal, green, lime,
yellow, amber, orange, brown, slate. Icons use the Material identifiers in
`socket_styles.hpp`: sports_esports, forum, music_note, movie, language,
headphones, mic, volume_up, view_in_ar, live_tv, notifications, cable, star,
work, lock, power_settings_new. These values describe presentation only.

This is an additive schema-v1 extension. Existing libraries need no migration.
Earlier readers retain it through their unknown-field preservation; newer readers
retain unknown future style IDs and nested members while displaying a safe
automatic/empty fallback. Known UI edits validate offered choices and preserve
unrelated appearance fields. Save/checkpoints, Save As and working-order changes
carry appearance with each socket. It never enrolls apps or reconciles policy.
App labels can hide .exe while catalog names, paths, aliases and assignments are
preserved verbatim. No identity migration is performed.


Version 0.2.4 appends `desktop_windows` (PC) and `laptop_windows` (Laptop) to the
icon catalogue, for 18 icons. Existing icon IDs, color IDs and saved appearance
are preserved. The schema and data roots remain unchanged. Application hover
help is the short drag instruction; it does not affect discovery or routing.

Version 0.2.5 adds disposable `local/icons.json`, separate from the unchanged
portable library and its schema. The icon STA atomically saves learned taskbar
RGBA pixels as hexadecimal JSON, keyed by complete canonical image/AppID and
executable write-time/size. PIDs, labels and runtime window handles never identify
saved icons. Replaced executables miss the old cache instead of inheriting stale
artwork. Entries are limited to 128 and 4 MiB of decoded pixels; documents are
limited to 10 MiB. Invalid or unsupported cache files remain untouched, produce a
diagnostic and disable cache writes for that worker lifetime; live lookup works.
It is safe to remove this file while Plug Board is stopped. Doing so requires
each app to expose its taskbar icon again; no assignments/history are removed.


Version 0.2.6 changes presentation only: Last seen unit formatting, status hover
text and the simplified frame/Settings controls. Age formatting never changes
history timestamps, assignment lifetime, schema versions or data roots. Probe
and diagnostic interfaces have been removed from the user-facing application;
internal bounded logs and the developer/host APIs remain available.
