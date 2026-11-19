# Plug Board integration

The owner-authorized integration embeds the supplied AudioHole Plug Board 0.2.6
view between Profiles and Devices. Its horizontal socket workspace, settings
drawer, drag and context actions, board library, policy reporting and native
backend are reused. No standalone launcher, second frame, second tray icon,
probe harness or fixture executable is shipped.

## Host contract

`Frame` owns one application-lifetime `plug_board::Module` and a parent-owned
`desktop::PlugBoardView`. The module's control thread owns persistence and
Windows policy calls; its discovery, process and icon workers retain their
original ownership contracts. The GUI supplies value snapshots and reads
immutable publications. Nothing runs on an audio callback.

- Profiles, Plug Board, Devices, Scanning / Diagnostics and Settings share the
  named `AppPage` order, including keyboard navigation and tutorial cues.
- Tray, Windows system menu and taskbar Jump List expose **Plug Board** through
  the existing bounded, same-executable shell relay. This opens the existing
  window/tab and issues no audio-engine commands.
- The scanner remains the source of all known playback endpoints: logical IDs,
  exact local Windows bindings, availability and ambiguity. Unavailable and
  hidden saved devices remain resolvable; friendly names never select bindings.
- Three read-only default-role queries populate System. MMDevice notifications
  refresh those roles on the module control thread, including while hidden.
  The standalone inventory enumerator is disabled; these notifications cannot
  substitute a second native endpoint list for the host scanner.
- Profile descriptors include saved profiles. Only an armed, committed
  Ready/Running/Degraded activation, after pending admission finishes, sends a
  new activation generation. Viewing or editing a profile does not activate a
  board. A successful re-Apply has a new immutable live intent. Failed switches
  do not activate the proposed board. Successful deletion explicitly removes its
  association; a partial descriptor reload never implies deletion.
- The visible flag requires the selected tab, a shown host and no minimization.
  Full session discovery and view timers stop otherwise. Managed process-start
  subscriptions continue, preserving the module's sticky assignment behavior.
- Host Light/Dark/Follow System and high contrast feed the module appearance
  after the host palette pass. Icon sizes follow physical DPI. The module's
  existing dialogs and native control painting remain in use.
- Exit detaches the view and begins shutdown, then keeps the GUI event loop
  responsive until both engine cleanup and the module shutdown future finish.
  The destructor remains an owned-worker join fallback for non-vetoable exit.

## Persistence and policy meaning

The supplied portable `audiohole-plug-board` schema version 1 remains
unchanged, including unknown members, checked replacement, conflict protection,
separate saved/working boards and stable profile/application/socket IDs.
The host places `library.json` under its current working data root's
`plug-board` directory. Disposable history, diagnostic and icon data live under
the sibling `test-local/plug-board` directory. Existing standalone-module data
is not migrated implicitly. Existing device/profile JSON and engine settings
are unchanged.

Assigning an app saves desired policy before attempting Windows changes. An
accepted Windows preference is distinct from observed current stream location;
an already-open application may need to recreate its stream. The supplied
Windows adapter reports unsupported capability on systems where its known
interface is unavailable. Wine validation is supplemental and cannot establish
native Windows policy support or actual playback movement.

## Personality and tutorial

The two supplied response files are preserved in `docs/plug-board` and imported
verbatim into the external Norm and Horu catalogs: ten events, four responses
per personality per event. Each has a stable response ID, its authored expression
and idle fallback. New stable Vanilla facts distinguish desired assignments
from live playback. One successful edit generates one response; unchanged
refresh, discovery, policy retry and startup do not select new random chatter.
Mixer commentary follows successful launch of the Windows mixer URI.

The Norm-only tutorial uses N033 as its pre-Plug Board transition, with both
incoming N032 choices and both outgoing PB001 choices selecting complete authored
lines. PB001 waits for actual tab activation. Socket creation includes confirmed
popup naming; PB005/PB008's later rename steps are retired. Progress requires
committed named creation, endpoint selection and app assignment using stable IDs.
Socket names are user-chosen. PB-R09 returns a lost required socket to its own
creation step and preserves the other socket. PB013 permits exploration, then
revalidates both sockets, their outputs and their app assignments on Settings
navigation. PB014's bridge and N034 appear as one message with N034's expression
and choices. Completion rechecks the current practice setup before N035.
Tutorial enables the view's optional distinct-name policy in the existing
creation popup. Blank or duplicate confirmation submits no command; ordinary
hosts retain the default naming behavior and the policy reducer is unchanged.
Highlights use the actual drawer button, socket heading, endpoint control and
virtual application row geometry. Optional exploration follows PB012.

The scan now contains nine fictional devices: the original two captures and
two playbacks, plus BoomSlab 9001 and MixBoard Music/Game/Chat/Aux. The original
MixBoard capture is named **MixBoard Mic Input**. Six fictional applications,
including Spooty and Felony Jaywalking LXVII, begin in System on BoomSlab.
Owner-supplied icons are packaged as small RGBA PNGs with hash provenance.
Committed tutorial assignments also update the fictional session observations,
so the practice rows show the chosen MixBoard outputs. This is sandbox-only;
native Windows session observations continue to come from the native backend.

Tutorial and all GUI smoke hosts construct a simulated backend and memory-only
checked storage. They never resolve a working configuration root or read/write
library, history, diagnostic or icon-cache files. Library restore and diagnostic
export are rejected in this mode. The Windows mixer URI does not open Settings
from the tutorial. Guided dialogue suppresses unrelated random commentary.

## Verification

The imported contract/library tests remain enabled in the host build. Added
checks cover memory-only file isolation, supplied fake artwork and default-role
notifications preserving scanner bindings. Host model tests cover the new
catalogs and extended tutorial. Native GUI smoke exercises actual embedded
controls and the tray shortcut; TutorialSmoke covers the complete original
lesson plus the Plug Board insert. Existing package allowlist checks include
the tutorial assets and required Material Symbols license/notice.

The exact validation results are recorded in `docs/plug-board/VALIDATION.md`.
Original handover source hashes and provenance are retained under
`docs/plug-board`. Local adaptations are the embedding build switches,
memory-only storage/backend guard, read-only System role refresh, simulated
artwork injection, virtual-row geometry helpers, guided creation-name validation
and an icon test fixture that
reads the host executable instead of the standalone launcher; the tested module layout
and policy reducer remain intact. No audio engine changes are included.
