# Validation prototype changes

## 0.6

- Bound zero-transfer startup to a 2-second validation watchdog and report an
  actionable `startup_progress` failure. Age is visible before the first frame.
- Before any valid transfer, an Exclusive-preferred route can retry once with
  its unforced preferred endpoints Shared. Join/release precedes reopening;
  explicit modes, formats, rates, buffers and maps remain requirements. Native
  Start failures can also trigger this retry; forced Exclusive never downgrades.
  Successful fallback receipts preserve the startup failure and actual new mode.
- Claim pending render events alongside capture notifications so capture cannot
  monopolize a multi-event wait. Count native wakes and timeouts for diagnosis.
- For impending FIFO starvation in native Shared output, permit one requested
  1-ms capture wait only when measured playback padding leaves a 2-ms reserve.
  Drain again, refresh padding and submit the original bounded block. Do not
  enlarge the FIFO, alter the DSP/controller, fabricate an Exclusive event, or
  hide starvation when capture remains unavailable. OS waits can overshoot;
  actual device stability and latency need a real-hardware rerun.
- Add focused zero-progress/startup/retry/cancellation lifecycle fixtures and
  a numerical late-capture regression. These are necessary software tests,
  not devices installed or simulated by the validation application.
- No GUI, input profile, schema, filenames, notes or clock preference changed.

## 0.5.1

Clean Windows rebuild with updated release identification. Routing, schema-v3
support and access policies are unchanged from 0.5. This build remains unsigned;
no Smart App Control acceptance or trusted publisher status is claimed.

## 0.5

- Accept current individual profile schema 3: hole `routing`, direction-owned
  `advanced_tuning` and `exclusive_mode_preferred`, authoritative preferences.
  Preserve read-only v1/v2 loading compatibility; v7 aggregate transfers remain
  separate from routing inputs. Retain clock preference as unapplied metadata.
- Use one stopped endpoint-acquisition policy across native/Experimental and
  Miniaudio. Effective forced modes are strict; preference without forced mode
  tries Exclusive then Shared with complete cleanup and unchanged requirements.
- Report requested policy, actual access mode, fallback and per-attempt issues
  independently for capture/playback. Forced failures become structured path
  errors; healthy sibling paths can continue. Stop prevents retry/late activation.
- Plan known repeated capture use for compatible Shared clients, recording the
  preflight Exclusive conflict. Forced ownership conflicts remain errors and
  ambiguous identities are never merged. No implicit destination mixing.
- Add schema, policy, cancellation, cleanup and Failed/Degraded lifecycle tests,
  including the owner's corrected profile. No live Windows success is inferred
  from software fixtures or cross-compilation. GUI and saved files are unchanged.

## 0.4

- During native automatic Shared priming, handle an oversized initial refill
  with an explicitly counted silent prefix plus at most one period of pipeline
  output. This prevents that startup-only request from permanently inflating
  the FIFO's render-demand reserve in the reproduced case.
- After the first valid source transfer, retain full-demand safety handling.
  Explicit buffers, Exclusive mode and unknown timing keep their prior startup
  behavior. Controller tuning, steady pacing and observed-packet protection stay
  unchanged. Hardware evidence is still needed for this changed startup path.
- Separate native preroll, startup prefix, submitted frames and largest native
  write from PCM pipeline progress so priming cannot masquerade as valid audio.
- Publish last-underrun block/queue/correction context through an eight-slot
  SPSC telemetry mailbox. Native renders attach QPC/capture/padding context;
  unsupported fields remain absent. Full mailboxes drop counted telemetry only.
- Add startup-demand, runtime-protection, output-boundary, event persistence and
  diagnostic-overflow regressions. No new latency result or permanent hardware
  underrun fix is claimed. Old validation packages remain unchanged.

## 0.3

- Add opt-in `--low-latency` native tuning and a separate launcher. Default
  scheduling preserves the 0.2 baseline for controlled comparison.
- Request the smallest legal Client3 shared period, retaining the same PCM
  tuple and recording fallback to default/legacy shared operation. No implicit
  Exclusive mode or change to manual profile requirements.
- Low automatic shared render submissions aim for one accepted period plus a
  1-ms margin instead of filling the allocation. Explicit endpoint buffers,
  Exclusive streams and unknown periods retain full-buffer behavior.
- Reduce only the native Low software queue floor to 5 ms; retain two periods,
  actual packet/render reserve, bounded storage and existing controller rules.
- Log advertised/accepted periods, native packet receipt timing, capture drain
  burst totals, and shared playback padding. Zero-padding observations are not
  labeled hardware underruns; QPC packet age is not acoustic latency.
- Add portable policy/extent tests and small-period cadence regressions. Real
  latency reduction and stability of the changed preset still need hardware
  comparison. GUI, profile schema, original profiles and device notes are untouched.
- Preserve the original 0.1/0.2 archives. Include later live-run reviews and the
  LagMeter signal path in the new package's reference material.

## 0.2

- Guard the FIFO using observed capture packets and render requests. Engine
  period alone does not establish packet cadence. The active target can rise
  for large packets/blocks and is reported; no callback resizes the queue.
- Replace the underdamped fixed PI proportional gain with a critically damped
  ideal-model relation to the current target. Add saturation anti-windup and
  exclude priming from feedback. These settings still need hardware comparison.
- At exact interpolation phase, read only the published current frame.
- Report packet/block min/max and the active controller target separately from
  initial target/capacity and reported periods.
- Log Stop/Quit request events AFTER requesting cancellation, preserving Stop
  precedence over disk I/O. Logged completion remains snapshot-based.
- Add zero-clock-skew packet-cadence regression coverage for both controllers.
  A longer PI model improves from 35 underruns to zero with 20-ms capture packets
  and 10-ms rendering. This is not proof of Corsair packet cadence/hardware fix.
- Profile record v1, override semantics, stream/buffer requirements, bindings and
  GUI code are unchanged. Original 0.1 archives remain immutable for comparison.

## 0.1

Initial separate module, native/miniaudio/Experimental selectors, independent
duplex requests and console validation host. First real run: Automatic resolves
native WASAPI/Shared; both directions process buffers for about 15 minutes 25
seconds. The user confirms forward audibility and clean audible Stop. Forward
has zero FIFO underruns; reverse has 42. See the detailed hardware review.
