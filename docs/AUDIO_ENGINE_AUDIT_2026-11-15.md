# Audio engine and integration audit — 2026-11-15

Scope: the integrated application at `work` commit
`39f860e5b752de13046f0d04a55054822d3602cb`. The repository's `main`
branch still contains the initial specifications. An isolated checkout preserves
earlier scanner/engine working copies. No GUI/animation implementation changes.

## Supplied run

Log SHA-256:
`95e412c7f8171a1bc75013ba905395c751dd57743810b17314b36d8726f130eb`.

- Every recorded direction uses native WASAPI, Shared access, Automatic audio
  preferences, advanced tuning off, and Exclusive preference off. No Exclusive
  attempt or HRESULT refusal appears. This excerpt cannot establish which
  application, if any, blocked Exclusive in a different run.
- Both directions negotiate 48-kHz, two-channel float32. The software queue target
  is 20 ms, playback lead 11 ms, and playback allocation 22 ms. These represent
  different quantities; none is calibrated acoustic latency.
- Forward capture/playback periods are 10 ms. Return capture is 7 ms and playback
  10 ms. MMCSS Pro Audio / Normal is reported applied.
- Counters reset between entries 2711 and 2714. Forward ends the first run at 42
  cumulative FIFO underruns, with 39 already present at the excerpt's beginning.
  The next run ends at 34. Return has zero FIFO underruns in both runs; recorded
  overruns are zero. The second run's capture count corresponds to about 14.4
  minutes at nominal rate, not a physical-clock measurement.
- Both runs end Stopped. No samples or channel-content observations are included.
  A two-channel format does not establish that the source signal is stereo.

## Changes

### Automatic mono monitoring

Format assessment advertised mono-to-stereo support, but `compile_mapping` left
the second playback channel silent. The shared processing kernel now duplicates
a genuinely one-channel source into the first two playback channels. Extra
channels stay silent; stereo sources retain distinct left/right samples.
Explicit maps remain authoritative.

This engine policy applies to WASAPI, Experimental and Miniaudio without a new
profile field, forced capture width or manual map. It does not manufacture stereo
from a two-channel dual-mono signal or infer mixer buses. Tests exercise both the
mapping kernel and real PCM/FIFO processing path with different left/right
samples. The new mono regression first failed against the old implementation.

### Scanner-assisted native format acquisition

The host previously passed exact endpoint IDs and availability, but omitted
scanner format capabilities. `routing_bindings` now copies at most 128 distinct
positive Exclusive tuples per endpoint, including reported channel masks. It
excludes unknown/negative evidence, contradictory identical records, unsupported
representations, invalid PCM geometry/precision and inconsistent channel masks.
The inventory revision still identifies the copied observation.

Native WASAPI and Experimental keep the live configured format as their first
Exclusive candidate. After an unsupported-format response they can try scanner
tuples satisfying **every active explicit rate/channel/encoding requirement**.
Every candidate is queried against the current endpoint before Initialize.
Old hints can be rejected without forcing their values into an Automatic profile.

Selection is initialization-only, bounded to the live tuple plus 128 hints,
deduplicated and cooperatively cancellable. Only a format refusal allows another
tuple/header alias. Ownership, permission and device errors stop that search;
access policy separately decides whether Shared fallback is allowed. Forced
Exclusive remains strict. The existing diagnostics identify the accepted format
as live-configured or scanner-derived and currently revalidated.

Miniaudio retains library negotiation and does not claim to use these hints.
Hosts that omit hints, including the existing standalone host, retain live-format
selection. Hints are local operational data, not new saved JSON. Profiles,
unknown fields, IDs, filenames, notes, retained tuning and clock metadata are not
rewritten. Rebuild/relink hosts with the changed C++ value types; there is no
separately versioned binary ABI.

### Exclusive diagnostics

`0x8889000A` preserves the ownership refusal and explains the two endpoint
Exclusive settings without inferring an owner from session listings.
`0x8889000E` explains that Windows disallows Exclusive access. `0x88890020` is
classified as an unsupported processing interval rather than an ownership issue.
The original HRESULT remains available.

Windows can preempt Shared clients when Exclusive use and priority are enabled.
Another Exclusive owner must release the endpoint. No external application is
killed or suspended and no Windows policy is changed. Preemption can interrupt
other clients' audio; it does not mix those clients with an Exclusive route.

## Behavior reviewed and retained

- Independent directional tuning/access, authoritative endpoint/common
  inheritance, exact bindings and immutable Start intent.
- Preferred fallback versus forced-mode failure; startup progress, Stop
  cancellation and owning-thread COM/resource cleanup.
- Independent queue/lead/period/capacity requests. Rounded capacity does not
  silently enlarge the Shared playback lead or an explicit queue target.
- Destination pacing, bounded FIFOs, correction commands and native underrun
  context. Correction ppm is not measured physical drift.
- Application-level worker scheduling and unapplied preferred-clock metadata.

Current source already includes packet-age/padding/underrun context absent from
the supplied log. Collect it before attributing Forward starvation to clocks or
increasing global queue defaults. Automatic buffering does not guarantee tolerance
of every device delivery gap. The linear adaptive resampler remains a prototype;
this audit does not establish production conversion quality or bit-perfect output.

## Validation and hardware follow-up

The Linux Debug build passed all 275 CTest checks, including engine, scanner,
model/integration, format assessment, tutorial and asset integrity checks. The
699 approved decoded-source PNG files passed the existing hash/structure check.
The complete standalone Windows engine/host/test Release build passed with
LLVM-MinGW 20261104 (Clang 23.1.5). Windows executables were not run locally.
Native MSVC/GUI execution and live hardware verification remain pending.

Portable fixtures validate arithmetic, samples and lifecycle behavior, not real
endpoint availability. Cross-builds establish Windows source compatibility, not
driver behavior, audible output or measured latency.

On real hardware:

1. Play known left-only/right-only material through the intended mixer return.
   Then test a genuinely mono microphone with an empty map and advanced tuning
   disabled. If stereo is already collapsed, inspect upstream mixer/headset
   configuration and Windows mono accessibility settings.
2. Stop, enable Exclusive preference, restart, and export per-endpoint receipts.
   Repeat forced Exclusive; any refusal must remain an error. Test both Windows
   endpoint Exclusive settings deliberately.
3. Repeat with Steam/SteamVR running. Use actual HRESULTs to distinguish ownership,
   permission, format and interval failures.
4. Run longer and inspect Forward packet gaps/age, render padding, underrun
   context and controller target. Compare one timing change at a time.
5. Verify Stop releases ownership and saved profiles/notes remain unchanged.

See [driver and Windows-format research](AUDIO_DRIVER_AND_WINDOWS_FORMAT_RESEARCH.md)
for the additional owner-requested investigations.
