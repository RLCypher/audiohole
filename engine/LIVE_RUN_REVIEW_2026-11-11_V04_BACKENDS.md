# Validation 0.4 — six backend/preset hardware runs

Six attachments contain 652 strictly parsed JSON records, 2,355,338 bytes total,
without duplicate keys or trailing corruption. All builds identify validation
0.4. All profiles select the same RCP2 Main capture and Front Audio Port playback,
same audio preferences and one enabled simplex hole with advanced overrides off.
Profile/hole IDs and backend selectors differ. The Wasapi-named profile still
selects Automatic; actual backend is correctly native WASAPI in both runs.

Every stream reports Shared / 48 kHz / stereo float32. All six finish Stopped,
disarmed, with unchanged path counters across subsequent stopped snapshots.
No backend issue or FIFO overrun is recorded. Miniaudio's native packet-flag
and timing counters are unavailable, not zero or evidence of absent native faults.

## Reported latency paired with measured FIFO faults

Latency figures are the user's approximate LagMeter averages, not measured
engine component delays. No raw per-echo series, repeat count or spread accompanies
them. A common hypothetical phone offset would not change these differences.

| Backend | Preset | User-reported ms | Start to Stop request | FIFO underruns |
|---|---|---:|---:|---:|
| Native WASAPI | Standard | 156 | 96.873 sec | 0 |
| Miniaudio | Standard | 163 | 64.668 sec | 0 |
| Experimental | Standard | 152 | 102.269 sec | 2 |
| Native WASAPI | Low | 148 | 152.869 sec | 0 |
| Miniaudio | Low-labelled | 155 | 76.925 sec | 2 |
| Experimental | Low | 148 | 138.108 sec | 0 |

These approximately 1–2.5-minute sessions are initial comparisons, not the
requested longer stability runs or final backend acceptance. Earlier intermittent
behavior and the shorter/different exposures prevent a permanent winner claim.

## Startup fix verified on native I/O

| Final native accounting/property | WASAPI Std | Experimental Std | WASAPI Low | Experimental Low |
|---|---:|---:|---:|---:|
| Capture / render period, frames | 480 / 480 | 480 / 480 | 336 / 480 | 336 / 480 |
| Active FIFO target | 962 | 962 | 960 | 960 |
| Largest actual native write | 1056 | 1056 | 528 | 528 |
| Largest pipeline render request | 480 | 480 | 480 | 480 |
| Initial native silence | 1056 | 1056 | 528 | 528 |
| Additional startup prefix silence | 576 | 576 | 48 | 96 |
| Pipeline rendered frames | 4639680 | 4898880 | 7327680 | 6620160 |
| Total native submitted frames | 4641312 | 4900512 | 7328256 | 6620784 |

In all four, `native_submitted = initial_silence + startup_prefix + rendered`.
The 1,056-frame initial refill no longer raises the controller target to the
previous 1,538 frames / 32.0417 ms. Standard remains at 962 / 20.0417 ms,
exactly 576 frames / 12 ms below that previous target. Low remains at 960 / 20 ms.
All Low minimum-period requests succeed without a fallback. Source allocation
is 738 frames / 15.375 ms in Low and 1,056 / 22 ms in Standard. Destination
allocation remains 1,056 / 22 ms; submitted lead is 528 / 11 ms Low and
1,056 / 22 ms Standard. Sampled pre-write padding is consistently 48 / 1 ms
and 576 / 12 ms respectively.

Experimental Low counts two empty-buffer observations during startup and 96
prefix frames, both already present in its first Running snapshot. Its pipeline
has no underrun. A zero-padding observation is not itself a hardware glitch
measurement. No native run has unexplained startup audio in its accounting.

The user's Standard WASAPI change from the previous approximately 168 ms to
156 ms is consistent with this buffering correction, but different run durations,
sampling phase and uncalibrated acoustic readings prevent an exact causal delay
allocation. The logs independently verify the policy/accounting change.

## Miniaudio is a useful control

Its two logs have IDENTICAL source/destination StreamConfiguration records:
480-frame / 10-ms periods, 1,056-frame / 22-ms allocations, Shared 48-kHz stereo
float32, no explicit buffer request. Active FIFO target is 962 in both. The Low
flag does not change this adapter's configuration or controller tuning.

After 30 seconds, sampled FIFO median is 922 frames / 19.208 ms in Standard,
versus 443 / 9.229 ms in the Low-labelled run, approximately one period apart.
The latter also starves twice. Thus an approximately 8-ms reported difference
does not demonstrate a Miniaudio Low-preset optimization. Different startup/
callback phase, queue/controller state and measurement conditions remain possible
causes. Sampled FIFO medians do not establish per-sample transit delay or isolate
which cause applied. Native render padding is unavailable for this adapter.

## The new event records locate real starvation

No event contexts are dropped. Each failing run reports both sequence 1 and 2.
Observed elapsed times below include the host's snapshot delay, not exact event
onset. The generic rendered-frame index and native call QPC provide finer context.

| Run / sequence | Observed elapsed | Source frames available | Output requested | Output produced | Partial-block silence |
|---|---:|---:|---:|---:|---:|
| Experimental Std / 1 | 48.860 sec | 402 | 480 | 401 | 79 frames / 1.646 ms |
| Experimental Std / 2 | 66.257 sec | 477 | 480 | 476 | 4 / 0.083 ms |
| Miniaudio Low-labelled / 1 | 18.502 sec | 443 | 480 | 442 | 38 / 0.792 ms |
| Miniaudio Low-labelled / 2 | 58.797 sec | 432 | 480 | 431 | 49 / 1.021 ms |

The partial-block column excludes any subsequent silent re-priming callbacks;
it is not a complete glitch-duration estimate. Fractional interpolation needs
the next published source sample, explaining the one-frame difference between
available and produced in these events.

Experimental's native padding was 576 frames / 12 ms at BOTH failures. Its last
capture drain gaps were 9.999 and 9.986 ms; maximum gap across the session was
10.336 ms. Capture drains never exceeded one 480-frame packet, and continuing
discontinuity/timestamp-error counters remain zero. Last capture packet QPC to
render entry is 10.2963 and 10.2023 ms; subtracting the corresponding receipt
ages gives approximately 10.1513 and 10.0553 ms since that capture receipt.
These are native scheduling observations, not physical microphone timing.

The application FIFO is therefore short at render-call entry even while Windows
has queued client audio. Gross 20-ms capture bursts/stalls are not recorded in
these failing native events. At Experimental's first failure the correction
command is already negative (-78 ppm); at the second it is positive (+277 ppm).
Miniaudio's two commands are +128 and -65 ppm. None is a physical clock-drift
measurement, and the failure is not simply explained by the sign at that instant.

## Engineering implications

Native WASAPI Low remains a sensible provisional lead: tied at approximately
148 ms with Experimental Low, with zero faults in both native-WASAPI sessions
and stronger native observability than the current Miniaudio adapter. Experimental
Standard's 4-ms lower reported mean comes with real starvation events; this is
not established backend superiority or a new production selection.

The next investigation should address render/capture phase and FIFO depletion.
One native possibility is using known queued playback headroom to await an
imminent capture packet within its deadline. It would require bounded pending
render handling on capture wakes and an explicit fallback when headroom expires;
no blocking audio-thread wait or silent suppression of underrun accounting is
acceptable. The current Low pre-write headroom is only 1 ms, so a Standard-only
12-ms opportunity cannot be assumed generally. Miniaudio callbacks still require
their requested output immediately and need separate synchronization analysis.

Per-event context narrows the failure boundary; a full pre-event timing trace
and/or a validated timing model would be needed to establish the mechanism before
another controller retune or reduction of safety margins. The earlier simple
phase experiment did not reproduce the hardware failures. No new engine change,
new build, GUI change, or acoustic latency reduction is claimed by this review.

## Input hashes

| Filename UTC prefix / run prefix | Bytes | SHA-256 |
|---|---:|---|
| `20261111T225441Z` / `9337b588` | 371856 | `a7f1f0454faa004f7821d4f92028d873d6c9a0a0cb8777ca15594ff59c0039c3` |
| `20261111T225634Z` / `b0d501d3` | 246653 | `acab1bd01a044044c2220c5daedb3c7044c13e31b39d741cf2661d5199470b92` |
| `20261111T225748Z` / `e783976a` | 387920 | `0e06778ae7a050fd39727bf26fbe70046f67cd2cfe09347adecbe0f0283ac94a` |
| `20261111T225949Z` / `3f16fd45` | 529444 | `e76e3ce57e0cf63e5b1fd79455700ad1147615be64de0ab4b965362edea7d58b` |
| `20261111T230232Z` / `21d9486a` | 353879 | `5e908aa32b569df8616895131c826775435e5e6fc3620421c39467e3f92d3178` |
| `20261111T230405Z` / `57188ab4` | 465586 | `698a6c119e155a2cc64b531e3bd33f904cf81851b45188e6604e745393058683` |
