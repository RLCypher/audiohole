# Validation 0.6 — startup and capture timing repair

The 0.5.1 hardware logs show two independent problems: Exclusive RCP2 VirtA
playback opens but never advances beyond its initial silent output, and a
working Shared headphone route occasionally requests PCM just before its next
capture packet arrives. No specific driver cause is established. This build
repairs supervisor handling of unusable startup and tests a bounded native
scheduling change; real hardware must establish whether it resolves the symptoms.

## What changed

The supervisor now reports startup age before the first transfer. A route with
no transferred frames after a 2-second validation watchdog fails rather than
remaining Starting indefinitely. Before the first transfer only, an eligible
Exclusive preference permits one cleanup/reopen with preferred, unforced
endpoints Shared. Native startup errors can also trigger that retry. Both source
and destination can be reopened; this does not prove both were at fault. Forced
Shared/Exclusive and all non-mode requirements remain authoritative. An explicit
buffer/format unsupported in Shared still fails. Stop suppresses pending retry.

The old route is joined and releases endpoint ownership before the retry. The
new negotiated receipt preserves original intent, failed Exclusive startup and
successful Shared opening. A receipt's `succeeded` describes accepted acquisition
including startup qualification, not merely an Initialize return. No invented
HRESULT represents zero progress. Retry frame/wake counters restart at zero;
`access_fell_back` and the receipt explain the transition. Failed Shared startup
also becomes Failed, with no retry loop. After any source-derived transfer, this
build never changes modes automatically on a later failure.

Native I/O also checks for a pending render notification when capture wins the
multi-event wait. It never invents an Exclusive buffer notification. For a primed
Shared route about to run short, measured playback padding can permit one short
capture wait: requested 1 ms, with at least 2 ms remaining submission reserve.
The worker drains capture again, re-reads padding and submits the ORIGINAL block
size; it does not increase the FIFO target or request a catch-up block because
of the wait. Normal starvation accounting still applies if no packet arrives.
No output buffer is held during this wait. No new DSP, FIFO size, clock-master
policy or profile fields are introduced.

A per-route high-resolution one-shot Windows timer provides the requested wait
deadline; there is no global timer-resolution change. If timer creation/arming
is unavailable, immediate rendering remains. Windows scheduling/API delays can
still exceed the requested duration. This is a measured-headroom experiment,
not a hard real-time guarantee. The minimum 3-ms budget and 2-s startup watchdog
are documented validation constants, not production quality limits.

The Windows behavior follows the documented
[multi-event wait priority](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)
and uses an optional
[high-resolution timer](https://learn.microsoft.com/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).
The native ownership/Exclusive full-buffer contract remains unchanged.

## First hardware run

1. Extract 0.6 separately; preserve the 0.5.1 package and logs for comparison.
2. Copy the SAME `Mixer to Wireless Headset.json` into `Profile`. Keep its RCP2
   Main → Corsair Shared direction and Corsair mic → VirtA Exclusive-preferred
   direction unchanged. Do not change access modes, backend or buffers yet.
3. Run `Run-Validation.cmd`, select the profile and Start. Observe status after
   about five seconds. The VirtA path should either progress in Exclusive or
   release/reopen in Shared with `access_fell_back: true` and a startup issue
   inside its attempt receipt. A truly unusable path must report Failed.
4. Run for at least five minutes; compare headphone-route underruns against the
   previous five in four minutes. Log `render_capture_waits`, timeouts and native
   padding alongside any remaining underrun event. No audible input is needed
   to verify that a silent active path consumes and transfers frames.
5. Stop and wait for Stopped. Repeat Start/Stop a few times, then return the logs
   with any audible behavior and the latency method/readings. A browser tapping
   estimate is still approximate; do not compare it as an objective improvement
   over the earlier acoustic test.

If this succeeds, repeat longer runs and the Low launcher with the same profile.
Test forced Exclusive separately on a copy: refusal/stall must produce Failed
with an issue, never silently Shared. This build does not promise to make a
non-progressing driver work in forced Exclusive; its compatible preference path
can recover through Shared without weakening the required-mode contract.

## New telemetry

- `native_capture_wakes`, `native_render_wakes`, `native_wait_timeouts`: native
  main-loop event servicing, not physical clock or audible glitch counts.
- `render_capture_waits`, `render_capture_wait_timeouts`: short capture waits and
  those ending at the timer deadline rather than a capture notification.
- `capture_wait_timer_available`: timer creation/most recent attempted arming
  succeeded. These native-only fields remain null for Miniaudio.
- `progress_age_ms`: age since start admission before the first transfer, then
  age since the last advancing transfer observation. Not sample transit latency.

The GUI is unchanged. Profiles, notes, endpoint identities and user data are not
rewritten. There are no synthetic devices in the host; lifecycle fixtures and
numerical PCM tests exist only in the test executables. Passing those tests and
cross-compilation cannot substitute for the real-hardware run above.
