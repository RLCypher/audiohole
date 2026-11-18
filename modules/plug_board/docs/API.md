# Module API and host integration

The standalone module has its own CMake entry point. No root AudioHole target,
engine, scanner, profile model or GUI source has been changed. Linking and wiring
the host remains a separate integration decision. The API is C++20, internal and
not a stable binary plugin ABI or external network service.

Targets are `audiohole::plug_board_core`, `audiohole::plug_board_model`,
`audiohole::plug_board_windows`, and `audiohole::plug_board_wx`. Public headers are
under `include/audiohole/plug_board`. wxWidgets is needed only for the view.

Create `Module` for application lifetime with explicit portable/local roots and a
backend factory. The factory, initialization, policy, persistence and destruction
run on the module's control thread, never the caller/GUI/audio thread. On Windows,
use `make_windows_backend(false)` in an integrated host. Leave
`standalone_inventory=false`: the host supplies its existing scanner's render-only
`EndpointInventory` through `set_inventory`. Retain saved unavailable descriptors,
logical IDs, exact local Windows bindings, ambiguity and three role defaults.
Friendly names are presentation only. The standalone executable opts into the
native inventory shim; it does not probe device formats or open audio streams.

```cpp
// Host initialization, outside audio callbacks; module outlives every view.
ModuleOptions options;
options.data_root = portable_root / "plug-board";
options.local_root = machine_root / "plug-board";
options.backend = [] { return make_windows_backend(false); };
Module module(std::move(options));
module.set_inventory(render_inventory);
module.set_profiles(profile_descriptors);
auto* view = new desktop::PlugBoardView(tab_parent, module);
view->set_host_visible(plug_board_tab_is_actually_visible);
```

Supply the committed active profile at construction or call
`profile_activated(profile_id, committed_generation)` only after the host confirms
activation. Selecting/editing a profile is not an activation. Generations prevent
duplicates or older activation completions from overriding a manual board choice.
`profile_deleted` removes that link; an incomplete `set_profiles` list never
silently deletes associations. Multiple profiles can share a board. Deleting a
board confirms affected links and atomically redirects them to the surviving
default. The last board is protected.

Commands are typed values from `library.hpp`. `submit` returns a future with
`ok`, `durable_intent`, structured code/message, operation ID and resulting
revision. Optional expected library revision **and selected board ID** bind an
edit to its UI context, including nested modal loops and OLE drags. An accepted
command is durable intent, not proof that a native preference or stream moved.
Inspect each application's `LivePolicyReport`: before/write/readback per process
and role are independent of `observed_routing`, freshness and active endpoints.
Failures/partial/unsupported outcomes are retained without an automatic setter
loop. `RetryRouting` is an explicit retry. Device binding return/new verified
processes can reconcile pending intent. No global reset is exposed.

Snapshots are immutable shared values. `subscribe` runs callbacks on the service
thread; enqueue a coalesced GUI event using a lifetime-safe mailbox. Unsubscribe
prevents future copies but an in-flight callback can finish, so use a weak sink.
No COM pointer/HICON/HWND/native process handle crosses through the model. The
view converts copied RGBA to wx bitmaps on the GUI thread. Icons are requested for
visible rows and cached; no background PNG files are created by live discovery.

The host owns visibility. Tab changes, hidden/minimized windows and tray closure
must call `set_host_visible(false)`. This stops full session subscriptions, UI age
timers and pending icon requests. It retains saved policy and, when necessary,
current-user process-start events for all managed identities, including accepted
assignments. These subscriptions are event-driven and have no inactivity expiry. Endpoint inventory remains host-owned.
Destroyed/detached views unsubscribe and delete queued GUI events. Detach/destroy
views before the module. `ViewOptions` can inject host button/choice factories;
the choice must retain wheel protection. `set_appearance` accepts host colors.

`sample_diagnostics` requests process CPU/time, memory, handles and I/O on the
control thread; it is on-demand only when a host/developer explicitly requests it; the standalone no longer has
a diagnostics page. Cumulative native-operation counters distinguish enumeration, events,
icons/cache hits, process snapshots and actual private API read/write calls.
Resource measurements exclude WMI providers and other processes. CPU deltas use
the native sample's monotonic tick, never incidental snapshot publication time.
`export_diagnostics` serializes off the GUI thread and defaults to redaction. A
host diagnostic sink must be non-audio, nonblocking and must not re-enter Module.

On exit, detach views and call `begin_shutdown`. Keep the event loop responsive
until its shared future completes. The service stops subscriptions/watchers,
joins owned workers, flushes dirty state, releases COM in owning apartments and
reports cleanup/storage failure. It leaves Windows application preferences in
place. Native Shell/RPC calls are cooperatively canceled between operations; a
blocked operating-system call has no guaranteed hard shutdown deadline. The
destructor joins as a fallback and therefore does not belong on an audio thread.

Simulation is explicit through `make_simulated_backend`; it has no Windows API
implementation and never counts as native evidence. `--demo` uses a separate
root. `--board-smoke` injects that backend and deterministic modal answers while
dispatching the real view's current control/menu/drop handlers.

## Observed placement and input diagnostics (0.2.1)

`application_placement` in `presentation.hpp` projects an unassigned app into the
unique existing socket whose available playback binding equals its fresh active
output. Saved assignments take priority. Missing or duplicate matches, stale
history and multiple observed outputs fall back to System. This changes display
placement only. Diagnostics exports both the intent's `socket_id` and the
`display_socket_id`/`placement_basis`, along with observation completeness. A known
output from partial discovery can be displayed without establishing a route match.
`record_view_event` queues UI gesture/outcome evidence into the bounded log;
it never enrolls applications or submits native preference work.

## Presentation and sticky assignments (0.2.2)

`PlatformBackend::internal_application` excludes branded AudioHole executables,
module helpers and (in the native backend) the exact running image even if renamed.
Discovery, saved-record presentation, alias watches and policy scheduling all apply
this filter. Display names never identify the host. The native live policy adapter
also rejects its own PID and branded images; isolated fixture experiments retain
their separately scoped allowlist.

`watch_processes` now covers every managed, resolved application identity, rather
than only PendingProcess. `find_processes` is a finite snapshot and must not
replace that subscription with its lookup subset. Register the full subscription
before startup snapshots. On each managed start, refresh that app's live tokens,
then reconcile its saved socket. Accepted receipts never unsubscribe an assignment.
No session, activity age or icon eviction changes the durable assignment. Startup
reconciles existing managed apps even with no attached/visible view.

App rows draw only a name and one Playing / Last observed line. Policy receipts
and observation comparison remain in snapshots, Sessions and Diagnostics. The
32-DIP icon and measured two-line text block share a vertical center, with DPI
resampling on the icon worker. `set_appearance` applies popup item/arrow colors,
native child themes and nonclient scrollbar colors as well as normal wx children.
Native keyboard, pointer tracking and accessibility remain Windows-owned. High
contrast uses system colors. There is no idle scrollbar animation timer.

The icon STA first checks a bounded set of visible, uncloaked taskbar candidates
whose current-user image/PID creation evidence matches the app. It prefers
RelaunchIconResource, WM_GETICON, window AppUserModelID, then class icon before
executable/session/generic fallbacks. Another window-owning process is accepted
only for the same complete canonical Win32 image, never a title/parent-PID guess.
Borrowed HICONs are copied and destroyed on the worker; only RGBA/source labels
reach the model. Window handles and icon metadata signatures key runtime caches
and are not persisted. Refresh/reopen clears the model image cache to recheck
late-created/changed windows. There is no continuous window polling/hook. Explorer
pinned-shortcut overrides may remain unavailable.

Both socket headings and Sockets-list entries open a Rename/Delete context menu.
System's Delete is disabled and rejected by the command model. Menu cancellation
never opens Rename. Audio-source menus contain a Move to submenu in current socket
order (plus Sessions/reset/retry diagnostics actions). `ViewOptions::menu` receives
the actual constructed `wxMenu` and flattened action labels, allowing injected
presenters/tests to inspect submenu structure and disabled states before selection.


## Appearance, modal scope and taskbar source corrections (0.2.3)

`SocketAppearance` is optional presentation intent. `SetSocketAppearance` accepts
an optional color and/or icon, preserving the other property and unknown members.
It persists a working edit without policy reconciliation. The 16 color/icon IDs
are in `socket_styles.hpp`. Header foregrounds are chosen from actual RGB contrast;
high contrast uses Windows colors. Material paths are embedded from checked-in,
SHA-256-recorded Apache-2.0 source SVGs and drawn with wx's native graphics renderer.
The view does not depend on installed Material fonts or wx SVG support.

Popup helpers install a GUI-thread modal scope. Generic dialogs theme after lazy
child creation; nested native wxMessageDialog calls are re-presented generically
with unchanged styles, labels, safe defaults and return IDs. The file browser is
wx's own generic implementation from the same pinned 3.2.6 source, compiled without
modifying framework code. Lists, menu items and dropdowns paint paired selection
colors; native report/list controls retain items, icons, callbacks and accessibility
while selected rows receive explicit colors. No audio work occurs in painting.

The standalone frames handle Windows' ImmersiveColorSet setting-change message
and re-read the selected preference, with no polling or OS preference changes.
Open owned dialogs receive the resolved palette. `application_display_name` only
removes a trailing .exe for presentation; it never participates in routing identity.

The icon worker now indexes local pinned, user Start Menu and common Start Menu
shortcuts once per source epoch. Refresh/reopen invalidates that index and decoded
images. Traversal is bounded to 4,096 entries, 512 link loads and depth 6, skipping
reparse points and network roots. Shell links are read without Resolve or launch.
Explicit custom icons require a complete canonical target image or exact verified
AppID; pinned folders take priority and conflicting equal-priority icons are not
chosen arbitrarily. Window ICON_SMALL2/ICON_SMALL precede ICON_BIG. Relaunch and
verified shortcut/AppID metadata precede generic engine/executable fallbacks.
Diagnostic image evidence reports the window kind, match basis, shortcut path and
resolution revision; paths and raw AppIDs remain redacted by default. Native
Windows taskbar behavior still needs target-PC checks.


## Socket icon additions and concise hover help (0.2.4)

The icon catalogue now contains 18 entries, appending PC (`desktop_windows`) and
Laptop (`laptop_windows`). Embedded vector paths are appended in the same order;
existing IDs keep their meaning. Context-menu command IDs reserve the complete
leaf count from both catalogues, including reset options. The UI smoke iterates
all icons, including the two additions, and checks persistence/rendering. App
hover text is exactly `Drag to assign to a new socket`.

## Application icon lifetime correction (0.2.5)

`Module` supplies its machine-local root through the optional
`PlatformBackend::set_icon_cache_root` hook before requesting images. The Windows
icon STA owns `icons.json`; no GUI or audio thread reads or writes it. Only learned
taskbar-derived pixels are persisted, with bounded entries/bytes and checked atomic
replacement. The memory/file key uses canonical image/AppID and executable file
metadata. It excludes transient PID/window/session evidence and requested DPI;
cached pixels are resampled on the STA when needed. Executable/stock fallbacks
cannot overwrite learned taskbar icons. Diagnostics marks `retained_taskbar` and
`current_fallback_source` when the window is unavailable.

Displayed images and request fingerprints are separate. Reopen, Refresh, DPI and
new process evidence invalidate requests while retaining usable pixels. A visible
live request schedules at most five follow-ups, at 250 ms, 1 s, 3 s, 10 s and 30 s
from the initial request, even if its first lookup found a window/engine icon.
Missed deadlines coalesce; there is no recurring window scan after that finite
startup window. Hide/refresh/shutdown cancel pending work, and source epochs plus
request sequences reject superseded work. Only copied owned RGBA reaches snapshots.
Icon-only observations carry an `image:<catalog-id>` request key, so completion
does not depend on an audio session still being present. Catalog removal or an
identity change retires that request alongside its image.


## Compact status and simplified product UI (0.2.6)

The standalone frame has one board workspace. Probe/Diagnostics navigation, tray
diagnostics and session/debug menus are removed. Application menus contain only
Move to; double-click/Enter uses the same command menu. The app no longer links
the probe harness. CLI fixtures are an opt-in developer target and are never
installed; test binaries stay in the build tree. Internal diagnostics, checked
configuration recovery and resource sampling remain service APIs for developers
and the future host, with no autonomous diagnostic polling introduced.

`last_seen_text` formats the bounded row-age vocabulary and safely saturates
elapsed additions: minutes, hours/minutes, whole days at 24h, whole weeks at seven
days, and a while ago strictly beyond four weeks. Exactly four weeks is `4w ago`.
Unknown activity is `Last seen: unknown`. Dates/assignment persistence are untouched.
Rows retain two physical lines. Last seen is never ellipsized; native font metrics
can choose a slightly smaller readable secondary font. Playback labels retain
single-line ellipsizing and a status-specific full observed-device tooltip. The
name/icon area keeps the concise drag hint. No app-wide long tooltip is restored.

Settings omits the checkpoint-status and drag-instruction blocks. Its bottom
Windows Mixer button opens `ms-settings:apps-volume`. `ViewOptions::open_url` lets
a host route that action through its Shell integration; the default uses the
Windows URI handler. Smoke intercepts that exact request and verifies that no
routing setter is invoked. A failed launch reports an ordinary themed error.
