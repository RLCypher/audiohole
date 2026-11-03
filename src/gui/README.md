# Native view implementation

Frame owns one ApplicationController decorator around the selected backend and one
Tray. wxWidgets owns child windows, sizers and event bindings; widget pointers are
borrowed and must not outlive Frame. Tray uses the same controller. GUI, catalog,
configuration and controller calls belong to the control/GUI thread. This is not
an audio-callback interface. The optional bounded post queue is for non-real-time
adapters, and introduces no worker or external transport.

View builders remain separate: profile_view owns editor drafts independently of
saved records/live plans; device_view never enumerates Windows endpoints;
diagnostics_view copies/exports only explicit user-requested text. Settings persist
isolated Preview Build preferences, dynamically discover personalities and present
unsupported operations as unavailable. main owns STA COM, instance guard, mode
selection and clean restart handshake. No privilege elevation or startup
registration is introduced.

A100ms timer advances Preview logical time and drains posted control commands;
it is neither an audio clock nor a performance measurement. refresh reads the
backend's authoritative snapshot and gates personality messages by event sequence.
Preview is deterministic, simulated and session-only. Test performs real isolated
profile/device file I/O with no audio/scanner backend. Test routing, scans,
monitoring, beep and Apply stay disabled; Simulate Errors is absent. Diagnostics
exports identify the actual operating mode and unavailable hardware measurements.

Commands/results, immutable state, severity/events, unavailable telemetry and
bounded pull subscriptions are documented in
[INTERNAL_CONTROL_CONTRACT](../../INTERNAL_CONTROL_CONTRACT.md). The future engine
must marshal completions without GUI/filesystem calls on audio threads. No real
stream supervision, measured timing, production serializer or avatar playback is
implemented. [Persistence](../../PROVISIONAL_TEST_PERSISTENCE.md) explains provisional
Test JSON and its deliberate differences from unresolved production requirements.

Application appearance uses palette.h, SurfacePanel, custom button painting,
read-only Windows theme/high-contrast queries and optional title-bar color. Native
controls and handlers are retained; system dialogs/menus follow Windows styling.
High contrast takes precedence. Disabled buttons desaturate. Header geometry is
bounded independently of copy length, text scrolls within the padded rounded card,
and the avatar follows that card. Both profile columns scroll when constrained.
The dark status bar retains a readable foreground. No Windows appearance setting
is changed; only the app's own preference is saved.

[Personality packages](../../personalities/README.md) are data-only, discovered at
startup/explicit Reload, with no parsing during refresh/paint. Technical facts are
compiled and independent. Identity switches clear previous art/flavor; missing
owned optional images hide rather than borrow another character. Bother uses the
existing weighted/no-repeat selector, a350ms cooldown and warning guard. DFA remains
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
See [implementation report](../../GUI_INTEGRATION_PREPARATION.md) and
[manual checklist](../../MANUAL_GUI_CHECKLIST.md).
