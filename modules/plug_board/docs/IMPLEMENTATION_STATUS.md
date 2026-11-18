# Standalone module 0.2.6

The owner authorized the full standalone module with diagnostics after the first
native read-only inspection. This superseded the earlier stop-before-GUI milestone
gate. Final integration into AudioHole remains separate and has not been done.
All changes are isolated under `modules/plug_board/` and `docs/plug-board/`.

Implemented: portable application identities; board/socket commands and checkpoints;
strict, forward-preserving JSON and checked atomic storage/backups; asynchronous
service and immutable snapshots; visibility-scoped event-driven session discovery;
verified process tokens and current-user managed process-start events; bounded native
icon extraction/cache; per-process/per-role preference/readback reconciliation;
fixed socket columns, independent lists, overlay settings drawer, context menus
and drag/drop; profile API hooks; standalone tray/lifecycle; structured diagnostics,
resource samples, redacted export and recovery controls. The normal executable
opens the board without arguments. The current product has no diagnostic/probe
navigation; internal logs and source-only developer tests remain.

Verification performed for this handoff:

| Check | Result |
| --- | --- |
| Linux portable contracts + library/storage/service | 44 + 68 tests passed |
| AddressSanitizer + UndefinedBehaviorSanitizer, leak detection | Same 112 tests passed |
| Fresh isolated source-copy build | Same 112 portable tests passed; implementation/build files match |
| Windows x64 Release cross-build, static runtimes | Passed; no compiler warnings in final build |
| Windows portable contracts/model under Wine | 44 + 67 passed; POSIX-only symlink test excluded |
| Windows metadata/evidence/backend under Wine | 24 passed; one native process-metadata test explicitly skipped |
| Actual board controls under Wine/Xvfb | 532 assertions passed; includes status/font/hover and mixer checks |
| Packaged argument-free real-backend launch and actual Exit button | Passed under Wine; owned shutdown and local log retained |
| Schema/example | Published example validates against the standalone schema |

The board suite exercises initial System-only state, grouping and inactive counts,
stable selectors and native wheel protection, constructed menus and installed drop
targets, malformed payload rejection, all-role policy receipt projection, minimum
geometry, input above the overlay, drawer scroll/size preservation, keyboard
reordering, inactive accessible actions, Save/Save As/remapping, rename, default and
profile links, confirmed removals, current-control names/help, session dialog,
diagnostics/export/redaction, and hide/reopen visibility. It injects real Windows pointer down/motion/up events for app and socket drags
and context menus. Audio policy is explicitly simulated. It does not establish
native Windows audio, device or DPI compatibility.

The Windows backend test samples real process resources and confirms a hidden
host-mode adapter with no managed assignments owns no session/process worker,
enumeration or policy write. Managed assignments now retain a start-event watch.
Wine is supplemental; native writes are intentionally disabled there. Wine's
window property-store entry point is an aborting stub and is bypassed there;
WM_GETICON pixel priority and hidden/tool-window/PID-reuse rejection are exercised.
The user's older ZIP proved read-only inspection, as documented separately. The
owner subsequently reported that live routing and the corrected application icons
can behave as intended on their PC, then reported icon regressions depending on
app/board startup order. Version 0.2.5 addresses those lifetime gaps; the new
startup/closed-app cases still need target-PC confirmation.

This environment has not independently verified native Windows routing/restoration,
real application behavior and icons, hardware loss/recovery, native WMI/COM teardown,
reboot persistence, screen readers,
Explorer/tray restart, multi-DPI/monitor gestures and resource comparisons remain
unverified. There is no invented native performance budget or Windows build matrix.
Use `NATIVE_VALIDATION.md`; the retired product Diagnostics UI is no longer an
evidence collection path. Developer/host APIs and bounded local logs remain. The ABI is
private and capability failure remains visible with desired intent preserved.

Handoff archives include the single GUI executable at ZIP root, matching source
hashes, static-runtime import inventory, licenses, screenshots and raw check logs.
No probe/fixture/test executable is installed. Earlier version sections below are
historical and do not describe current diagnostic navigation.
`SOURCE-MANIFEST.json` identifies the uncommitted module against base commit
`39f860e5b752de13046f0d04a55054822d3602cb`. No host files were changed or merged.


## Corrected input and placement in 0.2.1

The owner clarified that an unassigned application with a corresponding output
socket belongs there, rather than in System. A pure read-only projection now
implements this without taking policy ownership or changing the saved JSON.
Explicit assignments win. Known partial observations remain marked partial and
never establish successful routing.

The mouse bug came from calling wxWindow::HitTest, which returns a window-region
code, rather than VirtualHitTest, which returns a list row. That blocked context
menus and could read beyond the row vector on mouse down. Every pointer handler
now shares a bounded row lookup. Drag state remains live until the native OLE
loop unwinds, source widgets use deferred destruction, and unrelated catalog
updates do not invalidate a drag when the app identity/board are unchanged.

The board suite now injects actual Windows right-button and left-button down,
pointer moves and button-up through the production control/event path. It uses
simulation for audio policy only. An independent X11 pointer reproduction changed
no assignment before the fix and correctly moved the app after it. Both ran under
Wine/Xvfb; native hardware/audio and DPI claims remain unverified.


## Version 0.2.2 changes and focused evidence

- Taskbar/window metadata and WM_GETICON precede executable engine resources.
  Visible-window ownership is verified; copied icons remain on the worker. The
  WindowIconWins test exports pixels from a distinct red window icon and checks
  the source, size and pixels, with separate hidden/tool/PID-reuse checks.
- Rows have two centered lines: name plus Playing / Last observed. Policy
  acceptance and observation comparison remain available in Sessions/Diagnostics.
- AudioHole identities/helpers and the hosting image are excluded from discovery,
  saved-row projection, watchers and live policy scheduling. Display labels do not
  identify the host. Existing saved records remain intact but hidden/unmanaged.
- Native dropdown items/arrows/popups and scrollbar tracks/thumbs use the selected
  app palette, retaining native interaction/accessibility and system high contrast.
  The smoke suite checks actual native pixels in Light/Dark, opens real popups
  and preserves dropdown wheel protection. Screenshots were inspected.
- Explicit assignments have no expiry and need no Save checkpoint action. A
  previous success keeps its managed process-start subscription. Portable tests
  cover repeated hidden restarts, a 2010 activity timestamp after module shutdown,
  and an already-running app when the module starts. WMI remains event-driven;
  native provider availability/latency and reboot behavior still need Windows.
- Sockets-list drags own capture instead of delegating to the native selection
  loop. Actual pointer tests verify up/down order, main columns, immediate JSON
  persistence, System protection, Escape cancellation and click-without-edit.
- Both socket surfaces open Rename/Delete menus; audio sources expose Move to
  with every current socket. Smoke inspects the actual constructed menu and its
  enabled/submenu states, then chooses actions through injected presenters. After
  removal, the neighboring custom socket stays selected. All five custom sockets
  are deleted consecutively through the current controls, leaving only System.


## Version 0.2.3 changes

The owner requested themed popups and readable selections, corrected taskbar icons,
16 per-socket colors, selectable Material icons and app names without .exe.

- Confirmation, rename, choice, file and nested validation popups use a scoped
  app palette. Generic message dialogs preserve original button IDs, labels and
  safe defaults. The real socket-delete action is tested with No, without edits.
- Lists, dropdowns and menus paint matched selection pairs. Popup smoke checks
  actual pixels and text/background contrast in Dark, Light and Follow System.
  The generic file browser's filter dropdown uses the same themed choice; a test
  changes JSON to All files and verifies the displayed directory entries change.
  Dark/light popup screenshots were reviewed. Actual Windows high contrast,
  multi-monitor DPI and live OS appearance transitions remain native checks.
- Recolor offers 16 colors plus theme reset. Icon offers 16 Material Symbols plus
  None. Every choice is exercised through the constructed context menu and its
  persisted model. Header text contrast, icon rendering, resets, Save As,
  forward-field preservation and absence of policy writes are checked. Icons
  are bundled native vector paths derived from pinned-by-hash Google SVGs under
  Apache-2.0; no SVG library, font installation or runtime download is needed.
- App views omit a trailing executable extension; saved names/identities and
  routing still retain their original values. Metadata redaction includes the
  selected shortcut path while leaving the icon kind and match basis readable.
- Taskbar ICON_SMALL2/ICON_SMALL and small class icons precede large engine icons.
  Separate real-window fixtures supply different large/small and class icons;
  pixel/source checks verify the small icon wins. Real .lnk tests cover exact
  target matching, verified AppID priority, unrelated images and ambiguity.
  Local shortcut indexing is bounded, worker-owned and invalidated by Refresh
  or reopening the board. The owner subsequently confirmed the corrected Selkie
  icon matches the taskbar, with a screenshot.

The portable suite contains 109 tests; the ASan/UBSan and isolated source-copy
builds run the same suite. Windows/Wine runs 108 portable tests plus 19 native
backend/icon/evidence tests, with one native process-metadata test skipped for
Wine's aborting stub. The board smoke includes 476 assertions and the separate
probe smoke includes 44. Input checks use real Windows mouse events with a
GUI-thread mouse-up acknowledgement; this local hook exists only in smoke mode.
No live routing call, global setting write or background polling was introduced
by appearance or display-name changes.


## Version 0.2.4 refinement

PC and Laptop are appended to the socket icon catalogue (18 icons total), with
original Google Material Symbols SVGs, hashes and native vector data. Existing
appearance IDs remain stable. Menu ID reservation derives from catalogue sizes.
Application hover text is exactly `Drag to assign to a new socket`. Current
control smoke checks the text and exercises every offered icon. The owner has
confirmed the earlier application-icon corrections work on their Windows PC.

For this refinement, the current board suite passed 487 assertions under Wine,
including all 18 icon options and the exact application hover text.


## Version 0.2.5 icon lifetime correction

The first successful executable/window lookup no longer freezes the icon during
startup. The Shell STA performs at most five finite follow-ups through 30 seconds,
including after a window already supplies its initial engine icon. A per-process
lifetime record prevents session metadata churn from extending that window.
Pending source work is canceled on Hide/Refresh/shutdown, and epochs/sequences
reject replaced work. There is no permanent window-polling loop.

Displayed images survive source invalidation, view reopen, process/session changes
and DPI updates while replacement requests run. Learned taskbar pixels are saved
in a bounded, atomic machine-local JSON cache keyed by full identity and file
write-time/size. Closing an app or restarting the module can use those pixels
instead of the executable engine logo. A weaker class icon cannot overwrite a
previously learned window/taskbar override. Closed-app DPI scaling stays on the
STA. Corrupt/future cache bytes remain intact, diagnostics report the failure,
and live lookup continues. Routing intent and portable schema are unchanged.
Icon callbacks use stable catalog keys so a delayed result can update the row
even after its audio session ends; a live observation is not required to finish
an already-requested image lookup.

Regression coverage includes the model's close/reopen/new-process request behavior
and native fixtures for discovery before a window exists, a later WM_GETICON
change, closed-app worker restart/DPI, full-image separation and corrupt-cache
preservation. A real 30-second production-window test checks that metadata churn
does not restart periodic refreshes. The native fixtures run under Wine and do
not establish Selkie or Windows Shell compatibility on the owner's PC.


## Version 0.2.6 presentation and tool removal

- Last observed becomes Last seen, with compact units, hours/minutes and bounded
  whole-day/week labels. Ages beyond four weeks use a while ago. Boundary and
  overflow tests preserve timestamps and assignment semantics.
- Rows keep two physical lines. Last seen text fits without ellipsizing across
  the actual column width, all palettes and normal/larger fonts. Long observed
  playback names stay single-line/ellipsized; their hover region exposes the full
  device name, while the app name/icon retains the short drag hint.
- Probe/Diagnostics header and tray navigation, session dialogs/debug app actions
  and the probe entry point are removed. The normal app no longer links the
  harness; helper/test executables are not installed. Legacy CLI fixture sources
  require an explicit developer option. Internal log and service contracts remain.
- The checkpoint/working-edit and drag-instruction text blocks are removed from
  Settings. Its Windows Mixer button opens the exact Settings URI through an
  interceptable Shell callback; smoke verifies no routing preference is written.
- Obsolete probe/session/diagnostic UI assertions were replaced by current-control
  absence, complete accessibility traversal, status geometry/hover and mixer
  dispatch assertions. Routing, identity, persistence, confirmations, pointer
  input, popup themes and consecutive deletion checks remain intact.
