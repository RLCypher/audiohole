# Second hardware run — validation 0.2

Input: `20261111T194817Z-run-9ba5ef8063f36b83b67780ccf6d52300-1.txt`,
6,601,953 bytes, SHA-256
`bb1d692bbe333b5d853d5ff9faa06854b811742e5d715d8bf3a18a946fe7c939`.
The original attachment is unchanged. Its 1,502 concatenated JSON objects parse
strictly without duplicate keys or trailing corruption.

The file contains one Start and 1,501 Running snapshots. Last elapsed time is
1,632,546 ms (27 minutes 12.546 seconds). It ends armed and Running, with no
Stop/Quit event or Stopped snapshot. This is a live-run excerpt, not evidence of
a shutdown failure or another confirmation of clean shutdown.

## Matched configuration and queue comparison

The selected profile is identical in routing intent to the first run: one enabled
duplex hole, advanced overrides off, Automatic backend and all audio settings
Automatic. Forward is RØDECaster main capture to Corsair Virtuoso headphones;
reverse is Corsair microphone to RØDECaster Chat playback.

Both directions use native WASAPI / Shared / 48 kHz / stereo float32. Each
endpoint reports a 480-frame / 10-ms engine period and a 1,056-frame / 22-ms
allocation, unchanged from 0.1. All observed individual capture packets and
render requests are 480 frames. No packet/block min/max exceeds that size.

| Observation | 0.1 | 0.2 |
|---|---:|---:|
| Approximate recorded processing duration | 15 min 25 sec | 27 min 13 sec |
| Forward queue underruns | 0 | 0 |
| Reverse queue underruns | 42 | 49 |
| Queue overruns, either direction | 0 | 0 |
| Continuing capture discontinuities, either direction | 0 | 0 |
| Timestamp errors, either direction | 0 | 0 |
| Forward median FIFO occupancy | 480 frames / 10 ms | 482 frames / 10.0417 ms |
| Forward initial/active target | 960 frames / 20 ms | 960 initial, 962 active / 20.0417 ms |

Each endpoint direction again reports one startup discontinuity, separately from
continuing discontinuities. No backend issues are listed. All client counters
are monotonic and service progress is near 48,000 frames/second. Positive frame
counts and Running status do not establish glitch-free signal delivery.

The forward median changes by TWO samples (about 0.0417 ms), with no forward
underruns and unchanged endpoint allocations. This trace does not support the
earlier tentative explanation that 0.2's packet guard added about 15 ms to that
path. It does not measure all device/DSP/OS/wireless buffering or prove total
latency unchanged. The user-observed 472-BPM alignment corresponds to about
127.12 ms only under the previously stated one-beat assumption.

## The reverse issue persists

All 49 reverse underrun increments occur by elapsed 882,902 ms (14 min 42.902
sec), approximately every 17–19 seconds. No further increment appears during
the subsequent approximately 12 min 29.6 sec. The early pattern is more frequent
than 0.1's approximately 22-sec recurrence; the longer overall run must not be
used to imply a direct improvement by comparing totals or aggregate rates alone.

Before elapsed 900 seconds, reverse median correction is +480.206 ppm; after
that point it is -0.79 ppm, with the underrun total fixed at 49. These are
RESAMPLER COMMANDS, not measured headset clock drift. Captured and rendered
client-frame deltas are nearly equal. Actual timing, event phase and physical
clock relationships are not established by these sparse samples.

The 0.2 software regression fixed a real zero-skew packet-cadence case. This
hardware trace shows that it did not resolve the entire return-path issue.
Individual 480-frame packets do not confirm the previous larger-packet
hypothesis. They also do not rule out several 480-frame packets being delivered
in a burst after a delay: packet SIZE is different from receipt cadence and
total capture drained per wake. Those quantities are not logged yet.

The next useful instrumentation is capture delivery intervals/burst totals,
queue depth around render demand and underrun events, and valid native
packet/QPC timing. Do not retune gains or enlarge every buffer based only on the
aggregate counters. No new controller or hardware-fix claim is made here.

## Browser result: approximately 96 ms

The user confirms browser audio went to the RØDECaster, then through AudioHole
to the Corsair headphones. Therefore 96 ms includes an AudioHole path; it is not
a direct-headphone baseline to subtract from the metronome result.

Inspected public code:
https://github.com/NullVoxPopuli/latency-tester/blob/gh-pages/src/main.js
Git blob: `198fa7449890df2afe1646bf908262d26a336953`.

It starts a Web Audio buffer, records a JavaScript Date timestamp, then records
click/Space-key timestamps and averages tap-minus-play differences over a
sliding sequence. Space keydown calls the button click handler. No microphone
recording, electrical loopback, returned-signal detection or calibrated keyboard
delay measurement is present. The source uses wall-clock timestamps rather than
actual physical output timestamps. User anticipation/lateness and input/browser
event timing influence the number even when the tapping feels accurate.

Its source enters the mixer through the browser playback path; the metronome
enters through a physical microphone. These are different input paths and timing
references. Treat 96 ms and the approximately 127-ms perceptual alignment as
separate user observations, not precision measurements of the same boundary.
In particular, their difference is not a measured 31-ms AudioHole contribution.

This review changes no engine implementation, GUI, profile, bindings or archives.
The existing 0.2 package is preserved for controlled comparison and diagnosis.
