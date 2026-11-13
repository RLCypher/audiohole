# Validation 0.4 — native Exclusive on the wired route

The user reports approximately 92 ms average over 300 LagMeter pings, their
lowest reported result so far. This is external app/user measurement evidence;
the attachment contains engine snapshots, not the individual ping measurements,
their spread, or a calibrated component-latency result.

Input: `20261112T011008Z-run-41584973a4f3f3747a69794fb4a0d780-1.txt`,
2,590,749 bytes, SHA-256
`514b1f2ee1580d0a72bd2c7204441eef5051a95f955566027af1a822f1ca3060`.
All 625 concatenated JSON objects parse strictly, without duplicate keys or
trailing corruption: one Start, 614 Running snapshots, one Stop, eight Stopped
snapshots, and one Quit. Stop is requested at 667,003 ms, approximately 11 min
7 sec. The final record is at 674,505 ms, Stopped and disarmed.

## Intent and actual opened streams

Profile `Exclusive Mode Latency Test.json` selects native `wasapi` explicitly,
with one enabled simplex hole and advanced tuning enabled. Forward common mode
is `Exclusive`; endpoint modes are null and correctly inherit it. Rate, channels,
format and buffer preferences remain Automatic. Retained reverse settings are
inactive; only the forward path is opened.

Logical endpoints match the earlier wired comparisons:
RCP2 Main capture (`capture-bbe843a3366d74575eafd8563f860a43`) to Front Audio Port
playback (`render-3ebb8d7988f952ba756b87367def0660`). Both actually open Exclusive.
Neither silently falls back to Shared. Both clients are 48 kHz, two channels,
`pcm_s32`, with 480-frame / 10-ms periods and allocations. This is client and
reported configured-format information, not proof of 32-bit physical converter
precision. The previous Shared clients used float32. Internal processing remains
the same float32/linear adaptive-SRC prototype; Exclusive is not bit-perfect mode.

The launcher preset is Low, but its smaller Shared-period and Shared render-lead
policies do not apply to Exclusive. The 5-ms Low queue floor is also overridden
by the two reported 10-ms periods. This is evidence for the resulting Exclusive
configuration, not an additional Low-preset optimization within Exclusive.

## Comparison with previous native wired runs

| Observation | Shared Standard | Shared Low | This Exclusive run |
|---|---:|---:|---:|
| User-reported latency | ~156 ms | ~148 ms | ~92 ms / 300 pings |
| Capture period | 10 ms | 7 ms | 10 ms |
| Playback period | 10 ms | 10 ms | 10 ms |
| Capture allocation | 22 ms | 15.375 ms | 10 ms |
| Playback allocation | 22 ms | 22 ms | 10 ms |
| Playback lead target | 22 ms | 11 ms | 10 ms |
| Active FIFO target | 20.0417 ms | 20 ms | 20.0417 ms |
| Client sample representation | float32 | float32 | signed PCM32 |
| FIFO underruns | 0 in 96.873 sec | 0 in 152.869 sec | 1 in 667.003 sec |

The approximate improvement is 56 ms / 38% against Shared Low and 64 ms / 41%
against Shared Standard. These are differences in the user's reported averages,
not independently calibrated per-component measurements or a matched-duration
stability comparison. If the same tentative 10-ms direct-phone reference still
applies, the adjusted Exclusive estimate is about 82 ms; that correction remains
hypothetical and does not isolate AudioHole.

Visible queue/period values alone do not account for the full 56-ms difference.
In particular, Exclusive playback lead is only 1 ms below Shared Low, its capture
period is longer, and its active software target is essentially unchanged. The
sampled FIFO median after 30 seconds is 962 frames / 20.0417 ms, versus 745 /
15.5208 ms in the prior Shared Low trace. These independently sampled medians
must not be summed into a sample-transit latency budget.

The changed Windows/driver/format-processing path is therefore an important
candidate contributor. Exclusive can bypass shared-engine mixing/processing,
while driver/hardware behavior and the selected client format also differ.
This run does not isolate the audio engine, an APO/effect, driver queues, mixer
DSP, or either endpoint as the dominant source of the gain.

## One real starvation event remains

The first observation of the sole FIFO underrun is at 65,517 ms. No further
increment appears during approximately the following ten minutes. The event
records:

- 465 source frames available for a 480-frame output block.
- 464 valid output frames, leaving 16 frames / 0.333 ms silent within that block.
  This excludes subsequent re-priming silence and is not total glitch duration.
- Active target 962 frames; requested correction -45.772 ppm, already slowing
  consumption. The correction is an actuator command, not physical clock drift.
- Last nonempty capture-drain gap 9.986 ms; session maximum gap 10.416 ms.
- Last valid capture packet age at receipt 12 microseconds; render-call QPC is
  approximately 10.0129 ms after its capture QPC, or 10.0009 ms after receipt.
- Shared-mode padding context is correctly absent for Exclusive. There is no
  observation here establishing the headroom available before Exclusive's next
  device deadline, so the earlier Shared 12-ms deferral opportunity cannot be
  assumed to apply.

This locates another application FIFO-starvation event near a normal capture
cadence. It is not evidence of a gross 20-ms packet/stall or a demonstrated
clock mismatch. Cause remains unresolved; the event also shows Exclusive did
not eliminate the common synchronization problem.

## Other stability and accounting evidence

No FIFO overrun, continuing/startup capture discontinuity, timestamp error,
backend issue, or dropped underrun context is recorded. All Running progress
ages are zero; client frame counts are monotonic and advance near nominal rate.
The pipeline's initial target is 960, active 962, capacity 9,600 source frames.
Capture packets and pipeline/native render requests are consistently 480 frames.

Native startup-prefix silence is zero, as intended for Exclusive. Initial native
preroll is 480 frames. Final accounting balances:

```
32,008,320 native submitted = 480 initial silence + 0 prefix + 32,007,840 pipeline rendered
```

Final captured is 32,005,920 and transferred is 32,004,464. Pipeline rendered
minus transferred is 3,376 frames / 70.333 ms, including startup priming and
post-starvation silence; it is not a single delay or the route latency. All
eight Stopped snapshots preserve identical path counters through Quit.

## Next discriminating test

Hold the wired route, phone setup, backend and remaining Automatic fields fixed,
and compare Exclusive capture / Shared playback with Shared capture / Exclusive
playback. The existing endpoint-over-common mode inheritance supports these
tests. They would identify which endpoint access path contributes most, while
recognizing that automatic client format can also change with access mode.

Keep the current periods for that comparison; tuning smaller Exclusive buffers
at the same time would introduce another variable. Continue investigating the
remaining starvation without silently relaxing explicit Exclusive requirements.
An opt-in exclusive-preferred/shared-fallback policy remains a separate proposed
contract change, not implemented or validated by this run.

No application code, GUI, saved profile, notes or published archive is changed
by this review. No new build or permanent mode/backend decision is claimed.
