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
  scan, monitoring, beep or live profile Apply controls. Small buffer Apply enables
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
