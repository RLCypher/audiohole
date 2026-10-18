# Native view implementation

`Frame` owns one `Controller` and one `Tray`. wxWidgets owns the child windows,
sizers, and event bindings; widget pointers in `frame.h` are borrowed handles and
must not outlive the frame. The tray's frame reference is valid until its owner
removes/destroys the tray. All GUI/controller/catalog calls run on the GUI thread.
There are no audio callbacks, worker threads, locks, or sample buffers here.

View builders live in separate source files. `profile_view.cpp` keeps the editable
draft separate from saved controller profiles and the live plan. `device_view.cpp`
never enumerates Windows endpoints. `diagnostics_view.cpp` exports only explicit
user-requested technical text. `settings_view.cpp` keeps presentation preferences
local and exposes unresolved production behavior as unavailable. `tray.cpp` issues
the same controller commands as the window. `main.cpp` owns STA COM initialization
and the single-instance guard, with no startup registration or privilege elevation.

A 100 ms GUI timer advances fixture time; it is not an audio clock or performance
measurement. `refresh()` reads status and gates commentary by event sequence.
Mode selection and DFA touch presentation only. Temporary observation and beep
state are cancelled on relevant navigation/lifecycle operations.

The controller interface is an internal replaceable seam, not an accepted public
API schema. A real backend must marshal status/commands here without allowing
widget or filesystem calls onto audio threads. Real stream supervision, transactional
preparation, measured timing, and persistence belong to later authorized work.

The internal smoke mode dispatches real widget events and checks controller results.
BMP screenshots are review evidence; desktop capture availability and visual quality
are not asserted by those checks. See the root manual checklist and ENGINEERING.md.

Unattended smoke mode sends wx errors/assertions to stderr and terminates on an
assertion, preventing a modal debug dialog from hanging CI. Normal interactive
sessions retain the framework diagnostics. This does not suppress failed checks.
