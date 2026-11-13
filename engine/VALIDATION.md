# Validation record — engine prototype 0.6

This build adds the documented pre-transfer startup watchdog/Shared retry,
native event fairness, and bounded capture-headroom wait. See
[the repair and hardware guide](STARTUP_PROGRESS_TEST.md). The GUI, profile schema,
explicit requirements, device bindings, notes and saved files remain unchanged.

Current verification:

- GCC 14 portable build and all four CTest programs pass: profile/contract,
  processing/lifecycle, access policy, and startup progress.
- All four also pass with AddressSanitizer and UndefinedBehaviorSanitizer
  (`-fsanitize=address,undefined -fno-omit-frame-pointer -O1`).
- Windows x64 Release cross-compilation succeeds, including the validation host,
  all four test executables, and the compiled-in Miniaudio adapter. Windows tests
  are built but not executed on this Linux build host.
- Focused cases cover successful startup fallback; preserving forced capture
  during a preferred-destination retry; strict-mode failure; failed/stalled Shared
  retry without loops; native Start refusal; cleanup-before-reopen; Stop during
  retry cleanup; and error-only observations after earlier valid progress.
- Numerical PCM regression reproduces a short output block with insufficient
  queued source frames, and transfers a complete block when the next capture
  packet arrives before rendering. Missing capture still counts starvation.
  This is not a Windows scheduling, wireless device or physical-clock simulation.
- Both the corrected supplied v3 example and the latest supplied run's profile
  pass the portable CLI check without opening any devices.
- Formatting verification passes for the modified C++ sources/headers.

The Windows native timer/wait calls and actual real-device mode fallback cannot
be executed here. Passing software checks does not establish that VirtA now
works in forced Exclusive, that the capture waits eliminate hardware glitches,
or that latency improved. The new executable remains unsigned. The original
0.1 through 0.5.1 archives are preserved.

## Earlier 0.5.1 rebuild evidence


Version 0.5.1 is a clean Windows x64 Release rebuild of 0.5 with release metadata
updated. No routing, schema or access-policy implementation changes accompany
the rebuild. The portable/sanitizer evidence below was established for 0.5 and
is not represented as a fresh test run for this metadata-only rebuild. Windows
cross-compilation and package-integrity checks verify the new artifact. The
executable is unsigned; Windows Smart App Control acceptance is untested.

Subsequent real 0.5.1 hardware logs are reviewed in
[the preferred/required Exclusive comparison](LIVE_RUN_REVIEW_2026-11-12_V051_ACCESS.md).
Both measured forward routes successfully open native Exclusive with identical
formats, periods and allocations. The preferred run does not fall back; its
return route opens Exclusive but transfers zero frames and remains Starting
through five minutes, with 29,962 FIFO overruns. The other run forces only the
forward direction Exclusive and successfully runs its reverse direction Shared
for approximately 123 seconds. These results verify successful preferred
acquisition and mixed-mode routing on this hardware, but expose an unresolved
Exclusive return startup/progress problem and diagnostic gap. They do not verify
Shared fallback, forced refusal, or establish a latency advantage for requiring
Exclusive over successfully preferring it. The earlier software/build evidence
and its limitations remain unchanged.

A later [wireless-headphone/browser run](LIVE_RUN_REVIEW_2026-11-12_V051_BROWSER.md)
records four minutes of Shared RCP2 Main to Corsair playback with five FIFO
underruns, plus the same zero-transfer Exclusive VirtA playback signature with a
different capture device. The user's approximately 40-ms browser tapping result
is a human timing estimate, not an instrumented one-way latency measurement.

The module remains isolated under `engine/` in a checkout based on
`fda20900e8954993e267e2ca1d6d7be8a19840dc`. No GUI, model, controller, presentation
or parent profile-builder files are modified. The engine's typed/profile parser
implements the owner's schema-v3 update; its serialized contract is documented
in [the current handoff](PROFILE_V3_HANDOFF.md).

Build-time verification (before packaging):

- GCC 14 portable build; all three CTest programs pass.
- All three also pass with AddressSanitizer and UndefinedBehaviorSanitizer.
- Schema tests cover direction-owned flags, independent exclusive preference,
  endpoint/common inheritance, clock metadata, v1/v2 loading compatibility,
  rejection of v7 aggregate routing input, and the corrected owner-supplied v3
  example. Original JSON, notes, profile IDs and existing source files are not
  rewritten by validation or planning.
- Access tests cover preferred success/fallback, forced-mode precedence,
  unchanged explicit rate/format/buffer requirements, cleanup before retry,
  cancellation around API results, and auditable preflight capture conflicts.
  Lifecycle tests verify Failed for forced Exclusive refusal and Degraded when
  a healthy sibling continues. All acquisition callbacks are software fixtures;
  none is installed or exposed as a fake production device.
- The processing regression covers both controllers with 10/20/30/40-ms capture
  packets and 10-ms rendering over 120 seconds of virtual time per case. Both
  virtual clocks are exactly 48 kHz; no underruns/overruns occur. This is software
  algorithm evidence, not simulated hardware evidence.
- The separate 950-second virtual-time PI experiment reproduces the original
  weakness and demonstrates zero underruns across those four cadences in the
  candidate. Source and before/after results are under `research/`.
- The low-latency policy tests cover legal period rounding, invalid bounds,
  actual-allocation write extents, explicit-buffer/Exclusive/unknown-period
  preservation, and queue reserves. Both controllers pass 30 virtual seconds
  per case with 128/240/480-frame render periods and capture batches of one,
  two or four periods. These are necessary software tests, not fake devices
  included in the application or a Windows scheduling model.
- The new startup regression exercises both controllers for 120 virtual seconds
  each: an initial 1,056-frame native demand is serviced as counted prefix plus
  a 480-frame pipeline block, maintaining a 962-frame active FIFO target without
  underruns/overruns. A subsequent genuine 1,056-frame running demand still
  raises the guard to 1,538 and transfers available audio. This is a numerical
  reproduction/validation of the policy, not a Windows startup timing model.
- Event tests verify starvation-block metadata, preservation across observer
  polls, and bounded diagnostic overflow/reclamation without changing audio
  counters. Native fields are fixed test values, not claimed hardware timing.
- The Windows x64 Release cross-build and portable CLI profile check pass.
  The Windows executable is not executed by the build agent. Native minimum
  negotiation/rejection, driver timing, actual padding, hardware latency and
  real-device preference/fallback and stability of 0.5 still need a Windows
  hardware run. This also applies to the notification suppression used during
  stopped Miniaudio mode-negotiation cleanup. No latency improvement is claimed.

The real 0.1 log separately establishes native/shared duplex buffer processing
and supports clean audible Stop for one actual configuration. It also records
42 reverse FIFO underruns. See [the review](LIVE_RUN_REVIEW_2026-11-11.md); do not
generalize that result to the changed controller, other backends or glitch-free
hardware operation.

The later real 0.2 logs have also been reviewed: one has 49 reverse underruns
then approximately 12.5 minutes without another; another has zero underruns in
both directions over approximately 12 minutes 18 seconds, followed by stopped,
unchanging counters. The reverse issue is intermittent, not established fixed.
See [the second review](LIVE_RUN_REVIEW_2026-11-11_V02.md) and
[the LagMeter run review](LIVE_RUN_REVIEW_2026-11-11_LAGMETER.md).

The 0.3 comparisons are also reviewed in
[the wireless comparison](LIVE_RUN_REVIEW_2026-11-11_V03_AB.md) and
[the wired comparison](LIVE_RUN_REVIEW_2026-11-11_WIRED_AB.md). The wired Low run
reports approximately 148 ms versus baseline's 168 ms, but baseline runs only
approximately 25 seconds. Its startup full-refill raises the FIFO target to
32 ms. Both have zero recorded FIFO underruns and clean Stop; these results do
not verify 0.4's changed startup handling or fix the earlier intermittent issue.

Compare baseline and Low under the same real conditions using
[the current test guide](STARTUP_BUFFER_TEST.md). No hardware latency reduction, quality,
CPU result, physical clock drift or converter superiority follows from these
software checks. The user's approximately 116-ms LagMeter result describes the
intended phone/microphone/mixer/AudioHole/wireless-headphone acoustic loop;
phone calibration and component contributions have not been isolated.

The host reads profiles/scanner data without changing them. Its own logs contain
no samples and are never uploaded. Native adapters and restricted miniaudio
paths retain their documented prototype limitations.

Packaging performs no tests. BuildInfo.json identifies exact executable/source
hashes. This record describes the session producing the 0.5 package, not future
rebuilds. The original 0.1, 0.2, 0.3 and 0.4 archives are preserved unchanged.

## Subsequent 0.4 hardware evidence

[Six backend/preset runs](LIVE_RUN_REVIEW_2026-11-11_V04_BACKENDS.md), approximately
1–2.5 minutes each, verify successful native default/minimum Shared operation and
the new startup prefix/accounting on four native-I/O runs. Standard's active FIFO
target stays at 962 frames instead of the earlier 1,538. All native submission
accounting equations balance after Stop. All six runs stop and disarm with frozen
post-stop counters. No FIFO overrun or backend issue is recorded.

Native WASAPI has zero FIFO underruns in either preset. Experimental Standard
and the Low-labelled Miniaudio run each have two, captured by the new context
mailbox without diagnostic drops. Experimental Low and Miniaudio Standard have
zero. The Miniaudio Low flag still changes no tuning; its negotiated source and
destination records are identical in the pair. Native packet flags/timing remain
unavailable for Miniaudio and must not be treated as clean native observations.

These short runs establish limited real-device behavior, not long-term stability,
exclusive/manual-buffer behavior, rejected-period fallback, calibrated component
latency or a final backend/controller choice. Published 0.4 archives retain their
original build-time evidence and hashes; this post-build review does not replace
or rebuild them. No implementation change accompanies this review.

## Subsequent native Exclusive evidence — November 12

[The wired Exclusive review](LIVE_RUN_REVIEW_2026-11-12_EXCLUSIVE.md) verifies both
requested endpoints opening Exclusive with automatic 48-kHz stereo signed-PCM32
clients and 480-frame / 10-ms buffers. The approximately 11-minute run records
one FIFO starvation event near 65 seconds, then about ten minutes without
another; no overrun, capture discontinuity/timestamp error or backend issue is
recorded. Native preroll/submission accounting balances and Stop freezes counters.

The user reports approximately 92 ms over 300 LagMeter pings, compared with
earlier approximately 148 ms Shared Low on the wired route. The client format
also changes, periods are not smaller, and the internal FIFO target remains
approximately 20 ms. The comparison does not isolate individual latency sources,
prove glitch-free routing, validate exclusive fallback/manual buffer cases, or
implement an exclusive-preferred policy. The shared-only Low scheduling policies
do not apply to the opened Exclusive clients. Published packages are unchanged.
