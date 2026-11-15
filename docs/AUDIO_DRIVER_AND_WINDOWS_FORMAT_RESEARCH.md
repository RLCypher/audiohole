# Audio driver and Windows format research

Research date: 2026-11-15. Recommendations for evaluation, not new dependencies
or authorization to change a user's system.

## Optional virtual audio driver

Continue improving user-mode routing between existing endpoints. A custom driver
is not required for that operation.

An optional virtual driver could let other applications choose an **AudioHole
playback device** or **AudioHole microphone** directly. It could provide software
buses, avoid some physical mixer round trips, and keep clients on a virtual
endpoint while AudioHole alone opens a physical output. Any latency benefit from
avoiding a mixer trip needs measurement; another transport can also add buffering.

It cannot override another application's Exclusive ownership of a different
physical endpoint, improve a manufacturer's USB driver by itself, eliminate
physical clock differences, or remove mixer DSP/wireless delays. Moving existing
copying code into the kernel is not evidence of improved performance.

Microsoft's [SysVAD sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/)
is a relevant WDM/WaveRT starting reference, not a completed AudioHole cable.
Product work still includes secure data transport, topology, queue/clock
ownership, installation/removal, sleep/device-loss recovery and load testing.
Prefer a minimal bounded endpoint transport with graph control, conversions and
diagnostics remaining in user mode.

Driver development adds WDK/SDK tooling, dedicated Windows testing, kernel crash
analysis, Secure Boot/Memory Integrity compatibility, signing and distribution
maintenance. Microsoft recommends
[HLK-tested dashboard signing](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings).
Its [attestation instructions](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation)
require Hardware Developer Program registration and an EV certificate; retail
Windows Update distribution is unavailable for attestation-only submissions.
Disabling protection or relying on test signing is not a distribution plan.

Recommendation: finish reliable user-mode routing, establish a concrete virtual
bus requirement, compare an existing virtual cable with a minimal optional
SysVAD-based prototype, then measure total latency/reliability before approving
a production driver. No driver is implemented or installed by this audit.

## Windows endpoint format changes through PowerShell

Aligning nominal sample rates can remove an avoidable fixed-rate conversion when
Shared endpoints differ, for example 44.1 versus 48 kHz. Matching channel count
or bit depth alone does not establish a faster path. Shared Windows processing
and AudioHole's prototype use float32 internally; independent devices can still
need adaptive clock compensation when both report 48 kHz.

The supplied run already has 48-kHz stereo float32 clients in both directions.
It provides no evidence of a nominal-rate mismatch that a global setting change
would fix, and does not expose the complete hardware pipeline. Device delivery,
queued audio, DSP and transport delays require separate measurements.

The supported stream-local approach is to read the current mix format, honor
requested endpoint overrides, query live support and negotiate stream periods.
Exclusive uses a supported hardware tuple without changing the Windows Shared
default. Scanner hints now assist that native selection in the main engine.

PowerShell can call COM through helper code, but has no built-in general audio
endpoint format-setting cmdlet. Third-party scripts often use the undocumented
`IPolicyConfig::SetDeviceFormat`; it is not a stable documented application API.
Microsoft states that applications may read
[endpoint properties but should not set them](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audio-endpoint-properties).
The documented
[IAudioEndpointFormatControl](https://learn.microsoft.com/en-us/windows/win32/api/audioendpoints/nn-audioendpoints-iaudioendpointformatcontrol)
only resets the manufacturer default rather than selecting an arbitrary profile
format. See also [device formats](https://learn.microsoft.com/en-us/windows/win32/coreaudio/device-formats).

A default-format change affects the whole endpoint, can invalidate streams and
cannot satisfy two conflicting profiles simultaneously. It does not grant
Exclusive access or eliminate clock compensation. Avoid silent Start-time global
policy changes or registry edits.

Recommendation: prefer a read-only diagnostic helper comparing profile requests
with scanner/live observations and opening `mmsys.cpl` for manual experiments.
Use exact endpoint identities, not names. If an unsupported setter is evaluated
separately in a laboratory, require explicit opt-in, snapshot the original global
settings, stop affected routing, verify the change, provide restoration and rescan.
No setter or system modification is included here.

## Exclusive access with other applications running

[Microsoft's Exclusive-mode documentation](https://learn.microsoft.com/en-us/windows/win32/coreaudio/exclusive-mode-streams)
describes Exclusive-control and Exclusive-priority endpoint settings. With
preemption enabled, an Exclusive open can interrupt Shared clients; an existing
Exclusive owner still blocks it. EarTrumpet entries alone cannot identify that
owner. Use native HRESULTs and access receipts.

No supported general mechanism lets AudioHole seize an endpoint from an Exclusive
owner while preserving that application's stream. Shared fallback supports
coexistence; forced Exclusive must report refusal. Virtual endpoints could alter
which endpoint applications use, not bypass physical access rules.

[Windows low-latency guidance](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio)
separates application/engine buffering, APO processing and hardware delay.
Benchmark these components before attributing observed delay to conversion.
