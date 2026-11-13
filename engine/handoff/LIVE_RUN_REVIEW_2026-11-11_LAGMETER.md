# Third hardware run — LagMeter observation

Input: `20261111T203249Z-run-e3711e477e0616600ef0bbb67829b9ae-1.txt`,
3,231,802 bytes, SHA-256
`617beb93fbe8593f3955ddc388960c75a246b5016c9f002a9bf57e05a0a8e576`.
The attachment is unchanged. All 738 concatenated JSON objects parse strictly,
without duplicate keys or trailing corruption.

## Routing and stability

The build identifies itself as validation 0.2. The complete embedded profile
record is identical to the previous run: RCP2 Main capture to Corsair Virtuoso
headphones, with Corsair Virtuoso microphone to RCP2 Chat in reverse. All audio
preferences are Automatic and advanced overrides are disabled.

Both paths again use native WASAPI, Shared mode, 48 kHz, stereo float32. Each
endpoint reports a 480-frame / 10-ms period and a 1,056-frame / 22-ms allocation.
These actual endpoint settings are unchanged from the previous run.

There are 735 snapshots: one initially Degraded while the reverse path starts,
678 Running, and 56 Stopped. The first forward Running observation is at 192 ms;
both directions are Running by the next snapshot at 1,282 ms. The last active
snapshot is at 737,391 ms, approximately 12 minutes 17 seconds after Start.
The first Stop request is at 737,991 ms, approximately 12 minutes 18 seconds.

| Observation | Forward | Reverse |
|---|---:|---:|
| FIFO underruns | 0 | 0 |
| FIFO overruns | 0 | 0 |
| Continuing capture discontinuities | 0 | 0 |
| Timestamp errors | 0 | 0 |
| Startup discontinuities | 1 | 1 |
| Median FIFO occupancy while Running | 482 frames / 10.0417 ms | 482 frames / 10.0417 ms |
| Minimum / maximum FIFO occupancy | 414 / 1,167 frames | 414 / 962 frames |
| Active controller target | 962 frames / 20.0417 ms | 962 frames / 20.0417 ms |
| FIFO capacity | 9,600 frames / 200 ms | 9,600 frames / 200 ms |
| Final captured frames | 35,415,360 | 35,410,560 |
| Final rendered frames | 35,415,840 | 35,411,040 |
| Final transferred frames | 35,414,400 | 35,409,600 |

All observed individual capture packets and render requests are 480 frames.
No backend issues are listed. Frame and fault counters are monotonic. Both
directions have near-48-kHz client frame progression. Each final rendered minus
transferred count is 1,440 frames / 30 ms, consistent with startup priming; it
is not a measured end-to-end delay. No ongoing FIFO starvation is recorded.

Both directions' median requested correction is zero ppm, including after the
first minute. This is a resampler command, not a measured physical clock rate;
`measured_clock_drift_ppm` remains null. The sampled FIFO depth is also not the
full route latency, and unused FIFO capacity does not itself add 200 ms.

The previous run had 49 reverse underruns, all in its first approximately
14 minutes 43 seconds, including many before this run's duration. This run's
zero reverse underruns under identical reported settings establishes that the
failure is intermittent between runs. It does not establish a permanent fix or
identify its cause. Capture arrival cadence, delivery bursts, render phase, and
valid native packet timing remain useful next diagnostic targets.

## Stop evidence

The first Stopped snapshot is at 738,447 ms, armed false, both paths Stopped.
All 56 Stopped snapshots retain identical path counters and revision 23,561,
through elapsed 797,976 ms. A second Stop request at 778,345 ms leaves the
state and counters unchanged. This supports correct, idempotent shutdown in
the observed run. The 456-ms gap from the first Stop request to the first
Stopped snapshot includes sampling delay and is not an exact shutdown latency.

## External latency observation

The user reports a phone app named LagMeter indicating approximately 116 ms.
Record this as an external, app-reported observation. The user subsequently
clarified the acoustic setup:

- The RØDECaster-connected microphone was positioned at the phone's top speaker.
- The phone's bottom speaker was covered with gaffer's tape to reduce direct
  sound reaching the phone microphone.
- The phone microphone rested against the Corsair headphone earcup.
- Phone output volume and app gain were adjusted to avoid detection of the
  phone's direct sound. The user reports that direct pickup would read about
  10 ms, rather than the approximately 116-ms routed return.

The intended measured path is phone output, acoustic coupling to the RØDECaster
microphone, mixer/capture processing, AudioHole routing, headphone output, then
acoustic coupling into the phone microphone. Phone input/output delay may also
contribute depending on app timestamp references and compensation. The exact
app identity, timing algorithm, and calibration remain unverified.

This setup supports an instrumented acoustic end-to-end measurement, provided
the app automatically times the intended returned signal. It avoids the human
beat alignment and keypress timing of the earlier methods. The approximately
10-ms direct-pickup comparison is useful evidence against the short leakage
path dominating the 116-ms result; it is not a calibration of engine latency.
An additional useful control is to stop AudioHole while leaving the phone's
test signal running: the approximately 116-ms return should disappear. That
control has been suggested, not reported as performed.

Numerically it is near the earlier approximately 111–127-ms metronome estimates,
but agreement does not establish common measurement boundaries or calibration.
Do not pool it with the browser's approximately 96-ms result, subtract the
results to infer AudioHole overhead, or attribute all 116 ms to the engine.
Automatic returned-signal timing and calibration have not been independently
verified for this app.

The engine log explicitly retains `measured_end_to_end_latency_ms: null` and
`physical_signal_verified: false`. It independently supports the buffering and
counter observations above, not confirmation of the app's acoustic reading.

This review changes no engine implementation, GUI, profile, or packaged build.
