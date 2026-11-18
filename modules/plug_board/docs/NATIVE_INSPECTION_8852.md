# Native inspection findings and version 0.1.2 icon fix

Source: the user-provided `8852-123145312-0.zip`, containing `inspection.json`,
`icons.json` and eight exported icon PNGs. Archive SHA-256:
`3b0927f5673739511f747e582bc7bb94dce5734a2d4ae2e2899c7edeffea829b`.
The observation timestamp is `2026-11-17T05:19:19.456Z`. The summary below omits
full user paths, PIDs and endpoint identifiers; the original evidence is retained
separately from the distributable package.

## Observed on native Windows

The inspection reports Windows workstation build **26300.9974**, `wine: false`,
`elevated: false`, `snapshot_complete: true`, and no enumeration diagnostics.
It enumerated **45 playback endpoints**, **10 available outputs**, and **8
applications with 27 sessions**. Two sessions were Active; 25 were Inactive.

Spotify and Mozilla Firefox each had one active session on **Music (RØDECaster)**.
Console and multimedia defaults were **System (RØDECaster)**; the communications
default was **Headphones (RODECaster Pro II Secondary)**. Ten inactive Steam
sessions across endpoints remained one Steam application; ten System Sounds
sessions also remained one application. Inactive destinations did not become
active-routing claims. The MSI LEDKeeper entry remains uncertain because one of
its sessions is reported as spanning processes.

These are native read-only enumeration, grouping and identity observations.
The archive contains no capability, process-start, routing-experiment, restoration
or recovery results. It establishes neither private-ABI compatibility on this
Windows build nor actual stream reassignment or restoration.

## Icon defect exposed by the archive

All seven Win32 application Shell lookups failed with `0x80070057`
(`E_INVALIDARG`). Their verified executable paths used the extended DOS form
`\\?\C:\...`, returned by `GetFinalPathNameByHandleW`. The implementation passed
that spelling directly to Shell. Firefox recovered through its session-provided
executable resource; System Sounds resolved through its AudioSrv resource. The
remaining six entries used explicitly labeled generic icons.

Version 0.1.2 converts ordinary extended DOS paths to Shell parsing names for
icon lookup only. Canonical identity paths and process-token validation remain
unchanged. Other namespaces and names sensitive to Win32 normalization retain
their original spelling. If Shell and session resources fail, the loader tries
the verified executable's embedded icon before a generic fallback. A verified
filename is never interpreted as an environment-variable expression or resource
ordinal. Packaged applications retain their AppsFolder lookup.

Six portable regression cases cover the reported Steam path, Unicode, literal
percent/comma characters, ordinary paths, other namespaces and sensitive names.
A Windows evidence test copies the launcher's known embedded icon to a Unicode
filename containing a literal `%SystemRoot%` and comma, obtains its extended final
path, exports an actual PNG and checks that identity evidence remains unchanged.
Supplemental Wine execution passed this test. The corrected icons for the user's
installed applications still require a fresh native inspection.

## Next native checks

Open the version 0.1.2 launcher and use **Refresh applications**, then compare
the real app icons and inspect the new `icons.json` for any fallback diagnostics.
Use **Check Windows support** to record interface availability on build
26300.9974. Select an available nondefault output, then use **Test selected
output** to test only the disposable silent fixture. The result must separately
show preference acceptance, observed stream destination and completed restoration.
Use **Open results folder** to retain that operation's evidence. Follow the full
[native procedure](NATIVE_VALIDATION.md) when assessing the feasibility gate.
