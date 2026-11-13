# Validation 0.5.1 — preferred versus required Exclusive

The user reports approximately 92 ms latency with Exclusive preferred and 84 ms
with forced Exclusive. These are external/user measurements; neither log contains
the individual latency samples, their timing, or their distribution. No phone
reference subtraction is applied here. The logs do not establish an inherent
8-ms benefit from making Exclusive mandatory.

## Evidence integrity and duration

Both files strictly parse as concatenated JSON objects, with duplicate keys and
non-finite constants rejected and no trailing corruption:

| Input | Bytes | Objects | Stop request |
|---|---:|---:|---:|
| `20261112T211023Z-run-ace79de0ea48bdb58875f77faf87a778-1.txt` | 5,302,694 | 595 | 300,075 ms |
| `20261112T212106Z-run-18827802722818817a439393c0e57432-2.txt` | 967,535 | 119 | 123,508 ms |

SHA-256, respectively:

```
68696d3a74545f7ede67400b43d30a71cb5b09d415ba05dd6b786ebd95e7c5ad
20cef05a97c5888b7ea518510f8409d68949e87543c06c91f16ad85460c203e8
```

Both identify build 0.5.1 and the baseline preset. The first log continues with
Stopped snapshots until 642,895 ms; those extra minutes are idle logging, not
additional routing time. All post-stop counters are constant in both files,
with Stopped/disarmed status. The second includes Quit at 125,046 ms.

## Forward route opens identically

Both profiles route RCP2 Chat capture to Front Audio Port playback. Both choose
native WASAPI through `routing: automatic`. All forward negotiated records are
constant within each run. Their actual I/O and processing settings match:

- Source and destination Exclusive, 48 kHz, two channels, `pcm_s32`.
- Source and destination 480-frame / 10-ms periods and allocated buffers.
- Destination render lead 480 frames / 10 ms.
- Initial FIFO target 960 frames, active target 962 frames, capacity 9,600 frames.
- Float32 internal processing, linear adaptive SRC and occupancy PI controller.
- `preferred_clock_device: capture` retained, `clock_preference_applied: false`.

The first run reports `exclusive_preferred`, no forced mode, one successful
Exclusive open per endpoint, and `access_fell_back: false`. The second reports
`exclusive_required`, requested mode Exclusive, also one successful Exclusive
open per endpoint with no fallback. The preferred policy does not add another
live processing stage: successful acquisition exits negotiation immediately.
Only the failure policy differs for these forward endpoints.

This establishes successful preferred-Exclusive acquisition on real hardware.
It does not test preferred Shared fallback or forced-Exclusive rejection and
GUI error presentation.

## Queue history is a plausible contributor, not a latency measurement

The preferred forward path has one FIFO underrun, first observed at 52,181 ms.
Its event records 450 source frames available, a 480-frame output request, and
449 produced frames. Thus 31 frames / 0.646 ms in that block are silent; this
excludes subsequent re-priming silence. No second underrun occurs before Stop.
The forced forward path records zero underruns. Both record zero forward FIFO
overruns, capture discontinuities and timestamp errors.

Before the underrun, sampled queue levels are similar: each has a median of
431 frames in the 30–52 second interval. After re-priming the preferred queue
generally occupies an additional block. Between 90 and 125 seconds its sampled
median is 966 frames, versus 480 in the forced run. Across active snapshots
after 30 seconds, medians are 962 and 473.5 frames, respectively, a difference
of approximately 10.18 ms at 48 kHz.

These snapshots occur independently of sample-transit measurement and callback
phase, and the runs have different lengths and different reverse behavior.
They support investigating queue/re-priming behavior as a possible contributor
to the user's approximately 8-ms observation. They do not prove that contribution,
locate the latency samples relative to the underrun, or isolate endpoint/driver,
DSP or phone measurement effects. Requested correction is a controller command,
not a measured physical clock mismatch.

## Reverse route did not transfer in the preferred run

Both profiles request RCP2 Main capture to RCP2 VirtA playback, but their effective
access intent differs:

| Observation | Preferred run | Forced-forward run |
|---|---|---|
| Reverse advanced tuning | false | false |
| Reverse Exclusive preferred | true | false |
| Actual reverse modes | Exclusive / Exclusive | Shared / Shared |
| Reverse representations | PCM32 / 24 valid bits in 32 | float32 / float32 |
| Reverse pipeline rendered | 480 frames | 5,916,480 frames |
| Reverse transferred | **0 frames** | 5,915,520 frames |
| Reverse FIFO overruns | **29,962** | 0 |
| Reverse runtime state before Stop | Starting | Running |
| Overall state before Stop | Degraded | Running |

The second profile forces Exclusive only in forward common preferences. Reverse
has no forced mode, so its Shared operation is correct. Disabling reverse
advanced tuning does not disable its separate Exclusive preference in the first
profile; Exclusive operation there also matches the current contract.

In the first run both reverse Exclusive opens report success without fallback,
and capture advances to 14,391,360 frames. The pipeline renders only its first
480-frame silent priming block, transfers no source audio, and fills its bounded
9,600-frame FIFO. Native submitted frames remain 960, including 480 initial
silent frames. No backend issue is reported. This proves lack of reverse
transfer, not the particular driver or event mechanism responsible. The existing
telemetry does not distinguish missing render notifications from other wake or
render-service conditions sufficiently to assign a root cause.

Runtime inspection explains the unhelpful status: freshness reporting is only
initialized after the first transferred frame. A zero-transfer session with no
backend error can remain Starting indefinitely; the working forward sibling
then makes the aggregate Degraded. This is an unresolved startup-health and
diagnostic gap, not successful duplex validation. Shared fallback was not
exercised: acquisition returned success, and this build has no post-start
mode-recovery policy.

The second run transfers in both directions for approximately 123 seconds,
without FIFO underruns/overruns or continuing capture discontinuities/timestamp
errors. Reverse has one separately classified startup discontinuity. This is
successful mixed Exclusive-forward/Shared-reverse operation for that duration,
not proof of glitch-free long-term operation or audible reverse correctness.

## Next discriminating work

Compare preferred and required Exclusive using one simplex forward route, the
same endpoints and all other fields fixed. Alternate policy across several
equal-duration starts, recording the latency distribution and whether each
measurement precedes or follows any underrun. This removes the reverse-mode
change and failed return stream from the comparison.

Separately reproduce RCP2 Main to VirtA alone in Shared and Exclusive. Investigate
render progress/wake diagnostics and startup failure reporting before claiming
working Exclusive duplex. Avoid silently adding a new post-start fallback or
choosing a production health timeout from this single observation.

This review changes documentation only. No GUI, profile, notes, engine behavior,
or published archive is modified; no fresh build or hardware measurement is
claimed.
