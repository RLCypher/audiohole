# Norm's tutorial

Tutorial replaces the former user-facing Preview environment. The approved
owner script is [docs/tutorial/Norm-script.txt](docs/tutorial/Norm-script.txt),
version 2.7. It is compiled into inert UTF-8 catalogue data by
`scripts/GenerateTutorial.py`; `--check` verifies an exact reproducible build.
Dialogue is not executable code, and `NORM:` is an authoring marker, never a
rendered prefix. Existing welcome wording and the three choices stay intact.

The generated catalogue is split by the six named lesson phases, the separate working-launch greetings,
recovery lines and avatar-click reactions. Each component keeps complete beat
records in a separate UTF-8 literal, capped at 8,192 bytes to stay below MSVC's
C2026 limit. Oversized components fail generation with the phase name. Components
are joined once on the control thread into process-lifetime storage; no dialogue
is shortened or loaded from an external file. Catalogue integrity checks verify
the byte limit and exact reconstructed JSON before the Windows build and in CTest.

## Launch and storage boundary

On an unmarked launch, **It's my first time** opens Tutorial. **Show me them
holes!** explicitly skips onboarding and opens the working environment. A valid
`bin/AudioHole.has-run` suppresses welcome and opens the working environment on
subsequent launches. The old saved `preview_mode` preference no longer selects a
user-facing environment. Replay resets the marker and restarts; its ephemeral
restart argument selects the replay introduction. The developer-only legacy
fixture smoke remains available to preserve existing regression coverage.

Tutorial owns a fresh simulated controller and memory-only preferences. It never
constructs ScannerService, ConfigurationController, DiagnosticService or profile
persistence providers. It starts with zero endpoints/profiles, never enumerates
Windows audio devices, opens audio streams, reads saved devices/profiles, writes
profiles, exports bundles/logs, registers startup, or publishes taskbar tasks.
Appearance/tray/startup edits made here are temporary. Asset/dialogue reads and
normal GUI initialization remain necessary. These controls do not claim hardware
verification, microphone detection, measured levels, audibility or latency.

The only deliberate persistent tutorial change is the completion/explicit-skip
marker. Completion is requested at N035, after the required Settings visit and
acknowledgement. An early close leaves the marker absent. The Settings Skip tutorial action asks “Skip the tutorial and restart AudioHole?”
with Skip tutorial / Cancel. After completion, Settings and Norm's End tutorial
ask “End the tutorial and restart AudioHole?” with End tutorial / Cancel.
Confirmation records the outcome and restarts through the same clean parent-exit
handshake as Replay; cancel preserves the lesson. A
marker write failure is reported and never redirected to another path. Completion
leaves the user in the same sandbox for exploration; there is no automatic swap
to real devices. The next ordinary launch opens the working environment.

The marker now contains bounded UTF-8 JSON with `format: audiohole-onboarding`,
`schema_version: 1`, `outcome: completed|skipped|none`, and `greeting_shown`.
Presence-only markers remain accepted without a surprise greeting. Outcome and
receipt updates are atomic and protect against external edits. Deleting the
marker still replays onboarding without changing saved preferences or profiles.

On a working launch with a pending receipt, N037 is shown only if Norm is the
selected working personality. A skipped tutorial uses N037A for Norm or N037B
for an already-selected Horu. Vanilla/custom selections have no invented line;
an ineligible receipt remains pending for a later eligible launch. Mark the
receipt shown only after presenting the greeting. These are nonblocking ordinary
personality messages with a mischievous reaction, no tutorial controls and no
personality switch. Tutorial itself always uses Norm, including replay; its
memory preferences never replace the saved working personality.

## Event and identity contract

`Tutorial` observes confirmed UI/controller events on the GUI thread. Stable
choice IDs include their source beat: a stale/double click from a retired beat
cannot advance the next one. Each reply selects one complete incoming dialogue
variant; there are no comeback-only screens or generic Continue controls.
Navigation, Deep/Full settings, device edits/saves, profile/hole creation, Return,
endpoint assignment, exclusive preference, Save and Start all use the real GUI
handlers. N018A requires a confirmed profile rename after creation; N018B requires
a confirmed hole rename after creation. Both highlight only the specific record row, accept
any valid name different from the initial name, and have no reply buttons. Two distinct renamed and saved device logical IDs
are required. The four
fictional identities are fixed independently of their editable display names:

| ID | Original display name | Direction |
|---|---|---|
| tutorial-mixer-in | MixBoard Input | Capture |
| tutorial-mic | Microphone - FakeTech USB Audio Device 6675 (4-FakeTech USB Audio Device) | Capture |
| tutorial-mixer-out | MixBoard Output | Playback |
| tutorial-speakers | Speakers - FakeTech USB Audio Device 636B (3-FakeTech USB Audio Device) | Playback |

Forward connects the fake microphone to MixBoard Output; Return connects MixBoard
Input to the fake speakers. Forward Exclusive mode preferred must be enabled by
the user; Return and unrelated overrides are not silently changed. Save is still
explicit. Wrong assignments, premature Start, missing saves and unrelated
navigation cannot complete an objective.

The simulated scan runs for 20,000 ms and activation for 10,000 ms through the
simulation controller. Activation has no progress bar. Progress and CLI messages are explicitly fictional. Only a successful
`scan_completed` or `simulated_routing_running` event advances the corresponding
operation beat. Cancellation does not manufacture completion. The microphone
joke waits for exactly **I said something** or **Testing 1, 2**; neither is evidence
of captured or assessed speech. All other narration waits indefinitely for a
reply or required GUI action. No dialogue-length progression timers exist.

## Presentation

Norm's dialogue exclusively owns the personality window during Tutorial. Normal
operation messages/toasts cannot replace it. The two authored replies are stacked
at Norm's feet in a reserved region, keeping his size stable between beats.
Replies have the same pulsing pink outline as tutorial callouts and no hover text;
their surfaces/text follow Dark/Light/System, with native high-contrast styling.
A subtle right-pointing arrow is overlaid inside the first dialogue frame to
introduce the reply buttons. It disappears after the first choice and consumes
no layout space.
Pink rounded click-through highlights follow actual control geometry and do not
steal focus or intercept input. Device renaming highlights the specific FakeTech
microphone/speaker rows by stable ID, removing each row's guidance after its saved
rename. Guidance expands by 3 DIP over one second, returns over the next second,
then rests for one second; Low motion and Windows animation settings disable
the pulse. The name field is outlined only for the intended selected device, and Save only
while that device has unsaved edits. Both disappear after its confirmed save.
Forward and Return device-card rows scroll toward the center of the editor over
three seconds; the exclusive-preference checkbox centers over 1.5 seconds. Scroll
bounds are clamped to content, Low motion uses an immediate reveal, and manual
wheel input cancels a tween. Moving from source to destination within the same
card does not recenter again. F1 repeats guidance. Personality/animation/DFA stay visible
but disabled with explicit section labels; Replay is replaced by Skip Tutorial, relabelled End tutorial after completion.
The final message explicitly tells the user the lesson is finished and to exit
and restart for real operation. An End tutorial button at Norm's feet retains a static pink outline. It uses the
same restart confirmation as Settings; completing the lesson alone does not
switch environments.

Clicking Norm selects a contextual authored reaction with a 350-ms click guard.
Only this temporary overlay has a dialogue timeout: 6 seconds. The exact saved
variant, choices, highlight and expression return without advancing. A valid
reply/action during the overlay is handled against the underlying current beat.
Approved expressions use Norm's owned avatar engine, falling back to idle. There
is no separate rendering or animation engine.

## Validation

Portable tests cover catalogue integrity, variant/choice routing, exact identities,
independent exclusive preference, distinct saved renames, both microphone branches,
operation completion/cancellation, overlay restoration, inactivity and explicit
skip. `Gui.TutorialSmoke` walks the actual controls and never writes the installed
marker. Existing `Gui.Smoke` and `Gui.TestModeSmoke` retain their operation,
identity, persistence and confirmation checks. The extracted-package check also
runs all four direct modes outside the checkout, including Gui.EngineSmoke. Native Windows GUI,
click-through,
DPI, assets, first-run/replay and filesystem-boundary checks remain required; a
cross-build or Wine run does not replace MSVC/native Windows validation.

Local validation for this increment: all 246 portable CTest cases passed, including
avatar/scanner/engine/model and catalogue/asset integrity. After the final
selection-guidance adjustment, all 23 tutorial/workflow tests passed again.
The MinGW cross-build passed. Under Wine, fixture GUI, Test Mode, Tutorial,
software-adapter Engine and parent/child Restart smoke paths passed; operation
and tutorial also passed at verified 144 DPI (150%), with the test DPI restored
afterwards. Screenshots of the compact header and centered Return cards were
reviewed. These are supplemental results; MSVC/native Windows CI and hardware
playtesting remain required. The wheel-boundary smoke uses native SB_BOTTOM
rather than assuming that revealing the last character reaches the scroll limit.

The wheel smoke also queues a real presentation refresh before its bottom-edge
handoff. Native SendMessage completes scrolling synchronously, so content and
positions are asserted before yielding. Only after the temporary field text is
restored does the test drain that queued refresh and verify the real information
and controller revision. This guards the Windows CI race where an event-loop
yield replaced the 80-line fixture with the short live summary between gesture
and assertion; the original inner/outer scroll checks remain required.
