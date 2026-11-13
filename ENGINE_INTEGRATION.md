# Engine 0.6 integration

Normal operation now uses the supplied engine behind ConfigurationController.
Tutorial remains a separate in-memory simulator and never constructs the real
engine, scanner, persistence or device-check providers. The original user data
root and profile schema 3 / transfer schema 7 are unchanged.

The source handoff archive SHA-256 is
`516b3c5a68b597e1a782f038f72f8926648436dfb136a45e392a693fee420b3e`.
Its hashes were checked before import. Streaming algorithms and adapter behavior
are unchanged; integration fixes Windows SDK include dependencies, adjusts CMake/
test-fixture paths, disables the standalone host, and assigns Runtime package
components. Current handoff reference documents
are retained under `engine/handoff/`; older build-time notes remain in `engine/`.

## Ownership and lifecycle

The GUI issues commands to ApplicationController/ConfigurationController.
RoutingService owns one dedicated control thread, which constructs, calls and
destroys engine::Runtime. The engine owns its lifecycle and native stream workers.
No widget, paint handler or audio callback scans devices, parses profiles,
opens streams, writes JSON or joins a native audio worker.

Start prepares a transient typed projection of the saved profile and copies an
immutable scanner binding snapshot. Logical IDs, direction, observed availability
and opaque local endpoint IDs are supplied exactly. Missing, unavailable or
ambiguous bindings are never replaced by names or Windows default devices.
The existing persistence owner retains original JSON, unknown fields, filenames
and notes; engine projections and negotiated results are never saved back.

Queued admission, Runtime admission and confirmed frame progress are separate.
GUI state stays Starting until the engine reports actual progress. Each path
retains its own state, negotiated source/playback configurations, access receipts,
issues and counters. Failed admission retains structured code/location/native-code
issues in the public snapshot and diagnostics view too. A healthy direction can
continue while its sibling fails and aggregate state is Degraded.

Stop immediately invalidates older queued publications and disarms host intent.
Stopping persists until all native sessions are released. Another Start is
rejected while admission, routing or cleanup is active. Exit and restart keep
the GUI event loop responsive while waiting for Stopped, then destroy ownership.
Driver APIs remain cooperatively cancellable without a hard cleanup deadline;
there are no detached workers or forced thread termination.

## Policy and interface

- Host native scheduling preset is explicitly **Low**, matching the handoff's
  latest working hardware reference. It is not a profile field or a latency
  measurement. Miniaudio keeps its own provided scheduling policy.
- Hole `routing` retains automatic/wasapi/miniaudio/experimental. Automatic
  selects an available native adapter first, then an available Miniaudio adapter;
  it does not retry a failed native route through another backend.
- Direction flags, endpoint/common inheritance, channel mapping and strict
  overrides follow the supplied contract. Only unforced Exclusive preference
  permits the engine's documented pre-transfer Shared fallback. Its refused
  Exclusive receipt remains visible without presenting successful fallback as
  a fatal issue. Preferred clock metadata remains unapplied by this module.
- Endpoint cards show actual opened formats/modes when the draft assignments
  match the immutable Start-time intent. Edited assignments never relabel an
  older running route. The pinned, scrollable performance panel shows actual
  formats, allocated buffers, scheduling periods, access attempts, progress and
  optional counters. It preserves its scroll position as telemetry updates.
- Frame counts, progress age, correction ppm, FIFO occupancy and allocated
  buffers are diagnostics, not calibrated latency, physical drift, acoustic
  output proof or CPU measurements. Unknown observations stay absent.
- Real routing and temporary checks/scans cannot be started together. Metadata
  Save retains edits for the next Start; there is no implicit live reconfiguration.
  Read-only device availability refresh can update the inventory without
  replacing the engine's copied bindings.

The handoff does **not** implement automatic endpoint reconnection, transactional
live profile switching, live tuning changes or destination mixing. Stop and
Start are explicit. Saved automatic-wait/switch preferences remain preserved,
but those controls are disabled for this engine with explanatory help. Selecting
a different profile while routing changes only the viewed editor. Startup
activation supports a saved profile with Immediate/Delayed/Manual policy; Stop
cancels its pending deadline.

## Tutorial and UI refinements

The owner script is now version 2.5: explicit confirmed profile/hole rename steps,
six-second avatar-click overlays, a ten-second simulated activation without a
progress bar, cleaned final dialogue and End tutorial after completion. New
choices receive the selected appearance palette. Tutorial reactions now use a
valid player priority; intentionally static deadpan skips an ungenerated animation
without suppressing failures of approved assets.

Operation-mode avatar canvas height matches the personality frame; the frame
and action row expand into the released right-hand space. Tutorial sizing is
independent. Settings uses Application and Tray and Personality and Animation,
with Restart AudioHole inside Application and Tray. Restart uses a restricted
inherited process handle, avoiding PID reuse and filesystem path-identity checks.

## Validation boundary

Portable tests exercise the imported contract/processing/access/startup suites
and the actual service/Runtime with explicitly injected software-only adapters:
admission, independent failures, forced refusal, exact bindings, immutable saved
intent, cancellation, stale updates, asynchronous cleanup and startup failures.
Tutorial and avatar tests cover both rename gates and real player reactions.

Gui.EngineSmoke exercises actual Start/Stop controls with that same Runtime and
software adapters, never hardware. Existing fixture, configuration-only and
tutorial smoke modes retain their substantive checks. Gui.RestartSmoke launches
and waits for a real child process through the production restart handshake.
The package check runs all four direct GUI modes outside the checkout.

Cross-compilation and Wine execution do not establish native Windows/MSVC or
physical audio behavior. Native Windows CI and manual hardware testing remain
required. Repeat the handoff's known working profile in Low, then compare
Baseline separately before making latency or recovery claims.
