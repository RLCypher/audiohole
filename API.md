# AudioHole v0.1 — External Control API

**Document:** `API.md`  
**Status:** Exploratory design — transport and implementation not finalized  
**Related documents:** `SPEC.md`, `ARCHITECTURE.md`, `PROFILES.md`, `CONFIG_FORMAT.md`, `GUI.md`, `ROADMAP.md`

---

## 1. Purpose

AudioHole should be designed to support optional control and monitoring from external applications.

Potential integrations include:

- Elgato Stream Deck.
- OBS Studio.
- Desktop automation utilities.
- Macro pads.
- Custom scripts.
- Other local applications.

The primary objective is simple:

**An external application should be able to start, stop, select, and monitor AudioHole routing without requiring interaction with its main GUI.**

External control is entirely optional.

AudioHole must operate normally without any API client, external controller, network connection, or third-party plugin.

The initial release need not include Stream Deck or OBS plugins.

However, the architecture should expose a clean internal control boundary early so external integration can be added without redesigning the routing engine.

---

## 2. Scope

### 2.1 Core Operations

The initial API should be intentionally small.

Potential operations:

- Query application status.
- List available profiles.
- Query active and loaded profiles.
- Query hole status.
- Activate a profile.
- Stop routing.
- Subscribe to status changes.

These operations should use the same internal application controller as the GUI.

### 2.2 Optional Future Operations

Potential later additions include:

- Create or edit profiles.
- Change device assignments.
- Modify audio preferences.
- Request device rescanning.
- Trigger failover manually.
- Export diagnostic information.
- Control individual directional paths.
- Enable or disable an individual hole (deferred beyond v0.1).

These should not be prerequisites for the initial API.

### 2.3 Explicitly Out of Scope

The API is not intended to provide:

- Direct access to real-time audio buffers.
- A general-purpose audio-processing plugin interface.
- Remote administration over the internet.
- Cloud synchronization.
- Unrestricted filesystem access.
- Arbitrary command execution.
- Driver management.

The API controls AudioHole's existing functionality rather than introducing a separate routing system.

---

## 3. Architecture

The API should be a thin adapter over the existing application controller and routing supervisor.

```text
Stream Deck / OBS / Script
           |
           v
    Local Control API
           |
           v
   Application Controller
           |
           v
    Routing Supervisor
           |
           v
       Audio Engine
```

Status information travels in the opposite direction.

The API must not communicate directly with real-time audio threads.

### 3.1 Single Source of Truth

The GUI, tray menu, and external API must all use the same application state and command handling.

There must not be separate implementations of profile activation or routing recovery.

### 3.2 Command Serialization

State-changing commands should be processed through a coordinated control queue or equivalent mechanism.

This prevents conflicting requests from the GUI, Stream Deck, and OBS from racing to modify the same routing session.

### 3.3 Asynchronous Operations

Starting or switching profiles may take time.

The API must not block indefinitely while waiting for hardware.

Commands should return an acknowledgment or operation identifier, while status events report progress and completion.

### 3.4 Real-Time Isolation

API requests, JSON serialization, logging, and client communication must not block real-time audio processing.

---

## 4. Candidate Transport Mechanisms

Several local communication mechanisms are suitable for AudioHole.

### 4.1 Windows Named Pipes

Windows named pipes provide local interprocess communication without requiring a network server.

Advantages:

- Native Windows support.
- Low overhead.
- No TCP listening port.
- Local access control through Windows security descriptors.
- Suitable for bidirectional command and event communication.
- Works with ordinary user-level applications.

Disadvantages:

- Windows-specific.
- Some scripting environments require additional code.
- Web-based integrations cannot connect directly.
- Third-party plugin developers may need a small client library.

Named pipes are a strong candidate for the internal local API.

### 4.2 Localhost HTTP

AudioHole could expose a small HTTP server bound exclusively to the loopback interface.

Example:

```text
http://127.0.0.1:17842/
```

The port is illustrative and not reserved.

Advantages:

- Familiar to developers.
- Easy to call from scripts and plugins.
- Compatible with many automation tools.
- Straightforward request/response semantics.
- Human-readable JSON messages.

Disadvantages:

- Requires a local HTTP listener.
- Needs careful authentication and request validation.
- Port conflicts must be handled.
- Loopback binding alone does not prevent other local processes or browser-origin requests from attempting access.
- Event subscriptions require polling or an additional mechanism.

HTTP is attractive for broad compatibility, but its security and lifecycle requirements are greater than those of a narrowly scoped named pipe.

### 4.3 WebSocket

A localhost WebSocket connection could provide bidirectional commands and live status events.

Advantages:

- Persistent connection.
- Immediate status updates.
- Familiar to OBS and web-oriented developers.
- No repeated polling required.
- JSON messages are straightforward.

Disadvantages:

- Requires connection management.
- Requires authentication and origin validation.
- More protocol complexity than simple HTTP.
- Clients must handle disconnections and resubscription.

WebSocket may be useful if live integration becomes a major requirement.

### 4.4 HTTP with Server-Sent Events

HTTP could handle commands while Server-Sent Events deliver status changes.

Advantages:

- Simple command endpoints.
- Efficient server-to-client event delivery.
- Easier to implement than a full bidirectional WebSocket protocol in some environments.

Disadvantages:

- Two communication patterns.
- Reconnection and event replay must be handled.
- Not every integration environment has convenient SSE support.

This is a reasonable alternative to WebSocket.

### 4.5 Local Command-Line Utility

A small companion command-line client could communicate with AudioHole through named pipes.

Example:

```text
audiohole-cli status
audiohole-cli profile activate "vr-audio"
audiohole-cli routing stop
```

Advantages:

- Easy to invoke from scripts and automation utilities.
- No public HTTP listener required.
- Reuses the same local IPC implementation.
- Useful for diagnostics and testing.

Disadvantages:

- Requires a separate executable or command-line entry point.
- One-shot commands do not naturally provide persistent event subscriptions.

A CLI client could be particularly useful even without dedicated Stream Deck or OBS plugins.

A standalone CLI is not required for the core v0.1 release.

### 4.6 File-Based Commands

A controller could write JSON requests into a watched directory.

Advantages:

- Human-readable.
- Easy to inspect.
- Minimal integration requirements.

Disadvantages:

- File watching and synchronization complications.
- Ambiguous command acknowledgment.
- Stale requests after crashes.
- Race conditions.
- Poor suitability for immediate control.
- Filesystem permissions and cleanup concerns.

**Not recommended as the primary control mechanism.**

Configuration files should remain configuration files, not become an improvised command queue.

### 4.7 Global Hotkeys

Global hotkeys could trigger predefined actions.

Advantages:

- Very low integration complexity.
- Useful with programmable macro pads.
- No plugin required.

Disadvantages:

- Limited command vocabulary.
- Potential key conflicts.
- No structured status feedback.
- Poor support for multiple profiles and holes.

Hotkeys may be a convenient secondary interface, but they are not a replacement for an API.

---

## 5. Preliminary Comparison

| Mechanism | Runtime overhead | Integration accessibility | Live status | Assessment |
|---|---|---|---|---|
| Named pipes | Very low | Moderate | Yes | Strong native candidate |
| Localhost HTTP | Low | High | Requires extension | Strong compatibility candidate |
| Localhost WebSocket | Low | High | Yes | Good for interactive plugins |
| HTTP + SSE | Low | High | Yes | Good alternative |
| CLI over named pipes | Very low per request | High for scripts | Limited | Useful companion |
| Watched JSON files | Low but operationally awkward | Moderate | Poor | Not recommended |
| Global hotkeys | Very low | Moderate | No | Optional convenience |

The performance difference between these mechanisms is unlikely to matter for ordinary routing commands.

Reliability, security, implementation complexity, and ease of third-party integration should drive the decision.

---

## 6. Recommended Initial Direction

The preliminary recommendation is:

**Build one internal command-and-status interface, with Windows named pipes as the preferred first transport.**

This provides:

- A single local control path.
- No mandatory network listener.
- Minimal additional dependencies.
- Native Windows access controls.
- Bidirectional communication.
- A clean foundation for a future CLI.

A localhost HTTP or WebSocket adapter can be added later without changing the underlying command semantics.

### 6.1 Avoid Duplicate Control Logic

Each transport must invoke the same commands.

The application should not maintain separate routing behavior for named-pipe and HTTP clients.

### 6.2 Do Not Require API Startup

The API listener should be optional.

If disabled, AudioHole must not create unnecessary listeners or background services.

### 6.3 Implementation Timing

The internal command and status interfaces should be established early.

The external transport may be implemented during a later milestone.

Dedicated OBS and Stream Deck plugins may follow after the transport and command schemas stabilize.

---

## 7. Command Model

Commands should be small, explicit, and independently understandable.

### 7.1 Status Query

```json
{
  "request_id": "req-001",
  "command": "status.get"
}
```

### 7.2 Profile List

```json
{
  "request_id": "req-002",
  "command": "profiles.list"
}
```

### 7.3 Profile Activation

```json
{
  "request_id": "req-003",
  "command": "profile.activate",
  "profile_id": "vr-audio"
}
```

### 7.4 Stop Routing

```json
{
  "request_id": "req-004",
  "command": "routing.stop"
}
```

### 7.5 Enable Hole — Deferred

`hole.enable` is reserved for future consideration, not part of the v0.1 external-control contract. It must not be advertised as implemented.

### 7.6 Disable Hole — Deferred

`hole.disable` is reserved for future consideration, not part of the v0.1 external-control contract. It must not be advertised as implemented.

The preceding command examples are exploratory. Final command names and schemas require validation and versioning.

---

## 8. Command Semantics

### 8.1 Activation

`profile.activate` should request activation of a saved profile.

If the profile is already active, the command should normally be idempotent.

### 8.2 Ready State

An activation command may successfully arm a profile even if its devices are not yet available.

The resulting state may be Ready rather than Running.

**Command acceptance must not be confused with successful audio-stream establishment.**

### 8.3 Switching

An activation request targeting another profile must follow the transactional switching rules in `PROFILES.md`.

If the destination is unavailable, the previous working profile should remain active by default.

### 8.4 Explicit Ready Activation

The API may eventually support an explicit option to arm an unavailable destination profile.

Such behavior must be requested deliberately.

### 8.5 Stop

`routing.stop` must disarm the active profile and cancel automatic recovery.

It must not merely mute audio while leaving the supervisor attempting to restart streams.

### 8.6 Hole Control — Deferred

v0.1 supports profile-level routing commands and hole-status inspection, not individual hole enable/disable commands. Future support must first define temporary versus persistent changes, staged GUI edits, capacity validation, recovery semantics, and consistency across controller, GUI, tray, and optional external adapters.

### 8.7 Duplicate Commands

Repeated identical requests should not cause unnecessary stream restarts.

---

### 8.8 Pending Activation Status Contract

The mandatory **internal** controller status interface exposes `routing_state` and `startup` fields: `activation_mode`, `pending_activation`, `target_profile_id`, `activation_deadline_utc`, and `remaining_seconds`. `routing_state` stays `stopped` until activation begins. When no activation is pending, target and deadline are omitted or explicitly marked not applicable; consumers must use `pending_activation` rather than infer state from missing fields. Remaining time is derived from the controller's monotonic deadline.

Changing the selected profile preserves the deadline and retargets activation. Start Routing Now bypasses the timer; Stop Routing and Cancel Startup cancel it. Obsolete timer callbacks and duplicate activation are prohibited. Optional external transports may expose the same status semantics, but are not required for v0.1.

---

## 9. Responses and Errors

Commands should return structured responses.

Example:

```json
{
  "request_id": "req-003",
  "accepted": true,
  "operation_id": "op-104",
  "status": "pending"
}
```

A later status event reports the result.

### 9.1 Error Example

```json
{
  "request_id": "req-003",
  "accepted": false,
  "error": {
    "code": "profile_not_found",
    "message": "The requested profile does not exist."
  }
}
```

### 9.2 Error Categories

Potential categories include:

- `invalid_request`
- `unsupported_api_version`
- `profile_not_found`
- `hole_not_found`
- `profile_invalid`
- `capacity_exceeded`
- `resource_conflict`
- `switch_failed`
- `permission_denied`
- `operation_cancelled`
- `internal_error`

Errors must be useful to both developers and ordinary users.

---

## 10. Status Model

The API should expose the same profile states used by the GUI and tray.

Supported profile runtime states:

- `stopped`
- `ready`
- `starting`
- `running`
- `degraded`
- `recovering`
- `failed`
- `stopping`

### 10.1 Example Status Response

```json
{
  "api_version": 1,
  "application": {
    "running": true
  },
  "routing": {
    "armed": true,
    "profile_id": "vr-audio",
    "status": "ready"
  },
  "holes": [
    {
      "hole_id": "main-vr-audio",
      "status": "ready"
    }
  ]
}
```

### 10.2 Stable Identifiers

API messages must use stable profile, hole, and device IDs.

Display names may be included for convenience but must not serve as authoritative identifiers.

### 10.3 Desired Versus Actual State

Status responses should distinguish:

- Requested routing.
- Currently established routing.
- Waiting prerequisites.
- Active fallback devices.
- Errors requiring attention.

### 10.4 Status Consistency

The API must not report Running when the GUI reports Failed for the same routing session.

All consumers should observe the same authoritative supervisor state.

---

## 11. Status Events

External clients should be able to subscribe to meaningful state changes.

### 11.1 Example Event

```json
{
  "event": "hole.status_changed",
  "profile_id": "vr-audio",
  "hole_id": "main-vr-audio",
  "previous_status": "ready",
  "status": "running",
  "timestamp": "2026-10-18T04:46:03Z"
}
```

### 11.2 Potential Events

- `application.started`
- `application.stopping`
- `profile.loaded`
- `profile.activation_requested`
- `profile.status_changed`
- `profile.switch_failed`
- `hole.status_changed`
- `device.availability_changed`
- `routing.failover_activated`
- `routing.recovered`
- `routing.error`

### 11.3 Event Delivery

Events are notifications of state changes, not guaranteed permanent records.

Clients must be able to request a fresh status snapshot after connecting or reconnecting.

### 11.4 Event Ordering

Where practical, events should include a monotonically increasing sequence number.

This allows clients to detect missed events or unexpected ordering.

### 11.5 Slow Clients

Slow or disconnected clients must not block the routing supervisor.

Event delivery should use bounded queues and defined overflow behavior.

---

## 12. Stream Deck Integration

A Stream Deck integration could provide buttons such as:

- Start VR Audio.
- Stop AudioHole.
- Toggle PA System.
- Activate Desktop Audio.
- Show current routing status.

### 12.1 Button Feedback

A button could display:

- Stopped.
- Ready.
- Running.
- Degraded.
- Failed.

### 12.2 No Plugin Requirement

A simple Stream Deck action may be able to invoke a command-line client or local HTTP endpoint without a dedicated AudioHole plugin.

The exact capabilities depend on the installed Stream Deck software and available actions.

### 12.3 Dedicated Plugin

A future dedicated plugin could subscribe to status events and update button appearance automatically.

This is optional and deferred.

---

## 13. OBS Integration

An OBS integration could allow scene or streaming workflows to control AudioHole.

Potential uses:

- Activate a streaming profile.
- Stop routing when a stream ends.
- Show whether a required audio hole is healthy.
- Warn when a capture or playback device becomes unavailable.

### 13.1 Manual Actions

An OBS plugin or script could invoke the same commands as any other client.

### 13.2 Automatic Actions

Future integrations could associate profile activation with OBS events.

For example:

- Stream started → Activate streaming profile.
- Stream stopped → Stop routing.

These mappings must be configured explicitly.

### 13.3 Avoid Unexpected Routing

AudioHole must not automatically activate microphone paths merely because OBS is running.

All routing remains subject to explicit user configuration and the existing supervisor safeguards.

### 13.4 Integration Boundary

AudioHole does not need to understand OBS scenes, sources, or internal configuration.

The OBS integration translates OBS events into AudioHole commands.

---

## 14. Security

The API controls live audio routing and therefore requires appropriate local access controls.

### 14.1 Local-Only by Default

External control must be restricted to the local machine by default.

Internet-facing access is not part of the initial scope.

### 14.2 Named-Pipe Security

If named pipes are used:

- Restrict access to the intended interactive user.
- Apply appropriate Windows security descriptors.
- Avoid permissive access to unrelated local accounts.
- Validate all incoming requests.

### 14.3 HTTP Security

If localhost HTTP is used:

- Bind only to loopback interfaces.
- Require appropriate authorization for state-changing requests.
- Validate request origins where applicable.
- Avoid permissive cross-origin access.
- Consider browser-origin request attacks.
- Do not assume loopback binding alone provides authentication.

### 14.4 No Arbitrary Execution

The API must not accept arbitrary shell commands, executable paths, or script payloads.

### 14.5 Least Privilege

The API should run with the same ordinary-user privileges as AudioHole.

It must not require administrator access for normal operation.

### 14.6 Optional Enablement

External API access should be disabled by default unless a later security review establishes a safe, compelling reason to enable it automatically.

---

## 15. Discovery and Lifecycle

External clients need a reliable way to determine whether AudioHole is running and how to connect.

### 15.1 Stable Discovery

The chosen transport should provide a predictable local discovery mechanism.

### 15.2 Application Not Running

If AudioHole is not running, clients should receive a clear connection failure.

### 15.3 Starting AudioHole

Whether the API or a companion CLI may launch AudioHole when it is not running remains an open decision.

Starting the application must not silently bypass its configured routing activation behavior.

### 15.4 Application Exit

When AudioHole exits, API listeners and client connections must close cleanly.

### 15.5 Multiple Instances

The API must not accidentally connect clients to an unrelated AudioHole process or configuration.

The application's single-instance policy and IPC naming should be coordinated.

---

## 16. Versioning and Compatibility

The API must have an explicit version.

### 16.1 Version Field

Requests or connection negotiation should identify the supported API version.

### 16.2 Stable Commands

Once published, commands should retain their meaning across compatible versions.

### 16.3 Unknown Fields

Clients and servers should handle unknown optional fields according to documented compatibility rules.

### 16.4 Unsupported Versions

Unsupported protocol versions should produce clear errors rather than undefined behavior.

### 16.5 Documentation

Published commands, events, fields, and error codes must be documented.

The API must not require reading AudioHole source code to use it.

---

## 17. Configuration

API preferences should be stored in human-readable configuration.

Potential settings:

```json
{
  "external_control": {
    "enabled": false,
    "transport": "named_pipe",
    "allow_state_changes": true
  }
}
```

This is a conceptual example, not a finalized schema.

Security-sensitive credentials or secrets, if later required, should not be casually included in portable configuration exports.

Machine-specific listener settings may belong in machine-local configuration.

---

## 18. Acceptance Tests

### 18.1 Optional Operation

1. Disable external control.
2. Launch AudioHole.
3. Verify normal GUI and tray functionality.
4. Verify routing operates without an API client.
5. Verify no unnecessary external listener is active.

### 18.2 Status Query

1. Enable the API.
2. Connect a local client.
3. Query application status.
4. Verify the response matches the GUI and tray state.

### 18.3 Profile Activation

1. Send a profile activation command.
2. Verify the command is validated.
3. Verify the supervisor receives the request.
4. Verify status transitions are reported.
5. Verify routing follows the same rules as GUI activation.

### 18.4 Ready State

1. Activate a profile requiring unavailable hardware.
2. Verify the profile becomes Ready.
3. Verify the API reports Ready.
4. Connect the device.
5. Verify routing starts.
6. Verify a Running event is delivered.

### 18.5 Stop

1. Activate a profile.
2. Send `routing.stop`.
3. Verify routing stops.
4. Verify the profile is disarmed.
5. Verify device reconnection does not restart routing.

### 18.6 Failed Switch

1. Activate a working profile.
2. Request an unavailable destination profile.
3. Verify the switch fails safely.
4. Verify the original profile remains active.
5. Verify the API reports the failure.

### 18.7 Multiple Clients

1. Connect two external clients.
2. Subscribe both to status events.
3. Change routing through the GUI.
4. Verify both clients receive consistent updates.

### 18.8 Slow Client

1. Connect a client that stops reading events.
2. Generate status changes.
3. Verify routing remains unaffected.
4. Verify event overflow or disconnection is handled predictably.

### 18.9 Invalid Request

1. Submit malformed JSON.
2. Verify the request is rejected.
3. Verify AudioHole remains operational.
4. Verify no routing changes occur.

### 18.10 Unauthorized Access

1. Attempt access from an unauthorized local context.
2. Verify access is denied.
3. Verify no state-changing command executes.

### 18.11 API Restart

1. Connect a client.
2. Restart AudioHole.
3. Verify the old connection closes.
4. Reconnect.
5. Request a fresh status snapshot.
6. Verify the client can recover without stale assumptions.

---

## 19. Open Design Decisions

### 19.1 Primary Transport

Named pipes are the preferred initial candidate.

Localhost HTTP and WebSocket remain alternatives or possible later adapters.

### 19.2 CLI Companion

Determine whether a small CLI client should accompany the first API release.

### 19.3 Hole Toggle Persistence — Deferred

Individual hole enable/disable commands are not required in v0.1. Their persistence semantics remain a future design decision. Hole-status inspection does not imply permission to mutate hole configuration.

### 19.4 Launch-on-Command

Determine whether an external controller can start AudioHole when it is not running.

### 19.5 Event Subscription Protocol

Determine how clients subscribe, reconnect, and obtain current state.

### 19.6 Authentication

Finalize security requirements after choosing the transport.

### 19.7 Release Timing

Determine whether the first external transport belongs in v0.1 or a subsequent release.

The internal command/status boundary should exist from the beginning regardless.

---

## 20. Guiding Principles

1. **External control is optional.**
2. **AudioHole must work completely without external clients.**
3. **The API is a thin interface over existing application functionality.**
4. **The GUI, tray, and API share one authoritative state model.**
5. **Commands must not bypass routing safeguards.**
6. **A command acknowledgment is not proof that audio routing is operational.**
7. **Ready, Running, Degraded, Recovering, and Failed must be externally observable.**
8. **Stable logical IDs are used instead of filenames or Windows endpoint IDs.**
9. **External communication must never block real-time audio threads.**
10. **Local-only operation and least privilege are the defaults.**
11. **Prefer reliability and simplicity over unnecessary protocol complexity.**
12. **Third-party plugins can be deferred without sacrificing future compatibility.**

**AudioHole should be easy to control from a physical button or another application, but never depend on those integrations to do its primary job.**
---

## Temporary Signal Activity Observation

The internal controller may expose read-only, transient source-signal activity observations for GUI troubleshooting, keyed by hole and direction. Activity status is advisory, not a routing state or proof of destination output. An observation must distinguish unavailable/not monitored from no detected signal.

This feature does not add a required external API endpoint, audio-streaming endpoint, or persistent profile field. Optional external transports must not expose raw audio samples.


---

## Setup Test Beep Control

The internal controller shall support a GUI-initiated **Test Beep** command specifying exactly one resolved playback destination endpoint (and hole/direction where relevant), plus a cancellation command and transient status. The operation is explicitly initiated and bounded; it is not a profile setting or an automatic recovery action. Report the exact target and whether the test is idle, playing, unavailable, or failed. An acknowledgment is not proof of audibility.

No external transport or public test-beep API is required for v0.1. If exposed later, it must retain the same safety, target selection, and nonpersistent semantics. The test-beep command is distinct from passive source activity observation.

---

## Reconciled internal control boundary

The internal command/status boundary is required early; public external transport implementation is deferred until core routing is proven. Named pipes remain a candidate, not a selected or enabled transport. Exploratory external JSON examples are not new persistent schemas.

Public routing status follows all eight D-019 states, including Stopping; resolving remains internal. Report armed-plan enabled-hole capacity separately from running paths and hardware performance. Requested and actual endpoints/processing must remain distinguishable; exact new payload fields are not finalized here.

Use generation-based cancellation across asynchronous opens, retries, profile operations, scans, startup, and Test Beep. Stop takes precedence and stale results cannot commit routing. Keep acknowledgment, operation completion, and demonstrated stream operation distinct; no API consumer may infer audibility from acceptance or stream progress.

O-001 leaves fallback aggregation Open, O-002 leaves switch preparation thresholds Open, and O-004 leaves concrete production persistence fields Open. O-005 leaves direct-transfer policy Open: status must accurately explain any actual conversion/clock correction without promising bit-perfect output, but no direct-only command or preference is approved. O-003/O-006 retain close/tray and storage decisions for owner review. Internal UI adapters share these unresolved boundaries rather than independently selecting policy.

### Provisional synchronization diagnostics

D-034/O-005 remain Open. The internal status design distinguishes selected preference, actual processing, observed health, and endpoint mode, independently per direction. Comparison candidates are unchanged transfer, application-managed adaptive resampling, and Windows-managed shared-render rate adjustment. Observe mismatch/confidence, occupancy, effective ratio, processing transitions, discontinuities/recovery, and latency without publishing new field names, enums, commands, or protocol schemas here.

Automatic is the proposed prototype default; final algorithms and recovery permissions remain unresolved. Strict preservation stays internal/experimental, not a required public API capability: no intentional resampling, insertion, duplication, dropping, or other modification; incompatible formats are rejected; inability to continue is reported with explicit restart/rebuffer rather than silent compensation. No third public correction policy is added. Acceptance, current unchanged processing, and a scoped preservation observation do not prove end-to-end bit-perfect output. Public transport remains deferred under the existing decisions.
