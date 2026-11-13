# Validation 0.3 — baseline versus Low on real hardware

The user reports approximately 116 ms in Low, a similar central reading in
baseline, and apparently lower variability in Low. Baseline examples include
approximately 113 ms followed by approximately 124 ms. These are user-reported
LagMeter observations; no per-echo measurement series or calibrated component
latency is included in the engine logs.

The user also confirmed the measurement-path control: LagMeter stops reporting
latency when AudioHole routing is stopped while the phone signal continues.
This supports detection of a return dependent on AudioHole. It does not isolate
phone, mixer, Windows, engine, or wireless/headphone contributions.

## Inputs and integrity

Both logs parse strictly as concatenated JSON without duplicate keys or trailing
corruption. Their entire embedded profile records are identical. The selected
route is RCP2 Main capture to Corsair Virtuoso headphones, and Corsair microphone
to RCP2 Chat in reverse. Both use native WASAPI / Shared / 48 kHz / stereo float32.

| Input | Low | Baseline |
|---|---|---|
| Filename prefix | `20261111T210743Z-run-679622cf` | `20261111T211202Z-run-1e5ef9ea` |
| Bytes | 1,396,859 | 1,151,836 |
| JSON records | 227 | 188 |
| Recorded preset | `low` | `baseline` |
| Time from Start to Stop request | 237.897 sec | 185.982 sec |
| Running snapshots | 218 | 171 |
| Stopped snapshots | 6 | 14 |

Low SHA-256:
`d48fb74f00b106b24249d6cf101d6801eb061f80eb35da370b5577590572e32a`.
Baseline SHA-256:
`21338917bdab1c377d40f932aa852eff13507b4ba7f3fd6a36dd76ed667b7d0e`.

## The requested changes were actually applied

| Native client property | Baseline | Low |
|---|---:|---:|
| RCP2 Main capture period | 480 frames / 10 ms | 336 frames / 7 ms |
| Corsair headphone playback period | 480 / 10 ms | 480 / 10 ms |
| Corsair mic capture period | 480 / 10 ms | 480 / 10 ms |
| RCP2 Chat playback period | 480 / 10 ms | 336 / 7 ms |
| Forward render-ahead target | 1,056 / 22 ms | 528 / 11 ms |
| Reverse render-ahead target | 1,056 / 22 ms | 384 / 8 ms |
| Forward render padding before writes, steady samples | 576 / 12 ms | 48 / 1 ms |
| Reverse render padding before writes, steady samples | 576 / 12 ms | 48 / 1 ms |

All Low streams report `period_selection: minimum`, without a fallback. Corsair
advertises minimum=default=maximum=480 frames in this Shared configuration; Low
cannot obtain a smaller Client3 period from those advertised bounds. This is
not proof of an absolute hardware minimum across all access modes/formats.

The RCP2 endpoint allocations fall from 1,056 frames / 22 ms to 738 / 15.375 ms.
Corsair allocations remain 1,056 frames / 22 ms. Allocation, period, and submitted
audio are separate quantities. The post-submit estimate equals the render-ahead
target in steady samples, confirming the changed submission policy; it is not
a separately measured physical playback timestamp.

The source FIFO initial target remains 960 frames / 20 ms in BOTH modes because
one endpoint in each direction still reports a 10-ms period. The Low 5-ms floor
is therefore inactive. Active targets are 960 in Low and 962 / 20.0417 ms in
baseline, not 5 ms versus 20 ms of actual queued audio.

After excluding the first 30 seconds, sampled forward FIFO occupancy has median
672 frames / 14 ms in Low and 601.5 / 12.53125 ms in baseline. Reverse medians
are 796 / 16.5833 ms and 598.5 / 12.46875 ms, respectively. Low does not have a
lower sampled FIFO median in these traces. Native period/phase changes affect
when queue depth is observed. Do not add independently sampled medians and
render targets to declare a measured end-to-end latency saving.

## Stability and timing

| Counter at Stop | Low forward | Low reverse | Baseline forward | Baseline reverse |
|---|---:|---:|---:|---:|
| FIFO underruns | 0 | 0 | 10 | 10 |
| FIFO overruns | 0 | 0 | 0 | 0 |
| Continuing capture discontinuities | 0 | 0 | 0 | 0 |
| Capture timestamp errors | 0 | 0 | 0 | 0 |
| Zero-padding render wakes | 0 | 0 | 0 | 0 |
| Largest capture drain | 336 frames | 480 | 480 | 480 |
| Longest capture delivery gap | 7.898 ms | 10.650 | 10.877 | 10.679 |

Each path has one startup discontinuity and no backend issue. Individual capture
packets match the selected capture periods; render requests match playback
periods. There is no observed 20-ms packet, combined multi-packet drain or long
delivery gap to explain baseline's periodic FIFO starvation in these runs.

Baseline forward underruns first appear at 27.938 sec, then approximately every
14–20 seconds through the tenth reflected in the first Stopped snapshot.
Reverse first appears at 19.232 sec, then approximately every 17–19 seconds
through 183.466 sec. Zero native padding is not required for a pipeline underrun:
the pipeline can submit silence while the Windows client remains filled.

After 30 sec, baseline's median resampler command is approximately +478 ppm
forward / +455 ppm reverse, versus +0.014 / +2.459 ppm in Low. These are control
commands, NOT measurements of device clock mismatch. Client frame counters
progress near nominal rate. Matching nominal formats and short-run counters
do not establish a common hardware clock.

Capture packet timestamp age medians are approximately 0.15 ms for RCP2 and
10.13 ms for Corsair in both modes. These are native QPC timestamp-to-receipt
ages, with driver-specific timestamp boundaries; do not equate them to physical
microphone delay or use them to calibrate the whole acoustic loop.

Both logs end Stopped and disarmed. All stopped path counters remain unchanged,
including after Quit. The runs support clean shutdown and better FIFO stability
for this Low comparison, not a permanent fix or long-run acceptance. The earlier
0.2 baseline run without underruns remains evidence of intermittency.

## Additional software check and conclusion

`research/phase_receipt_probe.cpp` exercises six deterministic receipt schedules
over 180 virtual seconds each: fixed positive/negative capture phase, fast/slow
boundary-crossing jitter, biased jitter, and the 336/480-frame mixed period.
Both virtual sample clocks are exactly 48 kHz. All six produced zero underruns
and overruns. The experiment did NOT reproduce the hardware baseline failure;
no controller fix or hardware cause follows from it. No synthetic device is
introduced into the validation application.

The defensible result is: native timing/submission changes took effect; Low
was free of recorded FIFO failures in this approximately four-minute run;
an end-to-end latency reduction was not demonstrated by the reported readings.
The control confirms an AudioHole-dependent returned signal, but no component
has been established as the dominant portion of the approximately 116 ms.

Further work should capture bounded per-underrun context and correlate source
frame provenance with render submission timing, before another queue/controller
retune. Separately, a wired/headphone comparison could help isolate transport
delay if the hardware supports a suitably controlled route. No particular
alternative mode/backend is established faster by these runs.

This review and software probe change no engine implementation, GUI, profile,
device notes, or previously packaged executable/archive. No new build is claimed.
