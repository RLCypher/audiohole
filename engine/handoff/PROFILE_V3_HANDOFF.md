# Profile-v3 engine contract and GUI integration — validation 0.6

This implements the owner's current schema update in the engine module only.
The checked-out parent GUI/profile-builder documentation is an older baseline;
this handoff and the corrected supplied example describe the current engine
input. No GUI, controller, scanner record or saved profile is rewritten.

## Documents and loading compatibility

- Current individual routing record: `format: audiohole-profile`, `schema_version: 3`.
- Aggregate transfer record: `format: audiohole-test-configuration`, version 7.
  This is GUI import/export data, not an engine routing input. Export/pass an
  individual profile to the validation host. The engine rejects the aggregate
  rather than misreading version 7 as an individual profile version.
- Old individual v1 and v2 records remain loadable through a read-only in-memory
  projection. V1 uses hole `routing_engine` and the old hole advanced flag;
  v2 hole flags project to both directions, with `routing` preferred over its
  older key if present. V1 never activates an unknown exclusive-preference
  extension. V3 uses hole `routing` and direction flags, without falling back to
  stale hole flags. A v3 record containing only `routing_engine` is rejected.
- The public C++ Hole backend field remains named `routing_engine`; this is an
  internal API name, not the schema-v3 JSON key. The advanced flag now belongs
  to the typed Direction as well as the serialized direction.
- Parser bounds, duplicate-key rejection, original JSON/notes preservation and
  authoritative `settings.preferences` remain in effect. This typed model is
  not a serializer; the owning host retains the original document.

## Per-direction and per-endpoint resolution

Each `forward` and `reverse` owns independent booleans:

```
advanced_tuning
exclusive_mode_preferred
```

`advanced_tuning` gates that direction's retained rate/channel/format/mode/buffer
preferences and channel map. The other direction's flag has no influence.
Exclusive preference is independently active even when advanced tuning is off.
It applies to both endpoints of that direction, subject to effective forced modes.

When advanced tuning is enabled, each endpoint property still resolves endpoint
override → common → Automatic. Null means inherit, not cancel a common value.
For example, common Exclusive plus source Shared means required Shared capture
and required Exclusive playback. An endpoint null mode would inherit Exclusive.

| Effective mode after override resolution | Exclusive preference | Access policy |
|---|---|---|
| Shared | either | Shared only |
| Exclusive | either | Exclusive only; failure is an error |
| Automatic | false | Shared only |
| Automatic | true | Try Exclusive, then Shared if acquisition/validation fails |

Manual values retained under disabled advanced tuning are inactive, including
their mode. Backend `routing` remains one independent selector per hole:
automatic, wasapi, miniaudio or experimental. Preference never switches backends,
logical device assignments, or channel mappings to make a mode attempt succeed.

## Acquisition, cleanup and failures

The same non-real-time access negotiator is used by native WASAPI, the native
Experimental adapter and Miniaudio. Each stopped endpoint negotiation has at most
two acquisition attempts. Failed resources are released before Shared is attempted,
on the owning lifecycle/STA thread. All explicit non-mode requirements are
retained and validated on EACH attempt, including actual buffer allocation.
Automatic format choices may naturally differ between access modes.

Stop before/after a blocking API return releases resources and prevents another
mode attempt/late activation. No audio callback opens devices, allocates policy
data, serializes JSON or performs mode fallback.

Forced Exclusive has one attempt; it cannot silently become Shared. If a required
endpoint fails after activation admission, its PathSnapshot becomes Failed with
structured issues. A healthy sibling path continues and aggregate state becomes
Degraded; if none succeeds, aggregate state becomes Failed. Intent remains armed
until Stop, as before. Hosts must also display failed Runtime.start() admission
results (configuration, unavailable backend or known resource conflicts); no
streams are opened for an admission rejection. Unavailable/ambiguous bindings
retain the existing Ready-with-issues behavior when resolved by the worker;
they do not fabricate a failed native Exclusive attempt.

Version 0.6 also validates startup progress. A backend startup failure before its
first transfer, or no transferred frames within a 2-second validation watchdog,
can trigger one route reopen with unforced preferred endpoints Shared. Previously
Shared endpoints stay Shared; forced endpoint modes and every other explicit
requirement remain unchanged. Old route ownership is joined/released first. This
may reopen a healthy preferred capture endpoint as part of the same failed route;
the startup issue identifies route progress, not proof of which device is faulty.
If Shared cannot satisfy a retained manual requirement, failure is reported.

The previously successful open receipt for Exclusive is reclassified as failed
startup and carries its structured failure, followed by the Shared open receipt.
Existing open-time refusals survive for endpoints already Shared. The result is
checked again against ORIGINAL profile requirements. This is one pre-transfer
retry, not a retry loop or schema change. A forced mode, unsuccessful Shared
startup, cancellation or contract violation cannot silently become another mode.
An exhausted retry becomes Failed. Two zero-transfer starts can take roughly
four seconds in total. Driver API calls themselves are cooperatively cancellable
and do not have a hard timeout.

After the first transfer, device loss/streaming failure still reports failure and
requires explicit Stop/Start. There is no live mode migration of a flowing path,
backend substitution, automatic device recovery or global combinatorial search.

Repeated use of the SAME logical capture endpoint is planned as Shared when
compatible independent capture clients are needed. A preferred Exclusive claim
then records a known ResourceConflict refusal without pretending a driver call
was made, followed by an actual Shared attempt. Forced Exclusive on repeated
capture or multiple routes into one playback destination rejects admission.
Different logical records claiming one native endpoint remain Ambiguous; mode
preference never merges those identities.

## GUI/status contract

Runtime and its public snapshots remain independent of GUI widgets/transports.
No GUI was modified. For each source/destination StreamConfiguration, display:

- `access_policy`: automatic_shared, shared_required, exclusive_required or
  exclusive_preferred.
- `requested_mode`: effective explicit Shared/Exclusive, or null for Automatic.
- `exclusive_mode_preferred`: the direction's preference, even if forced mode wins.
- `mode`: actual successfully opened mode.
- `access_fell_back`: true only for a preferred Exclusive → Shared success.
- `access_attempts`: ordered mode, success, `native_open_attempted`, and structured
  issue code/location/detail/native code for each attempt. A reported Shared
  fallback requires a failed Exclusive attempt/refusal and a successful Shared
  attempt; backend postconditions reject an unreported fallback.

`progress_age_ms` advances from start admission before the first transfer, then
measures time since the last advancing transfer count. It is not audio latency.
Five new optional native counters (`native_capture_wakes`, `native_render_wakes`,
`native_wait_timeouts`, `render_capture_waits`, `render_capture_wait_timeouts`)
distinguish event servicing and bounded capture waits. Miniaudio leaves these
native observations absent. Hosts should detect the changed negotiated receipt
after startup retry; the new session's frame/wake counters restart from zero.
`capture_wait_timer_available` additionally reports optional high-resolution
timer availability/arming for the native short wait. It remains null for Miniaudio.

Successful Shared fallback is not a fatal path issue. Its Exclusive refusal
remains in attempt history so the GUI can explain the actual state. When all
attempts fail, path issues retain both mode-specific failures and their native
codes. The validation host writes equivalent fields in its logs/status output.
Known preflight conflicts use `native_open_attempted: false`; successful device
acquisitions always use true.

## Preferred clock metadata

`settings.preferences.preferred_clock_device` is retained as bounded string
metadata on Direction preferences and prepared route intent, regardless of
advanced tuning. Unknown future string choices are retained. Negotiated snapshots
echo the saved value and report `clock_preference_applied: false`. Destination
pacing and the existing synchronization algorithm remain unchanged. No significant
benefit from changing clock reference has been established, so this build does
not claim or implement it. No schema field physically synchronizes independent
device oscillators.

## Hardware validation with the corrected example

The corrected example has RCP2 Chat → Front Audio Port forward, and RCP2 Main →
RCP2 VirtA reverse. Their captures are distinct. Forward advanced=true and reverse
advanced=false both have exclusive preference=true; both directions should
prefer Exclusive on Automatic endpoints. Their saved clock choices remain inert.

1. Put the corrected v3 record in `Profile`, select it, Start and inspect status.
   Check actual modes and all attempt receipts independently for both directions.
2. For a fallback test, use an endpoint with Windows Exclusive access disallowed
   while Shared is available. For example, temporarily disable that endpoint's
   “Allow applications to take exclusive control” setting and restore it after
   the test. With preference=true and effective mode Automatic, expect a logged
   Exclusive refusal followed by Shared success if the remaining requirements fit.
3. For the strict test, enable advanced tuning and force Exclusive on that same
   endpoint. Expect a Failed path with its mode-specific error and no Shared
   attempt. Other healthy paths may continue. Stop and verify cleanup afterward.
4. Restore the intended endpoint setting and collect a normal longer hardware
   run. Driver/hardware coupling can still make both acquisition modes fail;
   that must be reported, not worked around by changing devices/requirements.

Portable acquisition callbacks and lifecycle fixtures are strictly software tests
of these contracts. They do not establish live Windows negotiation, capture reuse
or hardware success. No fake device or test route is automatically loaded by the
standalone executable; the corrected example is a parser fixture in source only.
