# Validation 0.6 — wireless duplex and real startup fallback

The user reports listening through a complete YouTube video, walking the wireless
headset out of range several times, and continued routing after the log bound
was reached. The log establishes successful application frame transfer during
its recorded window. Audible behavior and operation after that window are user
observations, not independently measured by the engine telemetry.

Input: `20261113T005944Z-run-14e3133673d2387355c0da53da6af5c8-2.txt`,
9,756,882 bytes, SHA-256
`9a1edb308f6c867862736572b1f4a2d6f7eed517e5e1cf12d8e5ba6f3e0791cc`.
All 995 concatenated JSON objects parse strictly without duplicate keys,
non-finite constants, trailing corruption or an incomplete final object. They
comprise one Start and 994 snapshots: one Starting, one Degraded and 992 Running.
The last snapshot is elapsed 1,082,045 ms, approximately 18 min 2 sec, Running
and armed. No Stop or Quit is recorded. UTC timestamps span November 13,
00:59:44Z–01:17:47Z (November 12 in the user's Pacific time zone).

## Important differences from the previous test

This is build 0.6 with the **Low** preset, not the prior baseline preset.
The forward destination is now **RCP2 Main playback**, not RCP2 VirtA. Reverse
remains RCP2 Main capture to Corsair Virtuoso Headphone. Thus it is not the
unchanged-profile baseline A/B test requested in the repair guide. This limits
causal claims about the underrun fix and VirtA compatibility; it does not weaken
the observed startup-recovery and sustained-transfer evidence for this setup.

The individual schema-3 profile has one enabled duplex hole, Automatic backend
selection, disabled advanced tuning in both directions and all manual fields
Automatic. Forward independently prefers Exclusive; reverse does not. Native
WASAPI is selected in both directions. Clock preference remains unapplied metadata.

## Startup fallback works on real hardware

Forward is Corsair Virtuoso Mic capture to RCP2 Main playback. It initially opens
both Exclusive: 48 kHz mono packed PCM24 capture and stereo PCM24-in-32 playback,
with 480-frame / 10-ms periods and allocations. At elapsed 1,151 ms, capture has
advanced to 49,920 frames, but transferred frames are zero. Pipeline rendered
stays 480; native render wakes are one versus 104 capture wakes. The queue has
already overrun 84 times in this last observed failed-start snapshot.

This repeats the previous non-progress signature on a DIFFERENT RCP2 playback
endpoint. VirtA alone is therefore not an established explanation. The new
fair event handling does not by itself resolve this instance. A single serviced
render event does not identify whether driver, format or another native condition
causes the lack of subsequent output progress.

By elapsed 2,239 ms, forward is Running with both endpoints Shared. Both receipts
show the unsuccessful Exclusive startup (`source/startup_progress` and
`destination/startup_progress`, no fabricated HRESULT), successful Shared open,
and `access_fell_back: true`. Both source and destination are reopened under the
documented route-level retry policy; the source diagnostic is not proof that
capture itself was faulty. New-session counters reset, explaining why later
overrun counts are zero despite the failed initial attempt.

The watchdog/release/reopen mechanism has therefore succeeded on this hardware.
It recovers the preferred route; it does not demonstrate successful forced
Exclusive operation on RCP2 playback. Strict-mode refusal/stall still needs its
separate real-hardware validation. Reverse remains Running throughout the
recorded forward recovery; its frame counters never reset.

## Sustained functioning configuration

All Running configurations remain constant within each direction:

| Observation | Forward after fallback | Reverse/headphones |
|---|---|---|
| Actual access | Shared / Shared | Shared / Shared |
| PCM client formats | 48 kHz stereo float32 / float32 | 48 kHz stereo float32 / float32 |
| Capture period | 480 frames / 10 ms | 336 frames / 7 ms |
| Capture allocation | 1,056 frames / 22 ms | 738 frames / 15.375 ms |
| Playback period | 480 frames / 10 ms | 480 frames / 10 ms |
| Playback allocation | 1,056 frames / 22 ms | 1,056 frames / 22 ms |
| Playback lead target | 528 frames / 11 ms | 528 frames / 11 ms |
| Active FIFO target | 962 frames / 20.0417 ms | 960 frames / 20 ms |
| FIFO capacity | 9,600 frames | 9,600 frames |
| Final transferred frames | 51,830,880 | 51,926,400 |
| FIFO underruns after recovery | **0** | **0** |
| FIFO overruns after recovery | **0** | **0** |
| Continuing capture discontinuities | **0** | **0** |
| Timestamp errors | **0** | **0** |
| Separately classified startup discontinuities | 1 | 1 |

Captured/rendered/transferred frame counts and native capture/render wake counts
increase at every adjacent Running snapshot, with no resets or non-increasing
progress intervals. Progress age is zero at every Running observation. No path
issue is reported. Native render-empty wakes, native main-loop timeouts and
dropped underrun contexts are zero. The maximum snapshot gap is 1,107 ms.

Final native accounting balances for both directions:

```
Forward: 51,832,368 submitted = 528 initial silence + 0 prefix + 51,831,840 pipeline rendered
Reverse: 51,928,368 submitted = 528 initial silence + 0 prefix + 51,927,840 pipeline rendered
```

Float32 processing, linear adaptive SRC and occupancy PI remain the prototype
pipeline. No measured clock drift, end-to-end latency, CPU usage, RF statistics
or physical output verification is supplied by this log.

## What this does and does not validate

This is substantially better observed application stability than the previous
baseline run's five headphone FIFO underruns in four minutes. However, **both
capture-wait counters stay zero in both directions**. The high-resolution timer
is available, but the new short-wait mechanism is not exercised here. In Low,
observed pre-submit playback padding is 48 frames / 1 ms, below the short wait's
3-ms minimum headroom. The changed 7-ms capture cadence and lower playback lead
also make a direct baseline causal comparison inappropriate. No permanent fix
for the earlier baseline starvation is established by this run alone.

Walking beyond RF range can interrupt the headset's radio audio while leaving
its USB dongle and Windows endpoint connected. The lack of device/backend errors
and continuing frame progress support resilient stream continuity at the
application boundary, consistent with the user's audible-return report. They
do not prove uninterrupted RF audio, detect its exact outages, or validate USB
unplug/replug, endpoint removal or automatic engine device recovery. The latter
is still a documented Stop/Start limitation of this prototype.

## Log bound and next discriminating test

The validation host limits compact `.jsonl` and readable `.txt` output to a
combined 16 MiB per run. Its logger closes both files at the next full-record
boundary and leaves routing running. A roughly 9.76-MB text file is compatible
with the combined cap. This clean, Running ending and the user's reported
continuation match that design. Operation after the final snapshot and eventual
Stop are not covered by the attachment; it is not a crash or a recorded shutdown.

Repeat this exact working profile with `Run-Validation.cmd` (baseline), keeping
the same endpoints and external audio conditions. Compare startup recovery,
underrun counts, capture-wait counts/timeouts and any audible glitches over a
similar window. Baseline's larger native padding can exercise the short wait
that Low never used. Test VirtA and strict Exclusive separately after that,
without changing multiple comparison variables at once.

This review updates documentation only. Engine code, GUI, profiles, notes and
published 0.6 source/binary archives remain unchanged. No new build or additional
hardware test by the build agent is claimed.
