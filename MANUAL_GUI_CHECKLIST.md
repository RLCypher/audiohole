# AudioHole Milestone 0.2 manual acceptance

Use the **Release** artifact for the exact commit under review. Extract the ZIP, read `build-info.json`, and launch `audiohole.exe`. Visual inspection and real desktop/tray behavior require a Windows 10/11 x64 user session. This is a GUI evaluation: **no real devices, audio, or persistent configuration**.

- [ ] Launch outside the source/build tree on a PC without Visual Studio. Confirm the startup simulation notice, build identifier, four pages, readable controls, keyboard navigation, high-DPI scaling, resize/scroll behavior, and clear error dialogs.
- [ ] View a different profile without changing the active profile. Create, duplicate, rename, and delete session profiles; confirm deletion and unsaved-change prompts. Check duplicate IDs and notes remain independent.
- [ ] Add/remove simplex and duplex holes, edit names/notes, enable/disable, and select endpoints by typing/searching. Inspect Windows-name tooltips and stable logical IDs. Save retains active routing; Apply validates and activates. Revert discards only unsaved edits.
- [ ] Configure forward/reverse formats, buffers, and operating preferences independently. Verify visible recommendation evidence, retained manual overrides, incompatible-format errors, missing-device Ready state, ambiguous identity rejection, eight-enabled-hole limit, and five–eight advisory.
- [ ] Start a simulated duplex profile. Inject one directional failure: the healthy direction remains Running and overall status is Degraded. Recover. Remove/reconnect a device; Stop prevents later automatic reactivation. Observe Starting, Running, Ready, Degraded, Recovering, Failed, Stopping, and Stopped (short transitions may require the automated tests).
- [ ] Edit device display name and notes. Confirm `windows_name` and logical ID remain unchanged; unavailable records remain saved. Search device records. Check unknown capabilities and all evidence remain explicitly simulated.
- [ ] Enable temporary source observation; leave Profiles, minimize, or close to tray. Observation turns off. No source activity is presented as destination audibility. Reopen: observation remains off.
- [ ] Select the destination direction explicitly and run **simulated** Test Beep. Confirm target, active indicator, one-second completion, cancellation, removal/Stop/profile-switch cancellation, and exclusive conflict explanation. **No sound is expected.** No output substitution occurs.
- [ ] Run Easy and Advanced simulated scans, inspect consent, progress, cancellation, technical console filters, search, copy, and explicit diagnostic export. No hardware testing or experimental-use permission is implied.
- [ ] Preview immediate/delayed startup; countdown does not reset on refresh. Start bypasses delay for its pending target; Stop cancels pending work. No OS startup entry is created.
- [ ] Switch Norm, Vanilla, and UwU during an active simulation. Operational facts, labels, warnings, names, and diagnostics stay literal. Norm/UwU commentary varies only on distinct events and avoids immediate repetition; Vanilla stays neutral/constant. Repaint/polling does not regenerate commentary.
- [ ] In Settings → Advanced click DFA repeatedly: only its local acknowledgment changes. Vanilla hides DFA. Routing and diagnostic history remain unchanged. Replay Tutorial shows the warning placeholder without losing session data.
- [ ] Close hides to tray; reopen without restarting the simulation. Tray shows state/profile, Start/Stop, profile activation, read-only directional states, Open, Exit. Minimize behaves normally. Verify Windows Explorer restart recovery, notifications/preferences, and second-launch focus on an interactive desktop.
- [ ] Exit stops and terminates, removing the tray icon. Relaunch shows fresh authored fixtures, with no previous edits saved.

## Evidence and limitations

Automated tests cover model transitions, validation, identity preservation, bounded diagnostics, catalog determinism, and unchanged-refresh behavior. Native GUI smoke tests dispatch Start/Stop and DFA events, check duplex/path state and personality visibility, and verify observation stops on page change. The extracted-package test also audits compiler-runtime imports. These checks do not establish visual usability, assistive-technology quality, Explorer behavior, or real audio performance.

CI attempts BMP screenshots of each page for human review; an available image is not a visual test pass. Real hardware discovery, routing, measurements, audible beep, probing, persistence/import, external API, and production startup registration are unavailable. O-001 through O-006 and evidence-dependent D-030/D-031/D-034 decisions remain unresolved; simulator behavior creates no production policy or schema.

## Approved presentation refinement checks

- [ ] Verify Follow System on first launch, switch Light and Dark without losing
  editor contents or changing running simulation state. Change Windows app appearance
  while following System; test Windows high contrast and DPI scaling. Inspect text,
  dropdowns, sliders, selection, disabled controls and focus. System dialogs/menus
  may retain Windows styling, as documented.
- [ ] Verify selected navigation has a distinct background, bold text and selected
  marker; test Tab, arrows/Home/End on navigation, and Ctrl+Tab/Shift+Ctrl+Tab.
- [ ] Edit a copied personality JSON with a unique ID; Load it and inspect authored
  commentary. Omit `scan_started`: only that event uses Vanilla. Other events keep
  the custom voice. Restore the event and Reload from the companion directory.
- [ ] Corrupt/remove a selected file and Reload: entire selection becomes Vanilla,
  shows guidance, and hides DFA. Restore valid files and select Norm/UwU again.
  No catalog can change facts, warnings, endpoint/user names, or controller commands.
- [ ] Check the button reads DFA and tooltip is Working as intended. Test 0.00,
  0.69, 4.20, 6.66, 6.70, 6.90 and 11.00 through exact numeric entry and the slider.
  Confirm appropriate personality responses, exact SIX SEVEN! at 6.70, no drag/poll
  chatter, and no changes to controller state or diagnostic history. Vanilla hides
  both controls. Repeated same-position notifications do not regenerate responses.
- [ ] Start/complete/cancel scans and trigger a selected-device scan without a valid
  selection: the top operations area reports each lifecycle event. Progress updates
  do not regenerate commentary. All results remain explicitly simulated.

## Artifact identity and tester regression checks

- [ ] Compare the **build** identifier in the window title with the first 12
  characters of the commit in the extracted `build-info.json`. Use a newly
  extracted package, not an executable retained from a previous download.
- [ ] Verify the button reads exactly **DFA**, the top heading becomes
  **DFA Acknowledged**, and the slider/numeric entry reaches every defined
  threshold with the appropriate personality response.
- [ ] Start a scan: both top information and personality commentary change.
  Wait for completion: both change again. Progress polling must not reroll text.
- [ ] Change each directional buffer using arrows, typed Enter, and focus loss.
  Confirm one callout per committed changed value, no callout for unchanged
  values, and no changes to active routing until Apply. Vanilla has facts only.

Buffer controls remain in milliseconds as specified in GUI §8.6. The eventual
backend represents frame counts at a particular sample rate; for example, 6 ms
corresponds to 288 frames at 48 kHz or 576 frames at 96 kHz. These are arithmetic
examples, not negotiated endpoint capacities or measured end-to-end latency.
Production buffer/queue policy remains unresolved.

## Production-style preview acceptance

- [ ] On every fresh interactive launch, acknowledge the notice covering all
  simulated functions, silent Test Beep, no real audio/device access, and
  session-only Preview records plus isolated persisted Test records/preferences. Replay Tutorial instead shows its separate placeholder. Check that operation panels,
  top status, tray, and Norm/UwU commentary use production-style language.
- [ ] Hover every action button; verify an explanation, readable text, and a
  dark blue-gray hot state in Dark. Press F1 on a focused button to read its help.
  Test pressed/disabled/default/focus states and Windows high contrast. Native
  dialogs/menus can retain system styling.
- [ ] Confirm Follow System is initially selected. Change Windows app appearance
  while following System; Light/Dark overrides retain all editor/controller state.
- [ ] Observe the dot preceding the state: green Running, red Failed, amber
  Degraded, blue Ready, yellow transitions, gray Stopped. The state text must
  remain readable and meaningful without color. Test high contrast and DPI scaling.
- [ ] DFA is exactly labelled, with no persistent placebo note. Its numeric entry
  and slider span 0.00–11.00. Type 0.69, 4.20, 6.66, 6.70, 6.90 and 11.00, press
  Enter, and verify the slider and threshold response. At 0.00 the minimum response
  applies. Try negative/out-of-range/non-numeric input: controls remain consistent.
  Duplicate Enter/focus notifications do not repeat commentary. Vanilla hides DFA.
- [ ] Start a second copy from a different extraction directory. The current
  instance should reopen instead of silently failing because its title has a build
  suffix. Exit completely before testing a newly downloaded executable.
- [ ] Copy/export diagnostics in Preview: the output retains a simulated-data header even
  though the on-screen console previews production wording. Latency, clock drift,
  CPU and hardware verification remain unavailable; no measurements are invented.

Native Windows hover rendering, F1, typed input, popup visibility, theme transitions,
DPI and tray behavior require hands-on validation. Portable tests and cross-builds
alone do not establish these visual/desktop behaviors.

- [ ] Change DFA to ordinary values such as 5.55 and 5.56: each settled adjustment
  selects a response from the button pool, avoiding immediate repetition. Repeated
  notifications at the same settled value do not reroll commentary. At 0.00 verify
  “Done fucking around.” in Norm and its authored UwU counterpart. Other special
  thresholds still select their own responses; controller/diagnostic state is unchanged.

- [ ] Selected tabs read `[Profiles]`, `[Devices]`, etc.; inactive tabs have no
  brackets or `[selected]` suffix. Retain bold/high-contrast selection and keyboard
  navigation. In Settings, **Replay Tutorial** shows the requested warning placeholder
  rather than the startup simulation notice. Dismiss it and confirm edits and active
  routing state are retained.

- [ ] Uncheck Duplex: acknowledge simplex selection in both Norm and UwU; the
  active duplex plan remains intact until Apply. Vanilla displays stable facts.
- [ ] Save Device: acknowledge success, preserving Windows name and logical ID.
  Empty display names show a validation error, not a success acknowledgment.
- [ ] Change Testing Level and Scope: each selection acknowledges the change for
  the next scan without starting work or altering a currently running scan.
- [ ] Copy Visible Diagnostics: acknowledge only a successful copy; pasted output
  contains only visible events plus the simulation provenance header. Test clipboard
  contention where possible. Export: acknowledge after a successful file close;
  cancel and unwritable destinations must not show success.
- [ ] Clear Diagnostics Log while routing/scanning: the console empties immediately,
  including filtered history. Clear search/filter afterward: old events stay gone.
  Routing, profiles, devices, scan progress, startup, monitoring and Test Beep are
  unaffected. New events appear normally and exported files remain untouched.
  Verify the Clear tooltip and Vanilla's deterministic acknowledgment.

Clipboard availability, file dialogs/cancellation/errors and desktop interaction
require native Windows/manual checks; the portable suite verifies controller clear
semantics and authored catalog coverage, not operating-system clipboard behavior.

## Unified branding acceptance

- [ ] Header logo is upper-right, approximately 128 logical pixels, with original
  mint/pink colors, transparent surroundings, and preserved proportions. Resize at
  100/150/200% DPI: no controls overlap, clipping or unexpected window enlargement.
  Vanilla/Norm/UwU keep the same logo and their existing commentary behavior.
- [ ] Explorer executable, window title bar, taskbar and Alt+Tab use the original
  unoutlined logo. No stock/generic application icon remains at common sizes.
- [ ] Tray uses the same logo: gray Stopped/Ready, green Running, amber Degraded,
  red Failed, blue transitions. The thin stroke follows the exterior silhouette,
  not the internal opening or a rectangular background. Inspect 16/32px on light
  and dark taskbars, high contrast, and after moving between DPI-scaled monitors.
- [ ] Tooltip/menu preserve canonical state text and Start/Stop/profiles/Open/Exit
  behavior. Reopening/Explorer restart retains one tray icon. Idle icons do not
  blink or continuously regenerate. Status remains simulated in this build.
- [ ] Extract away from the repository, disconnect networking, and verify branding
  remains available. The package contains original PNG/ICO reference copies; native
  isolated-package smoke additionally runs after removing those copies.
- [ ] README logo loads through its repository-relative path. Social preview remains
  separately configured; follow assets/branding/README.md only with owner approval.

Windows shell/GUI appearance is pending native execution and human review; portable
image tests, resource compilation and enlarged renderer previews are not substitutes.

## Expanded DFA precision acceptance

- [ ] Type 1.337 and Enter: display 1.34, move the slider to 1.34 and select its
  Skill issue/Skiww issue pool. Check halfway values 1.005 → 1.01 and 5.035 → 5.04.
  Extra digits are accepted during editing; normalized values always show two places.
- [ ] Exercise all 26 new values in the supplied catalogs, including exact wording
  and Unicode at 3.14 and 6.21. Existing special-value pools and 23 random button
  responses remain unchanged; settling an ordinary value still uses the button pool.
- [ ] With a known valid value, enter -0.001, 11.001, 12 and 999. Reject each with
  its authored range response while retaining the prior field, slider and session
  value. Malformed input restores the prior value without an Easter egg response.
  Enter/focus loss and Up/Down/stepper paths must all validate before changing state.
- [ ] Test 0.00, 11.00, long decimal tails and repeated unchanged settlement. No
  polling rerolls or invalid-input clamping. The saved value is session-only here.
- [ ] Cause a routing/scan warning and then use DFA: retain the warning in the
  header and display DFA feedback locally. Diagnostics and routing are unchanged.
- [ ] Vanilla hides all DFA widgets/feedback and produces no DFA validation or
  commentary. Switching back preserves the prior valid normalized session value.

Native typing, focus/stepper events, Unicode glyph rendering and warning layout
require Windows execution; portable tests validate arithmetic/catalog semantics.

## Visual refinement acceptance

- [ ] Compare Dark with the supplied style image at 1536×1024: branding/tagline and
  routing information left, actions/bounded commentary middle, static avatar right,
  mint navigation, two-column Profiles, accented endpoint cards and compact footer.
- [ ] Test 100/125/150/200% scaling, minimum window size, resize and long names/copy.
  Avatar stays within the client area without cropping; commentary scrolls instead
  of moving navigation. Vanilla shows the logo; other modes use the static placeholder.
- [ ] Repeat profile/hole Save/Apply/Revert, duplex, device selection/availability,
  manual formats/mode/buffer overrides, diagnostics and all routing control workflows.
  Both directional cards remain independent. Confirm summary changes with controls.
- [ ] Footer device/profile/viewed-hole and retained-log-error counts track real
  session data. Warnings remain unavailable until supported severity data exists.
  Clear diagnostics updates errors; no static mockup counts or active format claims.
- [ ] Check Light, Follow System and high contrast, keyboard focus and all tooltips.
  Session-only preferences/persistence limitations remain as before.

## Tester resize/icon regression

- [ ] Routing, Active and Viewed occupy separate lines. Switch profiles, pending
  startup, scan progress and long names: the branding width and commentary-card
  size remain fixed. Full ellipsized status information is available by hover.
- [ ] Drag continuously between minimum and desktop sizes at 100/125/150/200% DPI.
  No header oscillation, stale partial paint, avatar/navigation overlap or commentary
  rerolls. The card stays compact and scrolls overflow. Its heading and all endpoint
  content stay inside the rounded strokes with clear padding on each side.
- [ ] Commentary left/right edges align with Start Routing and Exit AudioHole.
  Its width stays constant as the window grows/shrinks; the avatar follows the card
  with a fixed gap and spare room at the right rather than following the window edge.
  Repeatedly shrink, expand, maximize and restore: no magenta border trails remain.
- [ ] At minimum, intermediate and maximized widths the avatar fills the header
  height with the same top/bottom inset. It never switches to a shorter compact
  rendering. The current source remains uncropped and proportionate, with enough
  width to avoid shrinking its height. Future 512×768 artwork will use a 2:3 region.
- [ ] Inspect a freshly extracted executable in Explorer at several icon sizes,
  plus taskbar/window/Alt+Tab. The file icon uses AudioHole rather than wx stock
  artwork. Windows CI independently audits the first PE icon group and image data.

Native Windows rendering and shell-cache behavior remain manual validation tasks.


## Preview/Test persistence and package milestone (native validation pending)

- [ ] Fresh per-user PreviewBuild root: both checkboxes enabled. Startup choices
  Preview Mode/Test Mode and Preview Build wording match behavior. Test starts
  empty; Preview retains fixtures. Production sentinel files stay untouched.
- [ ] Test: New, duplicate, edit notes/holes/directional buffers, Save, Exit/relaunch.
  Stable IDs and independent reverse values survive. Populate a Test device record
  externally using the documented provisional shape; GUI Save preserves identity.
  Saved device availability is explicitly unverified. Per-device filename rename
  is a future production requirement, not implemented by aggregated Test v1.
- [ ] Test has no Simulate Errors heading/actions/gap and no enabled routing,
  monitoring, beep or live profile Apply controls. Scanning is enabled through the
  real scanner integration described below. Small buffer Apply enables
  for a changed saved direction and persists only that preference; engine values
  remain unavailable. Other staged edits remain unsaved.
- [ ] Toggle startup dialog and next-launch mode, appearance, notifications and
  startup preview settings. Restart and fresh launch preserve values, never resume
  a previously pending route. Cancel unsaved-profile confirmation keeps the session.
- [ ] Restart via Settings closes tray/timers, waits for parent exit and opens one
  new process; rapid repeated requests and normal second launch do not duplicate.
  Test executable paths containing spaces/non-ASCII. Relaunch failure preserves data.
- [ ] Malformed/newer/external-edited configuration reports errors and remains
  byte-preserved after rejected Save. Repair while stopped, restart/reload records.
- [ ] Copy Visitor example folder into personalities, Reload, select it, restart;
  stable selected ID persists. Rename folder without changing identity. Remove it:
  whole-package fallback notice appears. Remove all valid packages: no character
  image or DFA/Bother, app/technical messages remain usable. Restore saved package.
- [ ] Horu uses the supplied owned placeholder, headed Horu. Vanilla is headed
  Status; selector labels remain UwU/Vanilla. Norm/Vanilla use their own preview
  files. No avatar engine playback is present. Bother has no visible button;
  clicking Norm/Horu shows only a reply, with no technical line or needless scrollbar.
  Malformed, duplicate-ID, escaping-path packages fail without preventing startup.
- [ ] Rapid Bother clicks stay bounded; each built-in pool has30 with10/10/5/5
  categories. Warning text retains priority, DFA/routing do not change.
- [ ] Dark footer remains white/legible. Inspect header and endpoint-card rounded
  corners at100/150/200% DPI, compact/maximized/restored sizes; image switches and
  long status/notes do not create resizing artifacts. Verify fixed beep width.
- [ ] Run both Debug/Release native CTest, including Gui.TestModeSmoke and the
  extracted clean package check. OS restart and appearance need human review;
  cross-compilation and portable unit tests do not satisfy these gates.

- [ ] Change only a buffer: small Apply enables, Save/Apply commits only that
  direction, and small Apply disables again. In Preview an active matching path
  confirms its simulated buffer; the reverse direction stays independent. In Test
  relaunch and confirm disk persistence with no running-engine claim. Other draft
  edits and external-file conflict protection remain intact.
- [ ] Type a new buffer value as well as using the spin arrows: small Apply updates
  immediately. Invalid/empty/out-of-range text disables it; profile Save reports
  invalid buffer input rather than persisting a cached/clamped number.

- [ ] Horu's kaomoji stay on one line at compact and enlarged sizes/DPI. Internal
  spaces are nonbreaking only for display; authored JSON/dialogue and selection
  history remain unchanged. Ordinary prose continues to wrap normally.

## Integrated avatar engine

- [ ] Sleep replaces the response with `zzz....`; Norm/Horu occasionally show a
  short dream comment after 45–90 seconds, returning to `zzz....` after six seconds.
  Dreams do not wake the avatar, change routing or send notifications. Input or a
  warning immediately restores awake feedback; Status stays quiet.

- [ ] Norm, Horu and Vanilla load their approved owned animation packs, not the
  old temporary previews. Verify smooth authored exposures, transparent edges,
  fixed512×768 aspect/anchor and full-header placement at100/150/200% DPI.
- [ ] Healthy Running is relaxed idle. Bother/DFA and other fresh responses request
  their authored expressions, then return to the latest baseline. Warnings and
  failures interrupt cosmetic feedback; recovery does not leave stale panic.
- [ ] With **Hold expressions between events** off, a reaction plays normally,
  holds its final pose for two extra seconds and returns to the current baseline.
  Turn it on in Personality settings: the pose remains until another event, and
  another event with the same expression can replay it. Turn it off during a hold;
  it releases after the normal deadline. Restart and verify the setting persists.
- [ ] Try varied DFA replies and references: 1.21 looks surprised, 4.04 confused,
  7.77 celebrating and 9.11 panicking for Norm/Horu. Ordinary replies also vary by
  authored expression. Wording/weights and easter eggs stay intact; during an
  operational warning DFA feedback remains local and cannot replace its avatar.
- [ ] Leave the application untouched for two minutes while Stopped or healthy
  Running: Norm/Horu use their sleeping animation, Status its sleeping still.
  Input or a new reaction wakes it, including when expressions are held between
  events. Scanning, transitions and warnings prevent sleep. Verify reduced motion,
  tray restoration and that polling/paint never resets inactivity or routing.
- [ ] Switch personalities during loading/reactions: no old character/frame leaks.
  Remove/corrupt an owned frame in a disposable copy: owned fallback/hide and
  nonmodal All events diagnostics, with routing/configuration unaffected.
- [ ] Hide to tray/minimize: avatar timer stops. Restore: current playback resumes
  without changing routing or reselecting commentary. Respect Windows reduced
  animation preference. Restart/Exit join the loader without hangs or leaked work.
- [ ] Native CTest runs `avatar_engine` and `Avatar.NativePixels` alongside GUI
  smoke. Extracted ZIP contains all approved owned assets and passes source-hash
  validation. No native test/viewer executable or generation tools are shipped.

## Profile workspace regression checks

- [ ] At minimum, square and wide sizes, Save/Apply/Revert and the pending-edit
  indicator stay visible above the editor. Fill Profiles, Notes and Holes beyond
  their viewport: each scrolls independently, and Save never moves out of view.
  The right routing editor retains its own scrolling. Test 100/150/200% DPI.
- [ ] Profiles and Holes each expose one **+** with descriptive hover/F1 help and
  screen-reader name. Right-click different rows and try Shift+F10/Menu from the
  keyboard: Rename/Duplicate/Delete target that row, while cancelling the menu
  preserves the current editor and staged changes.
- [ ] Rename a profile and hole; verify visible names/pending edits change, IDs
  stay stable, and Save or Ctrl+S persists them in Test Mode across restart.
  Revert discards unsaved renames. Duplicate a hole from a row other than the
  current one: copied endpoints/settings match its target and its ID is fresh.
  Deleting retains confirmation; active/pending profile deletion stays blocked.
- [ ] Launch on a roomy display: the initial window is square. On shorter work
  areas it fits the required header width without forcing a square height.
  Resizing remains free, and Notes benefits from the removed name fields/buttons.


## Real scanner integration in Test Mode (native/hardware validation pending)

- [ ] Devices has separate Capture and Playback lists, no device checkboxes. Each
  matching hidden section has one non-actionable divider; no empty divider appears.
- [ ] Right-click/keyboard context menu targets the pointed/selected stable ID and
  offers Update device, Deep update device, Hide/Unhide. The two scan actions also
  work from visible buttons; disabled/busy/ambiguous targets never substitute hardware.
- [ ] Basic Full, Update and Selected collect real reported evidence. Update skips
  known capability queries while refreshing state/driver metadata. Inspect exact
  format tuples and explicit unsupported/unavailable/unknown values and HRESULTs.
- [ ] Deep Full, Update and Selected require scope-specific consent and honor all
  exposed budgets/mode. Cancel waits for cleanup; UI stays responsive. Confirm short
  stream classifications are labelled limited evidence, never Healthy/Recommended.
- [ ] Hide a device, restart, and confirm hidden status in its JSON near its name.
  It remains below the divider and absent from new configurator/beep options.
  Existing saved references remain intact; hiding does not interrupt a live Preview route.
- [ ] Rename a canonical device JSON file and edit notes/extension fields externally.
  Reload updates the filename display name without changing identity or losing edits.
  Malformed/newer/duplicate records report failure and preserve the last good view.
- [ ] Refresh availability / unplug-return use metadata-only work, preserve scan
  timestamps and do not create devices. Review timestamps rather than assuming a
  loaded record proves current availability.
- [ ] Enter operating context and export a new scan ZIP. Review request, progress,
  reports, canonical records and local bindings. Refusing an existing destination
  preserves it. Inspect both executable imports in the packaged runtime.
- [ ] Test routing/monitor/beep remain unavailable, Preview stays simulated and no
  automated fixture runner executes during a GUI scan. See SCANNER_TEST_MODE.md.

## Taskbar / tray parity

- [ ] With the GUI open, ordinary taskbar right-click offers Close window and
  Exit AudioHole. Verify Open, Start/Stop (Preview) and AudioHole controls… too.
  Controls opens the same tray menu, with profile activation and read-only states.
- [ ] Close window hides to the tray and preserves the active Preview route/draft;
  Open restores it. Exit cancels a scan, stops the session and removes the tray icon.
- [ ] Test Mode has no routing tasks in the Jump List; the shared menu disables
  Start/Stop/profile activation. Taskbar actions never initialize a second backend.
- [ ] Alt+Space and Shift+right-click expose native window controls plus AudioHole
  controls and Exit. Check normal and pinned taskbar use and Explorer restart.
- [ ] After exiting, stale Start/Stop/Close/Exit shortcuts do not create a new session
  or data root. Open follows normal startup preferences. Check paths with spaces and
  Unicode and verify shortcuts target the current running executable.

## Queued scanner and workspace refinements

- [ ] Drag the Profiles width divider and the Profiles/Notes/Holes height dividers.
  Keep drafts intact, scroll each field independently and verify Save/Apply/Revert
  remains visible. Repeat at compact, square and wide sizes and 100/150/200% DPI.
  At a minimum pane size the divider must clamp; enlarge the window or move in the
  other direction to verify available travel without clipping either field.
- [ ] Drag the Devices column and Notes/Overview height dividers. Notes and evidence
  wrap. Long names ellipsize; hover shows the full name. The lists have no unnecessary
  horizontal scrollbar. Check keyboard navigation and status descriptions with a
  screen reader and Windows high contrast.
- [ ] Green means last observed available, gray unavailable, yellow attention or
  unknown. A rejected candidate format or old inconclusive test alone must not
  turn a device red or claim device-wide unusability. Hidden dividers remain
  non-actionable and hidden IDs remain excluded from new configurator choices.
- [ ] Edit a scanner device's name/notes, then Save. Confirm the filename changes
  while ID, Windows name, hidden status, extensions and profile references survive
  restart. Try invalid/reserved names, collisions, read-only files and external
  changes after typing. Failures preserve the draft and report how to recover;
  no existing other record is replaced. Explicit Reload can discard the draft.
- [ ] Scanning offers only Update/Full. Device right-click or Shift+F10/Menu offers
  Update and Deep update for that exact device. Check mouse and keyboard targeting.
- [ ] Deep limits show seconds, default stream `2`, allow `2.001` and arrow steps
  of 0.001 s. Reject excess precision, malformed/out-of-range values and attempt
  limits less than stream duration plus 1 s. Attempt count stays integer.
- [ ] **Ready to scan** identifies scope and suggests closing browsers, Steam/Epic,
  Discord/Telegram and nonessential audio apps, retaining critical workflow apps.
  It explains timing variation and sound/disruption; declining starts no worker.
- [ ] Overview puts status/time/next action first, distinguishes configured and mix
  formats, and retains exact tuple details in Reported formats. Check local time
  and daylight-saving offsets against UTC JSON; unknown stays explicit. Historical
  failures are distinguishable from the latest result; no health claim is invented.
- [ ] While scanning, a console at the bottom follows new output. Scroll up and
  confirm it stays anchored while output arrives. Return to bottom to resume;
  test search/filter changes and bounded-log eviction without controller commands.
- [ ] Export after each scope/depth combination. Check requested scope/ID, forward
  slash ZIP-relative record references, included raw histories or explicit missing
  reasons. Selected-device Basic completion may advance its date despite unrelated
  enumeration errors, but the overall partial result and diagnostics remain visible.

## Current backlog acceptance

- [ ] In Dark, selected Devices names are white on the existing darker-blue highlight;
  Light keeps its lighter-blue selection. Check focused/unfocused and high contrast.
- [ ] Check scrollbars, dropdown arrows/lists, steppers and normal/disabled/hover/pressed
  buttons in Light, Dark and Follow System, including live OS changes and 100/150/200% DPI.
  Older Windows may use its native theme fallback; no system preference changes.
- [ ] Deep fields have equal widths/heights and padded spacing. Enter invalid values
  in buffer, Deep limits, loop count and startup controls: dismissing feedback must
  leave avatar clicks/personality usable and retain any genuine existing warning.
- [ ] Audio/Hole align with the logo bottom, using existing interface cyan/pink.
  Capture/Playback list borders match routing cards. Material Mic/Output icons load
  beside readable labels from the extracted package without installing fonts.
- [ ] Deep progress shows familiar names, direction, device/configuration counts,
  format/mode, setup/stream/cleanup/save stages and a live elapsed clock during waits.
  A partial/cancelled scan does not display 100%; raw IDs remain in exported evidence.
- [ ] Scan limit 0 explicitly disables total batch deadline, retaining attempt/device
  limits and Cancel; use a small finite per-device limit for assessment. Also check
  ordinary finite budgets. Never use an unconsented hardware scan in CI.
- [ ] Export twice without editing the suggested name; collisions ask for another
  name and never replace an archive. Cancel/retry must not trap the personality card.
- [ ] Clear device list defaults to No, names all deleted metadata/history/bindings/
  sessions, and requires a deliberate Yes. With disposable Test data, verify restart,
  empty device lists, preserved profiles/preferences/unrelated files and unresolved
  saved references. Clear is unavailable during scans and in Preview.
- [ ] Test loop count 1 and 3, hold after final repeat, continuous looping precedence,
  and restoring saved count/hold after disabling continuous playback. Check the next
  event (including same expression), real warning, identity switch, reduced motion,
  two-minute sleep/wake and preference restart. Idle-ending artwork may still look
  idle when held; pruning/import is intentionally deferred.
- [ ] Compare a real scan bundle before/after: checkpoint timings, complete raw stage
  histories and external notes/extensions must survive. Measure actual wall time;
  portable tests do not establish a hardware speedup or stronger verification.

## Bundle and selection revision checks

- In Test Mode, export devices and profiles. Open the ZIPs and verify readable
  manifest/device/configuration JSON, stable IDs, notes and exact formats. Import
  into a fresh Test root, review the default-No preview, confirm, restart and
  inspect the restored profiles/holes. Import must never start routing or copy
  local Windows bindings. A conflicting local edit must be reported and retained.
- Check Profiles/Holes selection and every dropdown popup in Dark, Light and Follow
  System, including selected/unselected/disabled rows and high contrast. Selected
  dark rows use white text on blue. Empty source/destination edit surfaces stay
  dark. Type/search/select endpoints and navigate all choices by keyboard.
- With Profiles or Holes focused, press Delete and decline confirmation: no item
  is removed. Confirm a staged hole removal separately; Save commits it. Delete
  in Notes remains text editing. Active/pending profile deletion stays guarded.
- Expand/collapse Advanced directional settings at compact size and scaled DPI:
  the window size stays fixed and the right-hand editor scrolls as needed.
- Scan status reserves three text lines: short/long details do not move or resize
  the log. Dark/light progress fill is green. Deep-limit titles say sec. and hover
  text explains what each limit controls. Name input and Save device share a row.
- Exercise PR #7 reactions with finite loops, hold-final-frame and continuous
  loops, then interrupt with a new event, warning, sleep and personality switch.
  Expressions should retain their expressive final pose rather than idle padding.

### End-of-night layout and discovery follow-up

- Drag the divider below Current scan status taller/shorter, including with Deep
  settings expanded. Successive short and long statuses preserve the chosen height
  and log bounds. Long details remain scrollable; double-click cannot hide a pane.
  The initial status area reserves three lines. Check compact/wide windows and
  scaled DPI, and verify console auto-follow still respects a scrolled-up reader.
- Settings: Appearance/Tray and Startup share two columns. Personality/Animation
  immediately precedes Advanced; Preview Build Environment is last. Check keyboard
  order, wrapped action rows and long data paths, including Vanilla hiding DFA.
- With a physically disconnected saved device, Basic output says cached metadata
  only/live queries skipped. Inspect state/API/HRESULT in exported failed-lookup
  evidence. An absent endpoint with no cached name is an absence observation;
  existing records, names, notes and logical IDs survive. Active-device failures
  and unknown state remain visible errors. Reconnect and verify identity retention.


### Automatic configuration and Advanced Tuning refinement

- New simplex/duplex holes start Automatic with tuning unchecked/hidden. Choose
  Source/Destination; concise summaries and recommendation evidence remain visible.
  Test unknown evidence stays unverified; no stream/clock/latency claim appears.
- Enable tuning: direction-specific format, mode and buffer controls appear in one
  panel. Buffer 0 means Automatic. Set different duplex overrides, disable tuning,
  confirm retained-but-inactive summary, reenable and verify both values return.
  Save, switch profiles and restart Test; policy and values must survive.
- Load a disposable legacy v1/v2 file missing the flag: old active-manual behavior
  is retained. Loading writes nothing. Explicit save produces v3 with the exact
  original in the corresponding non-overwriting backup; unknown fields survive.
  Older application versions reject v3. Test bundle export/import preserves policy.
- Disabling tuning excludes an unsupported retained format/mode from effective
  requests. Enabling rejects unsupported overrides on Apply with clear diagnostics.
  Save/checkbox changes never start/reconfigure routing; explicit Apply is required.
  Individual Automatic fields remain managed while other fields are manual.
- Signal Testing & Monitoring starts collapsed. In Preview, explicitly monitor a
  source and start a beep at a selected destination; collapse/expand repeatedly.
  Operations persist, collapsed header indicates activity, and no new commentary
  occurs. Clear observation stops it without cancelling the beep. Test Mode keeps
  unsupported actions disabled and performance measurements unavailable.
- Norm/Horu react once to settled policy changes with different authored variants;
  Vanilla has facts only. Rapid toggles do not spam reactions. Merely enabling or
  saving Test configuration cannot acknowledge an applied manual override. Preview
  acknowledgement follows a confirmed running application, with simulation explicit.
- Keyboard-tab through basic fields, checkbox, revealed controls and disclosure.
  Hide a focused tuning/tool child: focus remains usable. Assess compact/wide
  windows, dark/light/high contrast and 100/150/200% DPI without top-level resizing.

- Advanced Tuning's Routing Engine dropdown offers exactly Automatic. Save/restart
  and profile bundle transfer retain `routing_engine: "automatic"`. Missing fields
  default to it. With disposable externally edited data, an unknown future choice
  is preserved and explained, ignored while tuning is off and rejected on Apply
  while tuning is on. Explicitly choosing Automatic resolves it without starting
  audio or changing endpoint IDs.
- From a fresh launch, exercise the Test Beep control before and after the first
  monitoring-tools expansion. Start/cancel, expand/collapse during a simulated
  test, then let it finish: button width stays unchanged and idle text returns.
  Check at supported DPI settings with both captions fully readable.


### Information overlap, Return terminology and data-folder controls

- From a fresh launch, select/change endpoints in a simplex hole before touching
  Advanced Tuning. The summary and full evidence share an eight-line viewport,
  with no overlap over the checkbox. Repeat with both duplex directions, unknown/
  incompatible/compatible pairs, long names and window/DPI changes. Long text
  scrolls/copies completely; unchanged polling preserves its reading position.
- Direction/tuning titles, source-monitor choices, beep destination choices,
  failure controls and tray/taskbar direction status say Forward/Return. Save,
  restart and bundle transfer preserve existing `reverse` JSON/identity data.
- Test Mode: Open Devices Folder opens `test/devices`; Open Profiles Folder opens
  the folder containing `configuration.json`. Test a fresh missing directory and
  an inaccessible path. Opening either must not save staged edits or start scans.
  Preview disables these buttons and explains its session-only records.
- Use − beside Profiles/Holes +: decline the default-No confirmation and verify
  no deletion; confirm on disposable records. Profile deletion respects active/
  pending guards; hole deletion remains staged until Save. Compare context menu
  and Delete key behavior. Verify tooltips and screen-reader names for glyphs.


## November 10 profile-builder and GUI checks

- Load a copy of a v3 configuration: no load-time writes. Save creates named
  profiles and preserves the original in local/legacy-profiles. Rename, restart,
  delete, import/export; verify IDs, independent directions, notes and extensions.
- Create multiple profiles/holes; verify numbered default names and live green/gray
  enabled dots. Runtime failures use red. Resize device lists: names expand.
- Select only channels, leaving rate/format/mode/buffer Automatic. Save/restart.
  Map UI channels 1:3,2:4 and verify JSON indexes 0:2,1:3. Invalid input cannot save.
- Collapse/show advanced controls: intent and live routing state do not change.
  Override enable and Reset to Automatic are separate explicit actions.
- Device/Profile Notes start at three rows at the bottom; resizing remains usable.
- Check Application & Tray ordering, Windows startup checkbox and uninstall opt-out,
  minimized launch, Routing Activation labels/disabled zero delay, tutorial default
  No and placeholder Yes. Confirm all message popups follow Windows appearance.
- Low motion shows each animation's first frame, including after personality change.
  Routine system copy uses two lines; warnings remain readable and complete.
- Monitor direction retains its natural height; Material direction chevron and
  return-channel wording render at 100/150/200% DPI.
- Deep confirmation mentions powered-on headsets/monitors/mixers/VR devices.
- Native Windows GUI, Explorer/registry, rendering, and audio hardware acceptance
  cannot be established by a Linux cross-build. Test Mode still has no streaming.


## Current profile contract, Enter and border checks

- [ ] Create basic simplex and return-channel profiles. Save/restart/export/import:
  named files use `audiohole-profile` record v1; bundle payload uses transfer v5.
  Direction settings contain preferences, no duplicate legacy audio settings.
- [ ] Enable overrides, set different source/destination channel requirements and
  mappings. Hide/reopen tuning: values and eligibility stay unchanged. Disable
  overrides: settings and mapping become inactive but saved values remain.
  Reset one direction: the sibling and assignments remain intact.
- [ ] Inherited fields use endpoint → common → Automatic. Null/blank stays
  inherited. An unsupported explicit tuple gives feedback without substitution;
  absent scan evidence does not forbid an Automatic draft. Unknown saved engines
  are unavailable independently of the override switch.
- [ ] Rename a device and press Enter: name saves, identity/references do not
  change. Enter in profile rate/channel/map/buffer saves staged intent, never live
  Apply. Enter on a checkbox/choice commits the settled selection. Invalid fields
  show feedback without saving malformed values. Unresolved endpoint search text
  does not erase an existing assignment.
- [ ] Enter in Device Notes, Profile Notes and Hole Notes inserts a newline,
  without saving. Settings numeric entries commit on Enter. Enter in scanner
  limits/search never starts a scan or saves an unrelated profile.
- [ ] Inspect buttons, list/input/dropdown/group frames and rounded cards in
  Light, Dark and Follow System at 100%, 125%, 150%, 200% DPI. Outlines are thicker
  with readable content/focus/selection, including popup lists; resize/move the
  window and check for shimmer. Confirm high contrast retains usable native
  borders. Keep screenshots and Windows smoke results with the assessment.


## Workflow refinement acceptance

- [ ] Forward and Return each have a separate Advanced tuning disclosure beneath
  their endpoints/information. Open one without opening the other or resizing the
  window. Return's entire section disappears for simplex. Use advanced tuning
  controls eligibility independently; hiding preserves every saved value.
- [ ] Exercise Automatic / Force exclusive mode / Force shared mode separately
  in both directions with Exclusive mode preferred on/off. Forced modes win.
  Save/restart/bundle transfer retains hole-level preference and lowercase routing
  choice. All four engine choices save intent; Test still cannot start an engine.
- [ ] Save different clock choices in Forward and Return. Labels clearly say
  reserved/no current effect. Disabling/re-enabling tuning retains both choices.
- [ ] Right-click saved profiles/devices and Show in Explorer. The actual JSON is
  selected, including Unicode/spaces and after rename. Missing files give useful
  feedback; Preview fixture actions are disabled. No shell launch occurs in CI.
- [ ] On an isolated copied installation, remove AudioHole.has-run and launch.
  Check exact welcome copy, Norm artwork, theme, DPI, long-text scrolling and all
  three button targets. Cancel leaves the marker absent. Accepted launch creates
  it. Replay Tutorial confirms, preserves saved data, resets marker and restarts
  back to welcome. Check unwritable marker error feedback without elevation.
- [ ] With saved devices, unplug/reconnect hardware and disable an endpoint.
  Check named factual messages in Vanilla and authored responses in Norm/UwU.
  Repeated polling, initial loading, renames, hiding and clearing do not repeat
  responses. Unavailable does not imply a successful stream or a physical unplug.


## Current polish acceptance

- [ ] Test and Preview: Routing on startup/Profile on startup are usable. Switching
  Immediately/Manual shows disabled delay 0; After Delay restores/enables the saved
  delay. Editing preferences never starts a Test routing backend.
- [ ] Toggle only Forward tuning/exclusive preference, then only Return. The
  sibling's policy and settings do not change. Each tuning checkbox is first
  inside its disclosure; each exclusive checkbox is below its devices and above
  information. Clock implementation-status text is absent, selection still saves.
- [ ] Wheel over focused/unfocused, closed/open and editable dropdowns across all
  pages. Page scrolls and selections stay unchanged. Explicit mouse selection and
  keyboard navigation still work.
- [ ] Fresh preferences follow Windows; saved Light/Dark choices survive restart.
- [ ] Fresh extracted package: root has only AudioHole.exe/README.txt and folders.
  Probe runs from bin/; licenses/docs/assets manifest are in their new locations.
  First accepted launch creates bin/AudioHole.has-run. Subsequent launch skips the
  welcome even with an old show-preview setting. Replay confirms/removes/restarts.
  Cancellation never resets the marker. Saved profiles/devices are retained.
- [ ] Exit button/tray/taskbar use Exit/Cancel. Cancel leaves routing/scanning and
  edits untouched; Exit stops and closes. Restart/smoke teardown has no extra
  confirmation. Bottom status keeps engine availability without (Test Mode).

### 2026-11-12 endpoint tools and profile workflow

- In Preview and Test, load an unchanged profile: Save is gray. Edit and undo
  profile/hole notes, endpoint, engine and tuning values: Save follows actual
  edits. Enter and Ctrl+S still save immediately without waiting for a UI timer.
- Scroll a long duplex editor and resize its column. Performance Diagnostics
  stays below the editor and above the application status row.
- Confirm Arm Ready and the shared Signal Testing & Monitoring pane are gone.
  Each Forward/Return card owns its own button; capture cards have a meter.
  Devices places its selected-device tools between Overview and Device Notes.
- On a real available Test playback endpoint, explicitly click Test beep. Hear
  one quiet tone from that selected device, then natural completion. Cancel
  before completion; check a second operation waits for cleanup. Try unsupported
  or busy devices and confirm a visible error, no fallback or format/volume edits.
- On a real capture endpoint, explicitly Check level and speak: the meter follows
  input. Cancel, switch tabs, change device/hole/profile, hide, minimize, close,
  Stop or unplug the target: the session ends and the microphone indicator
  releases after cleanup. No background checking starts on returning to a tab.
- No preview/smoke run should emit sound. Preview's meter is marked simulated.
- In a duplex hole, an assigned Forward source is gray in Return source and
  cannot be chosen with mouse/keyboard; current Forward selection remains usable.
  Clear endpoint is last and reusable; clearing frees the device for Return.
  Separate capture/render IDs and assignments in other holes remain allowed.
- Preview: test both Routing Activation preferences and restart persistence.
  Automatic waiting defaults on; Stop prevents device-return rearming. Automatic
  profile switching defaults off. When on, a ready saved profile switches while
  armed; a missing-device replacement asks first and declining keeps the old route.
  Editing preferences alone never starts a route. Test still has no routing backend.

Portable tests exercise state/persistence/cleanup contracts and Windows cross
build checks compilation/linking. Native GUI smoke, DPI/layout, microphone and
speaker behavior require Windows execution; do not infer hardware results from
portable or cross-build success.

### Advanced controls and Settings follow-up (2026-11-12)

- In both Forward and Return, select Automatic, Force exclusive mode and Force
  shared mode with Use advanced tuning both off and on. Each choice remains
  selected, survives Save/reload, and does not change the sibling direction or
  toggle tuning eligibility. With tuning off, preparation still uses Automatic.
- Verify invalid rate, buffer and channel-map edits receive validation while
  tuning is off as well as on. Existing saved overrides remain intact on failure.
- Inspect both tuning panels: no fallback dropdown; buffer field and Apply align
  with equal heights. Apply still saves only that direction's buffer.
- Settings has Application & Tray / Routing Activation, Personality & animation,
  Advanced, and a standalone Restart AudioHole button. The environment panel and
  environment checkbox are absent. Cancel restart with unsaved profile/device
  edits; verify nothing closes or saves. Welcome/replay behavior is unchanged.
- Wheel over the DFA slider, exact-value field and arrows, with and without focus:
  scroll the Settings page without changing the value or commentary. Typed values,
  keyboard arrows, clicked arrows and slider drags still adjust DFA. Arrow height
  matches the numeric field at 100%, 125%, 150% and 200% Windows scaling.
- Run native Preview and Test GUI smoke in MSVC Debug and Release. The previous
  9ff069b failure was C2445 in diagnostic_view.cpp (mixed string-literal/wxString
  conditional), before smoke execution. The diagnostic detail assignment now uses
  explicit wxString branches; do not count a cross-build as native MSVC execution.

### Norm tutorial and pending layout changes (2026-11-13)

- Start with a backed-up/absent bin/AudioHole.has-run. Welcome is unchanged.
  Choose It's my first time: no real inventory appears, no microphone/speaker
  stream opens, and no saved profile/device data is loaded or changed.
- Walk both reply branches. Every narration choice advances once into its full
  response variant. Check exact microphone choices, two distinct explicit rename
  saves, four exact logical assignments despite renamed labels, Return enabled,
  Forward exclusive preference only, Save, Start and Settings acknowledgement.
- The pretend Deep/Full scan runs about 20 seconds; routing activation runs 10 seconds without a progress bar.
  Cancel, retry, change tabs and click Norm while pending: no false completion.
  Dialogue waits for controller completion events and never times out to a new beat.
- Click Norm on a narration beat and an action beat. After about 6 seconds the
  exact previous variant/choices/target/expression return. Click a reply or finish
  the required action during the overlay: it still works once. F1 repeats guidance.
- Highlights match the pink frame, follow scrolling/resizing, remain click-through
  and never appear outside the window. Reply buttons are stacked at Norm's feet;
  appearing/disappearing choices must not resize Norm. Ordinary errors/status,
  scan output and routing notifications must not displace his current dialogue.
- Personality, animation and DFA remain visible but disabled with labelled
  sections. Skip replaces Replay. Appearance/tray/startup changes stay in memory;
  exports/imports, real folders, custom files and startup registry writes are blocked.
- Close early: no completion flag. Confirm Skip: only the flag changes and the app
  closes. Complete normally: flag changes at N035, then free exploration remains
  fictional until exit. Next launch is working mode. Replay deletes the flag and
  restarts welcome/tutorial, preserving real data. Test unwritable marker handling.
- Review larger logo/wordmark/subtitle, lower wrapped status, wider card/buttons
  and centered avatar at compact/wide sizes and 100–200% DPI. Check source meters
  beside buttons at half row width, no tool confirmation labels, personality
  responses in both hole cards and Devices, and visible asynchronous check errors.
- Run all three native GUI smoke modes before package publication. Tutorial smoke
  is a complete actual-control walkthrough, not a nonempty-widget traversal.


### Engine integration and current tutorial polish

- Walk N018A/N018B through right-click Rename. Cancel and unchanged names must
  not advance. Valid non-default names, including trimmed surrounding spaces,
  must advance after confirmation; no response buttons appear for these steps.
- Check all tutorial choices in Dark, Light and Follow System, including new
  choices after a phase transition and hover/pressed/focus states. Verify actual
  emote animation during dialogue and static deadpan without false approval logs.
- Complete the tutorial: End tutorial replaces Skip, final dialogue has no
  implementation-flag wording, and exiting preserves the completion marker.
- Replay/restart during a real session: native streams must reach Stopped before
  the old window closes and the next instance opens. No path-alias or PID-reuse
  assumptions belong to the handshake. Repeat from paths with spaces/Unicode.
- In operation mode, avatar height/top align with the personality frame; the
  wider frame/action row use the freed space. Tutorial retains its larger layout.
  Review at 100–200% DPI, especially fractional scales.
- Settings: Application and Tray contains Restart; Personality and Animation is
  immediately above Advanced. Unsupported automatic reconnect/live-switch
  policies are disabled for engine 0.6 with explanatory help, preserving settings.
- Real hardware: start a saved schema-3 profile with exact scanner bindings;
  confirm Starting, then actual per-path Running/Degraded/Failed/Ready. Check
  source/playback formats, modes, allocated frames, access attempts and frame
  progress. No latency/CPU value may be invented. Repeat the handoff Low profile.
- Test strict Exclusive refusal versus unforced preferred Shared fallback. Keep
  the refusal receipt visible after successful fallback; healthy siblings continue.
- Stop during startup and flowing audio. Another Start/profile Apply must be
  blocked until confirmed cleanup. Save preserves edits for next Start without
  changing the live copied assignments. Device checks/Deep scanning are blocked
  while routing. Hiding the GUI keeps deliberate routing alive; Exit stops it.
- Native MSVC Debug/Release CI must run all Gui.* checks, including EngineSmoke
  (real Runtime/software adapters) and RestartSmoke (actual child process), plus
  the four imported engine suites and controller contracts. Extracted-package
  validation runs the four direct modes and audits Miniaudio's license/runtime.
  Wine and cross-builds are supplemental, not native/hardware confirmation.
