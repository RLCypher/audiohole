# Engine 0.6 integration

The [2026-11-15 engine audit](docs/AUDIO_ENGINE_AUDIT_2026-11-15.md) amends
Automatic mono mapping, scanner-assisted native format acquisition and Exclusive
diagnostics. It changes no GUI/animation implementation or saved profile schema.

Normal operation now uses the supplied engine behind ConfigurationController.
Tutorial remains a separate in-memory simulator and never constructs the real
engine, scanner, persistence or device-check providers. The original user data
root is unchanged. The current timing amendment uses profile schema 4 / transfer schema 8; see [Profile v4 timing controls](docs/LATENCY_V4.md). Older records remain readable.

The source handoff archive SHA-256 is
`516b3c5a68b597e1a782f038f72f8926648436dfb136a45e392a693fee420b3e`.
Its hashes were checked before import. The original streaming baseline is retained; explicit timing and availability recovery are amended in [Profile v4 timing controls](docs/LATENCY_V4.md).
current integration amendments below cover binding refresh, buffer requests and
acquisition diagnostics. Integration fixes Windows SDK include dependencies,
adjusts CMake/test-fixture paths, disables the standalone host, and assigns Runtime package
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
  format/access overrides follow the supplied contract. Buffer values are API
  requests with driver-selected allocation, as described in the audit below.
  Only unforced Exclusive preference
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
  Read-only device availability refresh updates inventory. Ready paths can receive
  newer exact bindings through the waiting policy below; running paths retain
  their copied bindings.

The integration now refreshes exact bindings for Ready paths after inventory
changes. It opens waiting paths once all required bindings are available, without
reopening healthy siblings or reloading saved edits. Stop disarms waiting.
Failed/running streams are not silently reconfigured or reconnected. Transactional
live profile switching and destination mixing remain unsupported;
the live-switch preference stays disabled. The wait-for-devices preference is active. Selecting
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

Operation-mode avatar canvas uses the compact header height; the frame and
action row expand into the available right-hand space. Tutorial sizing is
independent. Settings uses Application and Tray and Personality and Animation,
with Restart AudioHole inside Application and Tray. Restart uses a restricted
inherited process handle, avoiding PID reuse and filesystem path-identity checks.

## Explicit profile Apply

All Profile-tab Apply buttons save the full draft before applying routing. For
the active profile, the controller requests Stop, waits asynchronously for
confirmed native cleanup, and starts the exact accepted settings. Save alone
continues to leave the live plan unchanged. Explicit Stop cancels a queued Apply
restart. This can briefly interrupt audio and does not provide seamless
transactional profile switching. Applying only names/notes to a healthy route
updates metadata without reopening its streams. Failed writes leave the working route intact;
failed backend admission after restart retains saved edits and reports failure.
See the [persistence audit](docs/PROFILE_PERSISTENCE_AUDIT_2026-11-15.md) for fields
and validation evidence.

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

## Owner-requested integration audit

The audit traced GUI capture/save, canonical preference projection, engine parsing,
inheritance/validation, binding, access negotiation, WASAPI/Experimental and
Miniaudio initialization, startup progress, receipts, cancellation and cleanup.
Profile record v3 and transfer v7 remain unchanged; edits never rewrite a live plan.

A concrete tuning blocker was found in `check_stream`: successful opens were
rejected when allocation differed from the requested milliseconds by more than
one frame. WASAPI can align allocations to engine/device periods; Miniaudio can
allocate several periods. The GUI now explicitly labels `Buffer request` and
explains alignment. `buffer_ms` still must be supplied unchanged to the native
API and confirmed in the adapter receipt. Actual allocation and its sample-rate
domain must be positive/known, and both requested milliseconds and actual
frames/milliseconds are shown. Exact rate, representation, channel count and
forced access modes remain requirements. This is a buffer-request policy
amendment, not a promise of reduced acoustic latency or an exact allocation.

The screenshot's **0x8889000A** is a separate endpoint-ownership refusal at
InitializeAudioStream. It is now classified as ResourceConflict with its HRESULT
and actionable exclusive-owner guidance; failures also identify Forward/Return,
and diagnostics include the frozen effective requests even if no stream opened.
No external owner is terminated, no forced mode is silently weakened, and no
claim is made that a specific application caused it. A real hardware reproduction
is still needed to identify that owner or a driver transition on the user's PC.
Native device-invalidated opens remain Ready for a later exact binding refresh;
format/exclusive-permission refusals remain explicit unsupported requirements.

Ready updates are marshalled through RoutingService to its owning Runtime thread
and copied to the lifecycle worker. The v4 availability amendment opens complete
holes independently. A missing or changed physical binding releases only its
hole; it may resume once that hole's bindings are all available and unambiguous.
Other holes retain their sessions, and Stop cancels all waiting. A failure without
a binding change still requires Stop/Start. No scans, JSON, allocations, locks or GUI work were added to audio
callbacks. Endpoint releases, joins, COM ownership and backend startup were reviewed:
failed mode attempts release on their owning worker before fallback, Stop cancels
queued binding updates, and confirmed Stopped follows session cleanup.

New regressions exercise initially missing devices, partial return, ambiguity,
Stop cancellation and healthy duplex sibling preservation. The real GUI engine
smoke saves/starts/stops independent advanced buffers at 1/6/20/50 ms using
explicit software adapters. Model tests cover all three backend selectors,
Automatic fields with advanced tuning enabled, strict formats/modes, endpoint
inheritance, rounded buffers and matched acquisition/release counts. These tests
exercise the actual service and Runtime, but software adapters do not establish
physical WASAPI/Miniaudio compatibility or wireless latency. Native Windows CI
and hardware playtesting remain necessary.

Verification for this increment: the Linux Debug suite passed 251/251 tests.
The MinGW Release build passed; supplemental Wine runs passed all five GUI
smoke modes at 96 and 144 DPI, 21 targeted Windows model regressions, 30 avatar
checks and byte comparisons for all 699 approved decoded PNGs. Dark and Light
disabled Settings and selector/resize screenshots were reviewed. These results
do not replace native Windows CI or the hardware validation above.
