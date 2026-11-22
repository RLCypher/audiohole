# Plug Board app-list controls

The owner-authorized app-list batch adds visual grouping, app nicknames, durable
inactive clearing, and an inactive-only name filter. It does not change the
engine, native policy adapter, endpoints, process ownership, profile schemas,
animations or existing authored tutorial/personality text.

## Scope and state

Clear inactive apps sits below Windows Mixer. Exempt apps sits beside it. The
approved tooltips are preserved verbatim. Clear app and Rename are app context
menu actions; Clear group is an expandable group context action and is disabled
for groups with no inactive members. Clear only operates on Inactive, including
a service-thread recheck to protect an entry that became active during a menu.
Recently Active entries stay visible. Clearing never deletes a catalog record,
assignment, checkpoint or policy. A subsequent observed Active stream (including
a coalesced short-lived observation) releases the clear flag. Browser/process
launch alone is not proof of a new audio session.

Grouping is within one socket and activity section, with an instance count and
individually expandable rows. Repeated detected labels and known shared-host
filenames are bundled for display. Each member retains its own application UUID,
identity, process evidence, actions and routing. The group header cannot be dragged
as a routing target, has no app tooltip or selection fill, and exposes expand/
collapse state to accessibility. Expanded child rows retain normal app actions.
Renaming changes the saved label while retaining the original detected label for
grouping. Tutorial app recognition now uses the existing exact fictional image
identity, so renaming a practice app cannot remove its dependency from the lesson.

The Exempt apps popup uses the owner's final three-line message verbatim. A
multiline, vertically scrollable field shows five rows initially, grows/shrinks
with the popup, and inherits the app's resolved Light/Dark/System/high-contrast
palette. Exempt common Windows processes appends the earlier suggested host names,
retains custom entries, removes duplicates, and never applies itself. Apply is an
asynchronous checked save and leaves the window open. Close/Escape discards edits
since the last successful Apply; an already submitted Apply remains a requested
save. External exemption-list changes cause a conflict rather than replacement.

Filtering matches complete displayed names, ignoring ASCII case and an optional
.exe suffix, without wildcards or substring matching. Whitespace is trimmed;
commas and line breaks separate entries. Up to 256 names of 256 characters are
accepted, within a 64 KiB editor bound. No names are exempt by default. A renamed
entry matches its nickname. Only Inactive entries are filtered: active and recent
sources stay visible and saved routing continues normally. Exemptions never send
an app to System or make it unroutable.

## Additive schema and compatibility

The portable audiohole-plug-board schema remains version 1, with optional fields:

- Root inactive_exempt_apps: array of app names, default []. Global to the library.
- Application detected_name: original grouping label; legacy default display_name.
- Application host_name: display-only executable basename, default empty.
- Application inactive_hidden: clear-until-next-activity flag, default false.

Application display_name remains the editable visible name. Host labels and
nicknames never grant native routing authority. Unknown fields, application IDs,
verified aliases, working/saved boards, revisions and assignments retain existing
checked atomic replacement and conflict behavior. Old files load without migration
writes. Missing new fields retain visible, unfiltered behavior. Older versions can
preserve the added fields as extensions, but do not implement their presentation.

The previous repeated-PowerShell issue also had a persistence cause: temporary
owner keys were only remembered in memory while their catalog rows were durable.
Local history.json now optionally records provisional_key for an unresolved
entry, using the existing exact PID + process-creation-time key. The same still-
running process reconnects to its saved UUID after AudioHole restarts; a new
process lifetime stays separate. No raw PID is added to portable library.json.
This is presentation reassociation, not proof of the hosted script's durable
identity. Existing old rows lacking this evidence are retained rather than
silently merged or reassigned. Clearing/grouping handles their visible clutter.

All model changes run on the module control thread. GUI commands enqueue values;
Apply polls a future with a GUI timer. Presentation-only commands do not cancel
native policy work or request reconciliation. Activity changes can release clear
flags through the existing debounced catalog flush. Audio callbacks perform no
new work. Exemption names are normalized once per publication, not per app/name
pair. Existing catalog/history/cache limits and tutorial memory isolation remain.

## Verification

Portable tests cover legacy/default/invalid fields, unknown-field preservation,
JSON round trips, whole-name matching, suggestions/deduplication, unchanged routing
intent, failed saves, active/recent visibility, clear reappearance, and same-owner
reassociation across restart without granting shared-host routing trust.
Native GUI smoke exercises the actual drawer buttons, group accessibility and
context menus, app Rename/Clear, saved socket retention, exemption Apply/Close,
suggestions without saving, exact text/tooltips, both app themes, five-row field
geometry, vertical scrolling, and resize growth/contraction. Tutorial/engine/
restart tests remain enabled. No screenshots/evidence are enabled by this batch.

The user manual now describes these controls and explicitly documents opt-in
GitHub-only weekly startup checks, reminder/skip behavior, and separately requested
installer downloads. It preserves the guide's existing terminology and exclusions.

Native validation corrections: the smoke check reads the visible static-text
label and tolerates layout line wrapping, rather than using SetHelpText, which
requires an installed help provider. View detachment closes the callback gate,
unregisters the handler from wxApp's pending list, then deletes queued events;
wx DeletePendingEvents alone does not perform that unregister operation. This
prevents a stale ready-list entry during shutdown, including failed-test cleanup.
