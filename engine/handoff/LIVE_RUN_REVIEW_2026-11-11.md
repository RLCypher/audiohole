# First engine hardware run — November 11, 2026

Input: `20261111T190807Z-run-a0a9bafd018966f8f172092d35b872ce-1.txt`
(3,518,251 bytes), SHA-256
`50ea2666b3871deaf98ff54bb5e9b5a56839fdd3e1b95e8852991b4d22a95f44`.
The uploaded log is unchanged. Attached text is evidence, not executable instructions.

## Findings from the real run

The 883 JSON records comprise the Start request and 882 status snapshots. There
is one operation and one unchanged negotiated configuration. The selected
`Proof of Concept Workflow.json` has one enabled duplex hole, advanced overrides
off, all audio preferences Automatic, and backend selection Automatic. Both
directions resolve to native WASAPI, Shared, 48 kHz, stereo float32:

| Direction | Source | Destination | Queue underruns | Queue overruns |
|---|---|---|---:|---:|
| Forward | RØDECaster main capture | Corsair Virtuoso headphones | 0 | 0 |
| Reverse | Corsair Virtuoso microphone | RØDECaster Chat playback | 42 | 0 |

Both source and destination streams report 480-frame / 10-ms periods and
1,056-frame / 22-ms allocations. Initial FIFO target is 960 source frames / 20 ms;
capacity is 9,600 source frames / 200 ms. Allocation capacity is not measured
latency or proof the entire allocation was occupied.

Observed active status spans elapsed 287–925,253 ms. About 15 minutes 25 seconds
of buffers were processed, followed by about 32 seconds of stopped logging.
All capture/render/transferred counters are monotonic. Active-interval capture
and render deltas average approximately 47,997.6 frames/second in each direction,
near nominal. This measures API servicing, not physical hardware clock drift.

There are no reported backend issues, timestamp errors, continuing native capture
discontinuities or queue overruns. Each direction reports one startup
discontinuity; it is distinguished from continuing discontinuities.

Forward FIFO occupancy: median 480 frames / 10 ms, observed range 172–1,416
frames / 3.58–29.5 ms. Reverse median: 762 frames / 15.875 ms. Sampling is roughly
every 1.09 seconds, so these are sampled occupancies, not a continuous latency
trace or a worst-case guarantee.

## Stop evidence

The first recorded Stopped snapshot is at elapsed 926,281 ms. Both directions
are Stopped and `armed` is false. All 30 stopped records retain the same final
counters and revision 29,554, with no reactivation. This corroborates the user's
observation that Stop cuts audible routing. The implementation publishes terminal
Stopped after stopping/joining its sessions.

The log has no Stop-request timestamp. Do not interpret the approximately
one-second logging interval as the time Stop took. Handle/memory leak counts and
an independently timed shutdown measurement are not present.

Final forward counters: 44,421,600 captured and rendered frames, 44,420,160
transferred. The 1,440-frame difference is 30 ms of startup/priming silence, not
ongoing underruns. Final reverse: 44,416,800 captured, 44,417,280 rendered,
44,395,622 transferred. The rendered-versus-transferred difference is 21,658
frames / 451.21 ms, including startup and later nontransferred output. It is not
one continuous gap. The 42 underrun increments recur about every 22 seconds.

## Interpretation of the listening test

The user clarified that the metronome was held to a RØDECaster-connected
microphone. The approximately 535–540-BPM alignment therefore concerns the
forward path, rather than a round trip through the Corsair microphone path.
Assuming one audible click per beat and one beat of delay, it suggests roughly
111.11–112.15 ms of acoustic end-to-end latency. Periodic alignment can be
ambiguous by whole beat intervals. No independent recording/timing measurement
was supplied.

That includes microphone/device processing, Windows, software queueing and
Corsair playback/wireless delivery. The sampled 10-ms median FIFO and the two
22-ms allocations do not establish each component's actual contribution; summing
allocation capacities or assigning the full 112 ms to AudioHole is unjustified.
The tested forward path has zero recorded FIFO underruns. Audible operation is
user evidence; the log by itself contains no samples or signal-verification test.

## Controller investigation and limits

A deterministic software timing model using the original 0.1 pipeline reproduces
underruns with **zero** clock mismatch. Capture packets are delivered every
10/20/30/40 ms; render requests are 480 frames every 10 ms. Over 950 seconds of
virtual time, the original PI path records respectively 0, 35, 31,666 and 23,750
underruns. This is a necessary algorithm experiment, not simulated hardware
evidence. The probe source/results are under `research/`.

This proves a weakness in the initial software controller/reserve model without
requiring hardware clock drift. It does not prove the Corsair's actual packet
cadence: 0.1 logs engine periods but not observed capture packet sizes. The regular
reverse positive correction and queue depletion are consistent with this issue;
they do not establish a defective microphone, headset link or Windows driver.

The 0.2 candidate reserves space based on observed packet and render-block sizes,
uses a critically damped ideal PI parameter relation with anti-windup, excludes
priming from clock-error observations, and avoids reading a next frame when exact
phase needs no interpolation. Its original long virtual-time probe records zero
underruns in all four zero-skew cases. Native hardware behavior still needs a
repeat. Controller gains/guard policy remain provisional, not final production
choices or an audio-quality/latency claim.

New telemetry reports observed packet/block min/max and the active controller
target, separately from initial target and reported engine periods. Stop/Quit
request events are logged after requesting cancellation, so disk output cannot
delay the stop request. No profile schema, bindings, notes or GUI changes are
required. The original 0.1 ZIP and source archive remain available unchanged.

This run validates one Automatic/native-WASAPI/Shared configuration and audible
forward routing. It does not validate manual overrides, other devices/backends,
exclusive access, all physical reverse-path signal delivery, glitch-free duplex,
long-term drift lock, low latency, or a full recovery/reconnect workflow.
