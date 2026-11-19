# Native view implementation

Frame owns one ApplicationController decorator around the selected backend and one
Tray. wxWidgets owns child windows, sizers and event bindings; widget pointers are
borrowed and must not outlive Frame. Tray uses the same controller. GUI, catalog,
configuration and controller calls belong to the control/GUI thread. This is not
an audio-callback interface. The optional bounded post queue is for non-real-time
adapters, and introduces no worker or external transport.

View builders remain separate: profile_view owns editor drafts independently of
saved records/live plans; device_view never enumerates Windows endpoints;
diagnostics_view submits typed scan requests and exports explicit diagnostics/bundles. Settings persist
isolated Preview Build preferences, dynamically discover personalities and present
unsupported operations as unavailable. main owns STA COM, instance guard, mode
selection and clean restart handshake. No privilege elevation or startup
registration is introduced.

A 100 ms timer advances Preview logical time and drains posted control commands;
it is neither an audio clock nor a performance measurement. refresh reads the
backend's authoritative snapshot and gates personality messages by event sequence.
Preview is deterministic, simulated and session-only. Test performs real isolated
profile/device file I/O and real Windows scanning through the owned ScannerService worker.
Test routing, monitoring, beep and live profile Apply stay disabled; Simulate Errors is absent. Diagnostics
exports identify the actual operating mode and unavailable hardware measurements.

Commands/results, immutable state, severity/events, unavailable telemetry and
bounded pull subscriptions are documented in
[INTERNAL_CONTROL_CONTRACT](../../docs/specifications/INTERNAL_CONTROL_CONTRACT.md). The future engine
must marshal completions without GUI/filesystem calls on audio threads. No real
stream supervision, measured timing or production serializer is implemented.
Avatar playback now uses PR6's shared Session/Panel through AvatarHost. [Persistence](../../docs/specifications/PROVISIONAL_TEST_PERSISTENCE.md) explains provisional
Test JSON and its deliberate differences from unresolved production requirements.

Application appearance uses palette.h, SurfacePanel, custom button painting,
read-only Windows theme/high-contrast queries and optional title-bar color. Native
controls and handlers are retained; system dialogs/menus follow Windows styling.
High contrast takes precedence. Disabled buttons desaturate. Header geometry is
bounded independently of copy length, text scrolls within the padded rounded card,
and the avatar follows that card. The routing detail pane scrolls; the left Profiles
column has draggable width/row dividers, independent list/notes scrolling and an
always-visible Save/Apply/Revert bar. Devices also has draggable width and
Notes/Overview dividers. These sizes are session-only. Context menus target stable profile/hole IDs, rename stays
staged, and duplicates receive fresh IDs. Glyph add buttons have explicit MSAA
names as well as tooltips. Initial sizing prefers a work-area-bounded square.
The dark status bar retains a readable foreground. No Windows appearance setting
is changed; only the app's own preference is saved.

Sleep text is a rendering overlay over retained awake copy. SleepCommentary uses
an injected clock/random selector to schedule occasional owned dream text; it
never calls AvatarHost::react, controller commands, or notifications. Waking and
warnings remove the overlay without reselecting the awake response.

[Personality packages](../../personalities/README.md) are data-only, discovered at
startup/explicit Reload, with no parsing during refresh/paint. Technical facts are
compiled and independent. Field headings use the character name (Horu) or Status for Vanilla. Identity switches clear previous art/flavor; missing
owned optional images hide rather than borrow another character. Bother uses the
existing weighted/no-repeat selector, a 350 ms cooldown and warning guard. DFA remains
presentation-only, with exact decimal range checking before half-up rounding,
0.01 stepping and owner-authored thresholds/eggs. Its value/history stays transient.
DFA feedback appears beside its controls when an operational warning must remain.
Vanilla and missing-feature packages do not expose DFA/Bother.

The internal Preview and Test smoke modes dispatch real widget events and record
explicit results. Test mode uses a private temporary data root and reloads saved
configuration through a new controller. Unattended assertions/errors terminate
with a failure instead of hanging on dialogs. BMP screenshots are review evidence,
not automated visual assertions. Native Windows execution/DPI/restart/tray and
extracted-package checks remain required; cross-compilation cannot establish them.
See [implementation report](../../docs/specifications/GUI_INTEGRATION_PREPARATION.md) and
[manual checklist](../../docs/specifications/MANUAL_GUI_CHECKLIST.md).


Bother is an avatar-click easter egg only: no visible button, no redundant
“Personality interaction” technical line. Empty message rows collapse. Horu's owned
supplied preview PNG is shipped unchanged. The small buffer Apply saves only one
direction through Controller::update_buffer, keeping other edits staged. Matching
active Preview paths expose simulated requested/confirmed values; Test has only
saved configuration, with engine values unavailable. Unchanged/invalid/unsaved
new directions keep this button disabled. This does not activate a whole profile.


The operative avatar integration is documented in
[AVATAR_INTEGRATION.md](../../docs/specifications/AVATAR_INTEGRATION.md). The avatar is no longer the
static AgentDisplay; that class remains for branding. The independent animation
timer does not advance controller time and stops when hidden/minimized. All owned
assets/manifests are deployed inside the selected personality package.

The two Devices lists, hidden-device behavior, evidence projections, scan scopes,
limits, consent, storage and cancellation contracts are documented in
[SCANNER_TEST_MODE.md](../../docs/specifications/SCANNER_TEST_MODE.md).

`DeviceList` uses accessible colored-dot cells and ellipsized device names; full
names/status remain in hover help and Overview. `SecondsField` uses exact decimal
conversion outside the GUI to retain millisecond precision without float rounding.
`Frame::update_console` follows only an already-bottom viewport, anchoring an
earlier visible line across append or bounded-log replacement. On Windows it
counts LF lines instead of confusing wx string indices with native CRLF positions.

`Frame::shell_menu` is shared by the tray and taskbar Controls task. The flat
Windows Jump List also offers Close/Exit and available routing tasks directly.
`shell_actions` owns Windows COM Jump List construction and a bounded fixed-action
relay to the existing GUI; it never creates a backend in the relay process.
The native window menu exposes Controls/Exit too. See TRAY.md and the manual
checklist for shell behavior, lifecycle and validation limits.
