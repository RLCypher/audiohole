# AudioHole Milestone 0.2 manual acceptance

Use the **Release** artifact for the exact commit under review. Extract the ZIP, read `build-info.json`, and launch `audiohole.exe`. Visual inspection and real desktop/tray behavior require a Windows 10/11 x64 user session. This is a GUI evaluation: **no real devices, audio, or persistent configuration**.

- [ ] Launch outside the source/build tree on a PC without Visual Studio. Confirm the simulation banner, commit metadata, four pages, readable controls, keyboard navigation, high-DPI scaling, resize/scroll behavior, and clear error dialogs.
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
