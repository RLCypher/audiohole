# Validation 0.5.1 — browser tapping estimate to wireless headphones

The user reports approximately 40 ms using
https://nullvoxpopuli.github.io/latency-tester/ with a human, keyboard and wireless
headset in the measurement loop. The page instructs users to tap on the beat and
reports a trailing average of ten beats. This is a human synchronization estimate,
not an objective instrumented one-way measurement. Keyboard/input delivery and
human anticipation or lag can bias it in opposite directions. The extra elements
therefore do not establish 40 ms as an upper bound on audio latency. No assumed
human, keyboard, wireless or earlier phone-reference delay is subtracted.

Input: `20261112T233216Z-run-e0bee4491481aac1d5855cb7a6c10eb4-2.txt`,
2,389,968 bytes, SHA-256
`9305b664e15cb20235f66a38e7ba6f19c6fc1c0ff58075aaba9de3098c87b286`.
All 269 concatenated JSON objects parse strictly without duplicate keys,
non-finite constants or trailing corruption. There is one Start, 221 Degraded
snapshots, one Stop and 46 Stopped snapshots. Stop is requested at 240,174 ms;
the remaining log through 289,418 ms is idle, with unchanged stopped counters
and routing disarmed.

## Actual routing

Build 0.5.1, baseline preset, individual profile schema 3, one enabled duplex hole.
Both directions select native WASAPI through `routing: automatic`; advanced
tuning is disabled and all manual preferences are null. Clock metadata remains
Automatic and unapplied.

The functioning headphone route is the JSON reverse direction:
RCP2 Main capture to Corsair Virtuoso Headphone playback. Both actually open
Shared, 48 kHz, stereo float32, 480-frame / 10-ms periods and 1,056-frame / 22-ms
client allocations. Neither requests Exclusive. Destination lead is 1,056 frames;
the software FIFO active target is 962 frames, capacity 9,600 frames. Internal
processing is float32 with linear adaptive SRC and occupancy PI synchronization.
These are buffering/processing observations, not measured sample transit latency.

The log does not identify the browser's Windows playback selection or prove the
browser test signal traversed RCP2 and AudioHole. Interpretation as a complete
game/browser-to-mixer-to-headphones test assumes the browser was deliberately
sent to the intended RCP2 playback endpoint. A routed-versus-direct playback
comparison must retain this distinction.

## Headphone-route health

This path remains Running until Stop and transfers 11,516,920 output frames.
Final pipeline rendered count is 11,520,960; native submission is 11,522,016,
equal to the pipeline count plus the 1,056-frame initial silent preroll. Startup
prefix is zero. Progress age is zero throughout active snapshots. There are
zero FIFO overruns, continuing capture discontinuities, timestamp errors or
backend issues, and one separately classified startup discontinuity.

There are five FIFO underruns, first observed at elapsed 19,856, 48,208, 90,584,
157,935 and 213,332 ms. The preserved event contexts show 480-frame output
requests with 431, 413, 435, 466 and 455 frames produced, respectively. The
partial blocks therefore contain 49, 67, 45, 14 and 25 silent frames, excluding
subsequent re-priming silence. These are not measurements of complete audible
glitch duration. Every event has 576 frames / 12 ms of reported native render
padding before submission; recorded last capture-drain gaps are approximately
10 ms. No diagnostic contexts are dropped. This repeats the unresolved FIFO
starvation behavior; it does not establish a gross capture stall or physical
clock drift as its cause.

## Separate Exclusive VirtA path stalls again

The JSON forward path requests Corsair Virtuoso Mic to RCP2 VirtA with Exclusive
preferred. Both Exclusive opens succeed on the first attempt with no Shared
fallback. Source is mono packed 24-bit PCM at 48 kHz; destination is stereo
24-valid-bit PCM in 32-bit containers at 48 kHz. Both periods and allocations
are 480 frames.

Capture advances to 11,523,360 frames, but transferred stays zero. Pipeline
rendered stays 480 and native submission 960, including initial silence. The
queue stays full at 9,600 frames and accumulates 23,987 overruns. No backend
error is reported; the path remains Starting, making overall status Degraded.
This is the same progress signature as the earlier Exclusive RCP2 Main to
VirtA stall, now with a different capture device. VirtA Exclusive playback is
a common factor worth isolating; the logs alone cannot assign driver, event
handling, format or hardware cause. Lack of audible input does not explain
the growing captured-frame count without downstream consumption.

See [the previous access review](LIVE_RUN_REVIEW_2026-11-12_V051_ACCESS.md) for
the startup-health reporting gap. This run does not demonstrate successful
duplex transfer, Shared fallback, or forced-mode failure handling.

## Interpretation and next measurement

A confirmed approximately 40-ms complete playback path would be a reasonable
practical result for listening and many gaming uses, while remaining noticeable
or unsuitable for some live self-monitoring and timing-sensitive uses. Acceptance
depends on the intended task; the present tapping result does not certify the
number or isolate AudioHole's processing overhead.

For a repeatable comparison, alternate the browser between direct headphone
output and the RCP2-to-AudioHole path, keeping the keyboard, headset and test
procedure unchanged. Repeated tap averages remain approximate; an instrumented
reference/return recording or a controlled acoustic correlation test is needed
for objective latency. Retain the earlier acoustic measurements as separate
test methods with different start/end boundaries rather than interpreting
40 ms versus 84–116 ms as an established engine improvement.

This review changes documentation only; no GUI, profiles, notes, engine code
or published packages are modified. No new build or physical measurement by
the build agent is claimed.
