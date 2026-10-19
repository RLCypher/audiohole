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
- [ ] Switch Normal, Vanilla, and UwU during an active simulation. Operational facts, labels, warnings, names, and diagnostics stay literal. Normal/UwU commentary varies only on distinct events and avoids immediate repetition; Vanilla stays neutral/constant. Repaint/polling does not regenerate commentary.
- [ ] In Settings → Advanced click DFA repeatedly: only its local acknowledgment changes. Vanilla hides DFA. Routing and diagnostic history remain unchanged. Rerun Introduction without losing session data.
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
  shows guidance, and hides DFA. Restore valid files and select Normal/UwU again.
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
  session-only settings. Introduction repeats it. Check that operation panels,
  top status, tray, and Normal/UwU commentary use production-style language.
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
- [ ] Copy/export diagnostics: the output retains a simulated-data header even
  though the on-screen console previews production wording. Latency, clock drift,
  CPU and hardware verification remain unavailable; no measurements are invented.

Native Windows hover rendering, F1, typed input, popup visibility, theme transitions,
DPI and tray behavior require hands-on validation. Portable tests and cross-builds
alone do not establish these visual/desktop behaviors.

- [ ] Change DFA to ordinary values such as 5.55 and 5.56: each settled adjustment
  selects a response from the button pool, avoiding immediate repetition. Repeated
  notifications at the same settled value do not reroll commentary. At 0.00 verify
  “Done fucking around.” in Normal and its authored UwU counterpart. Other special
  thresholds still select their own responses; controller/diagnostic state is unchanged.
