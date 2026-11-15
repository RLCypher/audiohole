# Profile v4 timing controls

Individual profiles use schema 4; configuration transfers use schema 8. Older
profiles remain readable without writes. An explicit save emits the current
version and preserves unknown properties and inactive tuning preferences.

Each direction has eight optional timing values, in milliseconds:

| Field | Empty value |
| --- | --- |
| `settings.preferences.queue_target_ms` | Automatic |
| `settings.preferences.destination.render_ahead_ms` | Automatic |
| `settings.preferences.common.period_ms` | Automatic |
| `settings.preferences.common.buffer_ms` | Automatic |
| `settings.preferences.source.period_ms` | Inherit common |
| `settings.preferences.destination.period_ms` | Inherit common |
| `settings.preferences.source.buffer_ms` | Inherit common |
| `settings.preferences.destination.buffer_ms` | Inherit common |

Empty values serialize as JSON null. Queue, processing interval, and playback
lead accept positive finite fractional milliseconds. Capacity retains the
existing integer 1–100 ms request range. Placeholders are presentation only.
Disabling advanced tuning preserves the saved values and applies Automatic.

`buffer_ms` requests capacity. Windows may round that allocation upward to a
supported minimum or alignment. Diagnostics show the request, actual frames and
milliseconds, and adjustment. Larger capacity does not increase a Shared-mode
playback lead or a software queue target. Exclusive event mode requires complete
native packets; incompatible independent lead requests are rejected.

Explicit processing intervals must match a supported period to within one
frame. Native Shared mode uses IAudioClient3 for an independent interval. With
an explicit capacity request, legacy initialization supplies that capacity and
the resulting current engine period must still match the requested interval.
No fallback to a substantially larger explicit interval is accepted. Manual
queue targets bypass the automatic packet guard; their effective
frame target remains visible in diagnostics. An aggressive target can underrun.

The standard pipeline can now recover its Automatic guard after a transient
packet/block spike, with conservative backoff after starvation. Explicit targets
remain unchanged; the experimental DLL retains its original guard policy.
See [processing/queue improvements](AUDIO_OVERHEAD_IMPROVEMENTS_2026-11-15.md).

The native WASAPI adapter supports these independent controls. The miniaudio
adapter rejects explicit independent processing intervals and playback lead
because this integration cannot confirm their application. It accepts an
independent software queue target and applies the worker policy through its
WASAPI lifecycle callbacks on each capture/playback worker. No adapter switch
occurs to make an unsupported request appear successful.

Load safe defaults affects only timing in the selected direction. Periods are
resolved from matching Shared format capability evidence. Unknown periods and
unconfirmed playback capacity remain Automatic. Presets are starting requests,
not guarantees of audible output or glitch-free operation. Reset, copy, and
paste preserve other directions and unrelated profile settings.

Application preferences contain `engine.scheduling_task` (`automatic`, `audio`,
`pro_audio`) and `engine.audio_thread_priority` (`automatic`, `normal`, `high`).
Automatic requests Pro Audio MMCSS with Normal relative priority. Changes apply
on the next Start, only to audio workers. Diagnostics distinguish pending,
applied, and unavailable registration/priority; no process priority is changed.

Native driver and hardware measurements must be collected on Windows. Portable
contract tests and cross-builds validate arithmetic, lifecycle, serialization,
and GUI behavior; they do not establish acoustic latency or driver support.

The [automatic](examples/latency-v4/automatic.profile-v4.json),
[stable](examples/latency-v4/stable.profile-v4.json), and
[inheritance](examples/latency-v4/inheritance.profile-v4.json) examples were
exported by the current profile serializer. Endpoint identities are fictional;
numeric values illustrate the contract and are not capability evidence. The
[application fragment](examples/latency-v4/app-engine-settings.fragment.json)
contains the two worker settings to merge into application preferences.

The processing prototype bounds explicit routing queues to 1000 ms and at least
2 source frames. Native periods must satisfy device fundamental/minimum/maximum
constraints within ordinary frame rounding. Preset periods use those constraints,
and generated queue/lead milliseconds reflect frame rounding.
