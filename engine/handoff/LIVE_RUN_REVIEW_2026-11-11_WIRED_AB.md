# Validation 0.3 — wired front-output comparison

The user reports approximately 148 ms with Low and 168 ms with baseline. This
pair therefore reports an approximately 20-ms acoustic improvement with Low.
The app's algorithm, timestamp calibration and individual echo series remain
unverified. If the same approximately 10-ms direct-phone reference applies,
the provisional adjusted estimates are 138 and 158 ms; the A/B difference stays
20 ms. This does not isolate AudioHole's contribution.

## Provenance and route

Both attached logs parse strictly without duplicate keys or trailing corruption.
The complete embedded profile records match. Files and SHA-256:

- Low: `20261111T214425Z-run-977177137cd0c85e8a9ec67d4eef5dfc-1.txt`,
  840,105 bytes, 258 JSON records,
  `d624388578f7100ae334acfd01f81b3bf3bc8ce8d9339f9cf9067abaab12107e`.
- Baseline: `20261111T214916Z-run-38ad241d39bfe50248b549e8664e02b0-1.txt`,
  93,514 bytes, 31 JSON records,
  `7e36584f554273a6ff59cc71e2ec39f9a0beb5efa1d7b7b372dec0386003da18`.

The profile is `Proof of Concept Workflow Wired Latency Test.json`, profile-3,
hole-4, source RCP2 Main / `capture-bbe843a3366d74575eafd8563f860a43`,
destination Front Audio Port / `render-3ebb8d7988f952ba756b87367def0660`.
Its notes describe wired headphones via an internal USB sound card; the log
does not independently establish the output's physical manufacturer or driver.

The hole is simplex (`duplex: false`). Retained reverse settings reference the
old Corsair/RCP2 Chat route, but are inactive: each snapshot contains ONLY the
forward path. This is correct handling of retained reverse preferences.

Both presets actually select native WASAPI / Shared / 48 kHz / stereo float32,
with Automatic preferences and advanced overrides disabled. No nominal sample
rate or channel-count mismatch is negotiated. This does not establish absence
of endpoint DSP, hardware conversion or adaptive synchronization.

## Applied settings and observations

| Observation | Baseline | Low |
|---|---:|---:|
| Time to Stop request | 24.956 sec | 270.672 sec |
| Running snapshots | 22 | 249 |
| RCP2 capture period | 480 frames / 10 ms | 336 / 7 ms |
| Front-output playback period | 480 / 10 ms | 480 / 10 ms |
| Source allocated buffer | 1,056 / 22 ms | 738 / 15.375 ms |
| Destination allocated buffer | 1,056 / 22 ms | 1,056 / 22 ms |
| Render-ahead target | 1,056 / 22 ms | 528 / 11 ms |
| Sampled render padding before submission | 576 / 12 ms | 48 / 1 ms |
| Initial FIFO target | 960 / 20 ms | 960 / 20 ms |
| Active FIFO target | 1,538 / 32.0417 ms | 960 / 20 ms |
| Largest capture packet | 480 / 10 ms | 336 / 7 ms |
| Largest render request | 1,056 / 22 ms | 528 / 11 ms |
| Median sampled FIFO, all Running snapshots | 1,593 / 33.1875 ms | 720 / 15 ms |
| FIFO underruns / overruns | 0 / 0 | 0 / 0 |
| Continuing capture discontinuities / timestamp errors | 0 / 0 | 0 / 0 |
| Longest capture delivery gap | 10.720 ms | 7.735 ms |
| Largest capture drain | 480 frames | 336 frames |

Low's minimum period requests succeed without fallback. The front playback
endpoint advertises a fixed 480-frame Client3 period in this configuration;
RCP2 advertises minimum 336 / default 480. Thus the 5-ms FIFO floor still cannot
take effect through the current two-period reserve.

The baseline median is from only the first approximately 25 seconds, while Low
runs approximately 4.5 minutes. It is not a settled, equal-duration comparison.
After excluding the first 30 sec of Low, its median is still 720 frames / 15 ms.
There are no corresponding post-30-sec baseline observations. Do not sum these
medians and lead targets into an exact per-signal delay budget.

## A concrete engine-side buffering finding

Baseline's first Running snapshot at 1.265 sec already records a 1,056-frame
render request and active target 1,538. The source code retains the largest
render request for the entire session, then calculates the safety target as
at least one capture packet plus that largest request plus interpolation room:

```
480 capture + 1056 render + 2 lookahead = 1538 source frames = 32.0417 ms
```

This is a full allocation-sized refill during startup, not evidence that the
routine render period changed from 480 frames. Later sampled writes consistently
begin with 576 frames queued and restore the 1,056-frame lead, implying normal
480-frame refills at those observations. The maximum nevertheless remains and
keeps the controller target high; it cannot decay within the current session.

Low's largest request, also visible in its first Running snapshot at 0.734 sec,
is only 528 frames. Its corresponding guard is `336 + 528 + 2 = 866`, below
the unchanged 960-frame initial target, so it does not raise the FIFO target.

This persistent startup-derived reserve is a concrete engine-side candidate
for improvement. Simply deleting the guard would be unsafe: genuinely large
runtime refill demands still require handling. A future change should separate
startup/empty-buffer recovery from routine pacing and validate the demand it
actually services. No such behavior change is implemented by this review.

The smaller render lead, smaller source period, and lower active FIFO target
are consistent with the reported approximately 20-ms improvement. They do not
independently prove its exact acoustic magnitude or allocate it among stages.

## Stability and interpretation across devices

Both paths remain Running until Stop, with monotonic frame/fault counters and
no backend issues. Each has one startup discontinuity. Both also record exactly
one zero-padding render observation before the first Running snapshot; that
counter alone is not a hardware underrun report or proof of audible disruption.
After Stop, counters remain unchanged through Quit: six Stopped snapshots for
Low and five for baseline. Baseline's short duration is insufficient to rule
out the earlier approximately 17–20-second intermittent issue over longer runs.

The previous wireless profile used a DIFFERENT destination endpoint (Corsair
Virtuoso) and duplex routing. This profile uses Front Audio Port and simplex
routing. It changes the driver/output path and potentially transducer/DSP as
well as transport. The wired result being higher than approximately 116 ms is
not a measurement of wireless overhead and does not establish wired audio as
inherently slower. The logs do not prove that wireless transport dominates the
earlier result. Front-output driver queues and Windows/vendor audio effects
remain possible contributors; they are not diagnosed by this telemetry.

Next useful work: investigate the startup-derived guard, obtain a longer
baseline under the same conditions, and inspect the specific front-output
processing path. Preserve the same phone placement/settings and note the
headphones used. Native packet timestamp age is not a physical microphone or
end-to-end latency measurement.

No engine implementation, GUI, saved profile, notes, or packaged artifact is
changed by this review. No new build or hardware fix is claimed.
