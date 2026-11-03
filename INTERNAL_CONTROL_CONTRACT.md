# Internal application control contract v1

Implemented C++ in-process seam in `model/application_controller.h`, decorating
one existing `Controller`. This is not API.md's final external JSON protocol or
an engine integration. Frame and Tray issue their existing calls through this
same decorator. There is no second routing state store or audio/scanner rewrite.

## Commands and results

`submit(Command)` is synchronous **control-thread** admission. Stable identifiers:

| Typed command | ID | Current behavior |
|---|---|---|
| StartRouting | routing.start | Preview accepts simulated activation; Test rejects |
| StopRouting | routing.stop | Preview accepts until authoritative Stopped; Test rejects |
| LoadProfile | profile.load | Completes with a copied saved profile; does not activate |
| SaveConfiguration | configuration.save | Completes after save (real disk in Test); Apply is asynchronous Preview only |
| DiscoverDevices | devices.discover | Preview simulated Easy/Advanced scan; Test rejects |
| UpdateBuffer | buffer.update | Commits one saved directional buffer preference; matching active Preview direction updates with simulated confirmation |
| RequestStatus | status.get | Completes with authoritative state available |
| RequestDevices | devices.get | Same snapshot's records; no discovery side effect |
| RequestDiagnostics | diagnostics.get | Same snapshot's retained events; no invented measurements |
| StartBeep | beep.start | Preview silent bounded simulation, overlap rejected; Test rejects |
| CancelBeep | beep.cancel | Cancels Preview active beep and correlated pending request |

Existing configuration/create/remove/device-update/startup/clear methods are also
recorded as named results. Simulation/recovery/observation publish authoritative
state/events. No deferred per-hole quick toggles or executable external plugins
are added. A future real probe command requires consent and backend semantics;
Advanced Preview is a simulation, not proof that hardware probing is integrated.

Every result carries a monotonic request ID, command ID, outcome, detail, snapshot
revision and simulated flag. Outcomes are Accepted, Completed, Rejected, Failed,
Cancelled, TimedOut. Admission is not operation success or audibility. Pending
activation/stop/scan/beep requests are settled from backend snapshots; Ready is a
terminal armed/waiting configuration, not flowing audio. Stop cancels pending
requests; replacement activation supersedes old activation. Requests have a bounded
30-second logical wait. Timeout reports uncertainty and does **not** silently
cancel or undo an operation; use explicit Stop/Cancel and inspect state.

Legacy `Result.ok` callers still receive command admission or synchronous save
success. Future adapters use the richer correlated stream for asynchronous
completion. Backend events remain operational events, separate from command
results; they have sequence, type, detail and severity. Failed commands that do
not mutate backend state can still produce rejected results. No pending command
is serialized as authority to resume after restart.

## Consumers, bounds and threads

`subscribe(filter)` returns a move-only RAII Subscription. Up to16 active
consumers, each receiving an immutable initial snapshot. `poll()` transfers its
own batch: latest state, sequenced events, correlated results, optional telemetry,
gap and closed indicators. Immutable shared snapshots survive later mutations.
No callbacks hold dead widget pointers or create reentrancy. Reset/destruction
unsubscribes; controller destruction marks surviving subscriptions closed.
Hosts must join any producers before destroying the controller.

Events/results retain at most128 per consumer. Overflow drops oldest records and
marks gap; resynchronize from latest snapshot. No durable replay or exactly-once
external delivery is promised. Each latest snapshot also contains the backend's
bounded retained diagnostic history. State/telemetry coalesce by revision rather
than building a high-frequency queue. Filtering can omit events/results/telemetry.
No more than64 queued posts or pending commands. Subscribe at the limit returns
an empty subscription; its poll has no state. Dropped result history does not
change backend state; consumers must track outstanding requests and resynchronize.

All controller calls, subscribe/poll and GUI consumption belong to the owning
control thread. Wrong-thread use throws a logic error. `post(Command)` alone
supports a non-real-time producer: try-lock admission to a bounded queue returns
an ID/queued receipt, or false/0 if busy/full. It does not execute backend work.
The owner's `advance()` drains it before advancing backend time. Queued receipt
is not the Accepted command outcome. There are no internal worker threads,
network endpoints, named pipes, sockets or automatic reconnect loops.

This seam allocates/copies and configuration saves do filesystem I/O on the
control thread. It is explicitly **prohibited on audio callbacks**. Future engine
workers require their own bounded nonblocking transport and completion generations
before posting control notifications; they must never wait for GUI subscribers.
The current no-audio backend cannot demonstrate real-time behavior.

Telemetry has explicit Available/Unknown/Unavailable/Unsupported/Stale states and
optional values. Latency, xruns and applied buffer remain Unavailable with no
numeric default. Scan progress is explicitly simulated in Preview. No invented
hardware counters, latency, applied buffers or clock confidence are published.
A future backend must supply those observations, timestamps and stale handling.
No hardware backend reconnection protocol has been implemented.

## Future personality/avatar adapter (not linked)

`presentation_baseline(snapshot)` maps healthy Running/Stopped/Ready to idle,
Starting/Stopping to working, scanning to thinking, degraded/recovering to concern
and failed to error, with higher warning priorities. Combine these authoritative
conditions with explicit validation errors where the host owns them. The GUI's
Bother cooldown and warning guard prevent low-priority flavor masking errors.

Conceptual host flow, following the inspected PR4 API:

```cpp
// Host thread; engine is NOT instantiated in this milestone.
const auto* pack = messages.package(selected_id);
// Use pack->avatar_registry.parent_path() as Repository/Session root only
// after avatar integration validates registry/manifests/assets with its loader.
session.select(pack->id, "", now_ms); // selected pack's default style
const auto base = presentation_baseline(*batch.state);
session.baseline(base.expression, base.revision, base.priority, now_ms, true);
const auto copy = messages.show(event, selected_id, occurrence_id);
// Reaction's event_id is an OCCURRENCE, distinct from authored copy.response_id.
// Admit only with valid owned expression, current revision and bounded TTL.
session.react({copy.expression, occurrence_id, base.revision, now_ms, 1500, 10}, now_ms);
// Queued acceptance is not playback completion; inspect decisions after tick().
```

Use real Session types/signatures in PR4 and do not blindly invalidate every tick:
`invalidate_reaction=true` is for meaningful operational changes. Continue ticking
the shared host, return to latest baseline, clear previous identity on switch,
respect severity/expiry and shut down/join loader on exit. Session already owns
animation/static/owned-idle/last-valid/hide fallbacks. Never borrow another
personality. No duplicate playback implementation belongs in Messages.

GUI, Tray, future desktop pet, OBS, Stream Deck and CLI adapters can consume this
same state/result stream. Only GUI/Tray are wired now. Transport authentication,
reconnection, runtime telemetry and actual audio/animation integrations are future
work, not implied by these interfaces.


## Buffer preference Apply follow-up

`Controller::update_buffer(profile_id, hole_id, reverse, milliseconds)` validates
1..100ms and a saved direction, then updates only that field through the existing
save interface. Other GUI draft edits remain staged. `UpdateBuffer` carries an
optional explicit profile_id; omission targets only the active profile, never an
arbitrary selection. ConfigurationController commits real Test data with atomic
replacement/conflict checks and reports no engine application. SimulatedController
also updates a matching active copied plan without restarting unrelated directions.
PathStatus exposes optional requested_buffer_ms/applied_buffer_ms; Preview confirms
only Running directions and labels every confirmation simulated. Unavailable or
waiting directions never receive a fabricated confirmation. These are parameter
confirmations, not measured latency, and are never persisted as runtime authority.
The global telemetry metric remains unavailable because independent directions can
have different buffers. No real audio engine buffer update has been implemented.
