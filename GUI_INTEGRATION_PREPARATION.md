# GUI refinement and integration preparation

This records the November 2026 assignment, repository inspection, first GUI
increment, and dependencies needed to finish the milestone. It is not a claim
that Test Mode, production persistence, personality packages or avatar integration
have been implemented.

## Inspection findings

- `src/model/controller.h` is the existing internal command/snapshot boundary.
  Frame and Tray share its single controller. `SimulatedController` owns session
  records and implements no device enumeration, audio streams or disk persistence.
- `Result` currently means a synchronous simulator result. Events have monotonic
  sequence numbers; snapshots hold authoritative simulated state. Results have no
  request correlation IDs, subscriptions or worker-thread transport yet.
- Profile Save modifies session records, while profile Apply validates and
  activates a copied simulated plan. Neither persists files. Buffer preferences
  are distinct from actual engine buffers; actual values remain unavailable.
- Test Beep tracks a simulated destination and a bounded interval. It emits no
  sound. Cancellation, natural completion, device loss and Stop clear that state.
- Personality catalogs are flat JSON files. Norm uses stable ID `default`, UwU
  uses `uwu`; `vanilla` is compiled in. Existing behavior includes per-event neutral
  fallback. The requested package-isolation policy changes that behavior and
  requires explicit migration tests, rather than silently borrowing resources.
- No completed avatar subsystem, manifests or public API is present in the
  inspected workspace or in this branch. The supplied static placeholder is not
  the completed subsystem. It has not been changed or integrated.
- `CONFIG_FORMAT.md`'s reconciliation notes and `DECISIONS.md` O-004/O-006 leave
  production serialization and storage details unresolved. There is no production
  persistence implementation in `src/` to reuse for Test Mode.

## Implemented independent GUI increment

- The existing startup notice now says Preview Build. It still describes the
  existing simulated, session-only environment accurately. Two-mode selection and
  startup preference persistence remain pending; no nonfunctional Test Mode choice
  has been added.
- Each buffer editor has an adjacent disabled Apply button. Its explanation
  distinguishes an edited preference from confirmed live engine settings. The
  controller has no live-buffer update/acknowledgment operation, so the button
  cannot report fictitious application. Existing profile Save/Apply is preserved.
- Application-painted disabled buttons use desaturated backgrounds and existing
  disabled foregrounds. Native disabled control behavior is retained. Interactive
  tutorial and other intentional placeholders remain enabled.
- Navigation and log hover text are removed. Testing level and scope explain
  their actual simulated behavior. DFA tooltips retain character while documenting
  decimal rounding, range rejection and 0.01 steps; DFA behavior is unchanged.
- A single fixed-minimum-width Test Beep/Cancel Beep button reads controller state
  for every click. Refresh follows that state on start, cancellation and natural
  completion. The preview continues to emit no sound.
- Failure injection and recovery are moved into a separate Simulate Errors section.
  All current sessions are preview sessions. Hiding that entire section in Test
  Mode remains pending the authoritative mode implementation.
- Native smoke checks cover beep cancellation/completion and width, disabled buffer
  Apply, and absent navigation/log tooltips. Cross-compilation does not execute
  these checks or establish Windows visual quality.

## Required inputs before dependent implementation

1. Supply a read-only repository/branch or workspace location for the completed
   avatar subsystem. Inspect the actual manifests, public interfaces and per-character
   expression availability. Do not infer availability from the 20 specification
   identifiers in the assignment or from the temporary PNG.
2. Supply the approved production persistence implementation/schema, or finalize
   the unresolved contract explicitly. Test Mode must reuse that implementation
   under an isolated root. Serializing the current fixture structs into a new
   ad-hoc format would not establish production compatibility.

## Remaining implementation sequence

1. Add one authoritative operating mode at application construction. Preserve the
   preview fixture environment. Build Test Mode from the approved persistence
   provider without loading preview devices/profiles into that root. Store startup
   dialog and next-launch mode preferences through the approved settings mechanism.
   Record errors without overwriting newer or invalid user data.
2. Add the two startup choices and Settings controls. Restart must save pending
   preferences, complete shutdown and release the single-instance guard before
   relaunching. Preserve unsaved-edit handling; a failed relaunch must remain
   recoverable. Do not start a competing process against the existing guard.
3. Gate the complete Simulate Errors section and handlers using the authoritative
   mode. Test Mode must expose neither fixture injection nor empty section gaps.
4. Discover data-only folder packages with stable IDs independent of folder names.
   Validate versions, duplicate IDs, bounded resources and relative paths, including
   symlink/reparse-point escapes. Migrate existing flat catalogs without changing
   their exact dialogue or DFA selection. Preserve selected IDs. Required resource
   failure rejects the package; optional avatar resources disable presentation or
   select a documented same-personality fallback.
5. Author exactly 30 Bother responses each for Norm and Horu: 10 general, 10 useful
   factual tips, five easter eggs and five unmistakably fictional irrelevant facts.
   Keep Bother independent of DFA, use existing repetition avoidance, and bound
   click frequency. Preserve existing authored dialogue and easter eggs verbatim.
6. Assign stable response IDs, categories, preferred expressions and optional
   same-personality fallbacks to every applicable response. Validate against the
   inspected manifests and publish an availability report for each character.
   Until those manifests are available, expression support is **unverified** for
   both Norm and Horu. Do not generate approved artwork or start animation playback.
7. Extend the existing controller boundary for correlated command results and
   bounded subscriptions to state/events. Keep its authoritative snapshot; do not
   create a second engine-state store. Publish unavailable telemetry explicitly.
   Unsupported live-buffer/audio commands must be rejected clearly. Control and
   telemetry delivery must never block audio callbacks or depend on GUI responsiveness.

## Future avatar handoff, not implemented integration

The intended flow is a GUI click requesting Bother from the selected package,
returning its stable personality/response IDs, text and validated expression
metadata. A future presentation adapter can send that reaction and the selected
package's resolved manifest/asset root to the shared avatar engine. No engine code
belongs inside personality folders. Operational errors take priority over transient
flavor. Completion returns to the latest authoritative baseline, with healthy
active routing relaxed rather than continuously working.

The exact call signatures, capability negotiation and interruption/duration fields
must follow the completed subsystem's inspected API. OBS, Stream Deck, desktop-pet
adapters and external IPC are not implemented by this increment.

## Validation gates for completion

Beyond current portable and native smoke tests, verify isolated persisted Test
profiles/devices, preference round trips, restart ordering, failed writes, external
edits and schema migration. Test package installation/removal, duplicate IDs,
missing resources, path escapes, whole-personality fallback and saved selections.
Check exact existing DFA strings and behavior, the 60 Bother category counts,
expression availability and subscription lifecycle/error/cancellation behavior.
Run native Windows builds, GUI/DPI inspection and extracted-package launch. Report
these results separately from compilation and simulated operation results.
