# Native Windows validation — 0.2.6

Run the extracted AudioHole-PlugBoard.exe in an ordinary Windows 11 user session.
The product has no Probe Tools, Diagnostics or session/debug windows. Developer
contract and simulated GUI tests stay in the source/build tree, not the package.
Wine checks are supplemental and do not establish native audio, mixer or DPI support.

## Requested UI checks

1. Verify Last seen ages: just now; minutes (`2m ago`); hours/minutes (`2h 41m ago`);
   whole days from 24 hours; whole weeks from seven days; a while ago beyond four
   weeks. Exactly four weeks can display `4w ago`. Last seen never ellipsizes.
2. Check normal and larger system fonts at 100%, 125%, 150% and 200% DPI in Light,
   Dark, Follow System and high contrast. Both app fields remain one line and
   centered with the icon. Bounded age text can use a slightly smaller readable
   secondary font to fit actual metrics.
3. Use a long playback device name. Its status stays on one ellipsized line.
   Hover the status for the full actual device name; hover the app name for the
   concise drag hint. Check a stream that differs from its saved assignment:
   the status/tooltip must describe the observed output, not requested intent.
4. Confirm Probe Tools and Diagnostics are absent from the header and tray, and
   app menus contain Move to with current sockets. Double-click/Enter opens that
   same menu. No session/debug window or --probe entry point is present.
5. Settings has no working-edit/checkpoint paragraph or drag-instruction block.
   Its bottom Windows Mixer button opens the Windows volume mixer. Automated
   smoke intercepts the exact `ms-settings:apps-volume` URI and verifies that the
   click does not write routing preferences; actual Settings launch needs Windows.
6. Exercise socket ordering, consecutive deletion, System protection, Rename,
   16 colors, all 18 icons, and the drawer overlay. Selections and popups must
   remain readable. At 980 x 680 DIP, check independent scrollbars and drag input.

## Routing, persistence and icons

- Move a consenting application by real drag/drop and through Move to. Observe
  actual playback separately from Windows accepting the preference. Repeat for
  console, multimedia and communications roles, multi-process applications and
  apps selecting their own device. Do not infer routing from the row moving.
- Unassigned playback belongs in a unique matching output socket, falling back
  to System otherwise. Explicit assignments always win, including System.
- Reopen assigned apps while visible/hidden and after restarting the module.
  Verify long inactivity does not expire assignments. Test unavailable/returned
  outputs; no arbitrary substitute or repeated preference setter loop is allowed.
- Check both Selkie startup orders. A late window/custom icon should be found
  within the finite 30-second startup refresh window. Close the app, then reopen
  Plug Board with the app still closed: its learned taskbar icon should remain.
  Repeat at another DPI. A first upgrade needs one successful taskbar observation.
- Save/checkpoints, Save As, board selection/default, profile API links and
  unknown JSON members retain their existing semantics. Backup restoration and
  configuration reload remain service APIs for the future host, not product
  diagnostic menus. Close the standalone app before editing/restoring files.
- Minimize/close/reopen and explicitly Exit. Full session discovery stops while
  hidden; managed start events remain. Exit must finish owned native work without
  resetting Windows preferences. Check Explorer/tray restart and screen readers.
- Test hardware loss/return and reboot persistence on the target build/UBR before
  claiming them. Record endpoint/app settings and actual playback evidence.

## Developer evidence

Run CTest from the build tree. `--board-smoke --smoke-output <isolated-folder>`
uses explicit simulation and current controls, never live preference writes. It
checks actual pointer gestures, durable commands, popup/selection/scrollbar pixels,
status widths/hover bindings in every theme at normal/larger fonts, the absence of
retired navigation, and the Windows Mixer command. Native icon fixtures exercise
real window icon lookup, delayed creation/change, retention and a full 30-second
idle deadline under Wine; one aborting process-metadata stub is explicitly skipped.

Internal `local/diagnostics.json` remains bounded and redacted. The C++ module API
can sample/export structured state for development; no GUI upload/export feature
is shipped. Use a debugger or external process tools for native CPU, memory,
handles, I/O and worker lifetime measurements. Clean hidden operation must wait
on events, with no ongoing full-session/process/icon polling or setter loop.
Shell/RPC calls can block; shutdown has no invented hard deadline.
