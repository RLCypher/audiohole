# GUI and scanner refinement revision

This implements the owner-authorized active backlog. Animation artwork pruning/import remains deferred; the approved PNGs and animation manifests are unchanged.

## Scanner duration and progress

- Visible progress uses device names and Capture/Playback, device X/Y, configuration X/Y, format/mode and setup/stream/cleanup/checkpoint stages. Counts come from the actual Deep plan. The gauge tracks completed configurations rather than resetting for each API call; partial/cancelled work does not show 100%. The operation elapsed clock continues while Windows/driver calls are pending. Raw endpoint IDs remain in the JSONL journal and exported structured evidence.
- `Scan limit = 0` disables the **overall Deep batch deadline**. Per-device and per-attempt limits, explicit consent, cancellation and worker cleanup still apply. Positive GUI budgets remain 1–3,600 seconds, with millisecond precision. The private scanner utility also accepts `--system-timeout-ms 0`; the separate automated fixture suite retains its own finite limits. This does not make an individual stream indefinite.
- Each attempt retains a durable canonical start checkpoint (running/inconclusive) and atomic terminal checkpoint, preserving externally edited notes/extensions. API stages no longer reparse/rewrite the whole inventory and back up two JSON files each time. They are accumulated in bounded memory and appended/flushed to the existing bounded session JSONL journal. Terminal raw history includes the complete stages. No fsync is performed for every journal event; an OS/power failure can lose the journal tail, while the durable initial record remains inconclusive rather than claiming success. No automatic startup migration or success reconstruction occurs.
- Initial checkpoint and worker wall times are recorded in attempts. Final checkpoint and total attempt wall times, timestamps and monotonic elapsed times are recorded in progress. Requests include executable build ID. Deadline/cancellation checks run between worker protocol messages and the remaining device budget is checked again after the initial checkpoint. Filesystem operations remain synchronous/cooperative: a blocked OS file call is not a hard interruptible deadline.
- Defaults remain eight configurations and two-second streams. The optimization does not relax format/evidence criteria, invent recommendations or establish sustained reliability. The handover's known limited `verified` classification remains qualified.

## Clear device list

Devices has **Clear device list…**, enabled only for an idle Test scanner. A default-No confirmation explicitly covers device names/notes/hidden preferences, IDs, scan records, probe histories, local bindings (including backup) and saved scan sessions. Profiles, preferences and unrelated files remain; profile bindings may require reselection after rescanning. A per-request confirmation flag is required by the worker.

Only named scanner-owned paths are retired under both store locks. Symlinks/junctions at roots or nested inside scanner data are refused; failed retirement attempts checked rollback. Retired paths are then removed; cleanup failures return partial completion with the retained path rather than silently claiming all data was erased. Cancellation does not stop a filesystem rename/removal midway. Actual user files are never cleared by automated tests. Runtime exports outside the scan data folders are unaffected.

## Appearance

- Devices retains the existing native selected-row backgrounds, with explicit white selected text in Dark and Windows-selected text in high contrast. Names still ellipsize and have accessible descriptions/tooltips.
- Capture/Playback list cards use the same pink/cyan borders as routing cards. Source/Capture and Destination/Playback headings include privately loaded Apache-2.0 Material Mic/Output icons beside readable labels; failure to load the font leaves the text usable.
- Audio/Hole are adjacent, bottom-aligned with the logo, using the existing interface cyan/pink. Source artwork is not modified.
- Deep limit fields use equal-width grid cells, uniform editor heights and horizontal gaps. Buttons force the existing accessible owner-draw path; disabled and hover colors derive from the surrounding theme, including primary accent buttons.
- Native scrollbar/arrow/dropdown themes follow the app's Light/Dark/System choice. Optional process-local Windows dark-mode capabilities are resolved only on compatible builds (18362+); older Windows falls back to available native theming. No user/system theme setting is changed. High contrast retains system colors. Native visual/DPI/accessibility assessment is still required.

## Reactions and error recovery

Settings offers **Animation loop count** (total plays, 1–20, default 1) beside the existing hold checkbox, and **Loop until next event**. Continuous playback overrides/disables count and hold while retaining their saved values. Finite repetitions finish before the final-frame hold. New events, operational warnings, personality changes and two-minute inactivity sleep still interrupt continuous cosmetic reactions. Reduced motion uses the owned still image. Baseline idle/working/sleeping loops remain authored behavior.

The provisional build-preference JSON gains optional `animation_loops` and `loop_until_event`; absent fields preserve previous behavior. Invalid types/ranges fail without overwriting preferences. No production device/profile schema is finalized.

Invalid input and recoverable save/export failures use local dismissible feedback rather than latching the character/personality warning state. Genuine scan/runtime/identity failures retain operational priority. Bundle exports suggest timestamped names, ask for another name if the ZIP exists, and still refuse overwrite at the worker to cover races. Asynchronous file-action failures use a distinct non-operational event, preserving normal avatar interaction and any pre-existing real warning.

## Validation

The focused regression tests cover repeated/continuous playback, reduced motion, warnings/sleep, preference restart/default/range checks, real progress counts/names, confirmed clear/isolation/profile-reference preservation, unlimited batch completion/cancellation/controller lifetime, stage retention/external edits and bounded checkpoint writes. Existing GUI smoke additionally exercises recoverable numeric feedback and continuous-checkbox precedence. No actual hardware performance improvement or native GUI pass is claimed from portable tests or cross-compilation. See docs/scanner/VALIDATION.md and MANUAL_GUI_CHECKLIST.md for executed results and pending Windows assessment.


## Scanner-backed profile configuration and pending visual fixes

- Test profile endpoint choices now come from nonhidden scanner records; Preview
  retains fixture choices. Duplicate names have an ID suffix, hidden/missing saved
  references keep their identities, and current names appear beside IDs in JSON.
- Exact format/mode capabilities reach the editor through typed worker projections.
  Mode changes rebuild the choices without discarding manual overrides. No support
  is invented from mix formats, closest matches or independent rate/channel lists.
- Ordered provisional Test schema 2 retains full Windows format descriptors;
  names/notes come first and scanner history remains separate. Version 1 loads
  without writes, and an explicit upgrade save preserves the original backup.
- Shared control-side preparation creates minimal typed routing requests; Test
  continues to disable actual routing, monitoring, beep and live-engine Apply.
- Start/Cancel scan now appear above Deep settings.
- Empty Hole Notes stays enabled/read-only with the normal themed surface.
  Inactive directional inputs are disabled individually, keeping container labels
  enabled so Windows does not emboss their text. Empty Profile Notes follows the
  same read-only approach. Native smoke checks the inactive-editor contract and
  scan-action ordering without pixel-sensitive assertions.

Animation pruning and Smart App Control/signing work remain deferred as requested.

Validation for this increment: all 153 portable CTest cases pass in Debug and
Release; the complete Windows x64 cross-build and executable branding integrity
check pass. Replaying a copy of the owner's physical-system report projects 22
devices and 244 typed format/mode observations, finds 36 active pairs with exact
common tuples, and saves/reloads a prepared profile without hardware access.
Native Windows GUI smoke and visual assessment remain CI/Windows checks; a
cross-build is not evidence that those native tests ran here.

## Ten-item polish revision and owner-provided PR #7 art

All ten pending items are implemented in this revision:

1. Delete on the focused Profiles/Holes list dispatches the same context-menu
   action, including default-No confirmation and active/pending profile guards.
   Text editors retain normal text deletion; modifier shortcuts are untouched.
2. Shared owner painting pairs selection text/background for profile/hole lists,
   choices and editable combos, including popup and disabled states. Devices and
   those lists share blue selections, with white selected text in dark mode;
   high contrast retains system colors. Disabled combo edit surfaces stay themed.
3. Deep-limit hover text explains purpose and consequences. Field units now say
   `sec.` and adjacent title/control rows retain uniform sizing.
4. Scan status reserves three text lines with fixed height; changing details does
   not relayout the console. Full visible status is also available as a tooltip.
5. Native progress gauges use one explicit green fill in dark/light themes;
   high contrast keeps Windows styling.
6. Devices/Profiles have Export/Import bundle controls. Saved profiles include
   their hole configurations and referenced device records. Imports preview,
   preserve IDs/notes/extensions, reject conflicts, recheck external edits and
   commit profile references last. See CONFIGURATION_BUNDLES.md for the engine
   handoff, versioning, bounds and failure contract. No routing is started.
7. Advanced directional panes use `wxCP_NO_TLW_RESIZE`; their content scrolls
   inside the existing editor instead of resizing the top-level window.
8. Authoritative device/attempt start and finish checkpoints remain durable.
   Derived capture/playback summaries regenerate once per device run, rather
   than per attempt. Direct checkpoint callers retain their old default behavior.
   Summary refresh time is journaled separately. This reduces redundant summary
   work; a new physical scan is still needed to measure wall-time improvement.
9. `pcm_s24_in_32` is projected as a distinct supported tuple and described as
   24-bit integer PCM in a 32-bit container, including exact valid/container bits.
10. Save device is vertically centered beside the name input; the label sits
    above their common row.

Owner-provided PR #7 (`feef4b0`) is incorporated: 97 idle-padding frames removed
from 34 sequences, 699 approved operating PNGs retained, with manifests,
provenance, native RGBA reference and distribution allowlist updated together.
Retained image bytes/timing and authored playback flags are preserved. Existing
loop-count, continuous-loop, hold, reduced-motion and interruption policies remain
in force. There is no additional manual pruning or image editing in this revision.

Smart App Control/signing remains deferred. Production O-004/O-006 remain open.

Validation: all 164 portable CTest cases pass in Debug and Release. The full
Windows x64 cross-build, executable icon integrity, three PR #7 padding/art
regression tests and the exact 729-file installed-runtime inventory pass. Bundle
fixtures exercise Unicode names and explicit extended Windows paths; actual
native Windows execution remains a CI check. Native smoke now checks declined
Delete confirmations, fixed status/log geometry and unchanged top-level size
when toggling Advanced panes. Dark/light popup/disabled painting still requires
Windows visual assessment; cross-compilation does not prove those native checks.
