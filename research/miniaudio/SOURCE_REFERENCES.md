# Pinned source references

Upstream release **0.11.25**, commit **`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`**. Source clone is `upstream/miniaudio/`; all code line ranges below are the unsplit root `miniaudio.h`. Windows code was source-inspected, not run.

[Upstream commit](https://github.com/mackron/miniaudio/commit/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d) · [root header](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h) · [README](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/README.md) · [CHANGES](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/CHANGES.md)

## Acquisition and reproducibility

The first ordinary shell clone failed because the executor could not reach the session proxy. A connected GitHub read returned the master commit above; the shell clone then succeeded with command-scoped network access while retaining the session proxy. The cloned HEAD matched the independently fetched commit. No bypass of a destination-policy denial was attempted. The source checkout remains unmodified. `upstream/SNAPSHOT.json` records SHA-256 and Git blob hashes of examined primary files. No assertion is made that this remains the latest revision after retrieval.

The provided AudioHole task checkout was retained; only `research/miniaudio/` was authored. Baseline `a2aaf7a8ca2b14c3ca37b59cf6d8bc255124a984`. At publication, remote `work` had advanced to `766ed88f7d7786bfb90fb7a520da34a15de010d1`; its current specifications and implementation status were independently reviewed and this report reconciled. Publication uses an isolated base tree and commit via the GitHub connector, without a shared-checkout switch or reset. Research source clones/build artifacts remain inside it.

## W1 — Backend context, commands and registration

- [context command posting, command thread and service acquisition](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L22618-L22800) — `miniaudio.h:22618–22800`.
- [WASAPI context initialization, dynamic linking, callbacks; read/write registration](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L25031-L25210) — `miniaudio.h:25031–25210`.

## W2 — One-endpoint initialization and period negotiation

- [ma_device_init_internal__wasapi: formats, flags, shared/exclusive, period calculation, Client3/legacy](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L23626-L24085) — `miniaudio.h:23626–24085`.
- [ma_device_init__wasapi: per-side initialization, auto-reset events, descriptors and notifications](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24229-L24455) — `miniaudio.h:24229–24455`.

## W3 — Reinitialization and start/stop

- [ma_device_reinit__wasapi: abandoned audio-client references, refreshed descriptors/converters](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24108-L24228) — `miniaudio.h:24108–24228`.
- [reroute, start/stop, MMCSS, drain/reset](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24514-L24719) — `miniaudio.h:24514–24719`.

## W4 — WASAPI capture/render transfer

- [available frames: GetCurrentPadding and buffer capacity](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24458-L24513) — `miniaudio.h:24458–24513`.
- [read/write packet mapping/copies, flags, discontinuity discard, waits and wakeup](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24721-L25026) — `miniaudio.h:24721–25026`.

## G1 — Synchronous worker and duplex path

- [ma_device_audio_thread__default_read_write: duplex read/convert/callback/convert/write and simplex cases](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L20737-L20924) — `miniaudio.h:20737–20924`.
- [ma_worker_thread: Windows COM, lifecycle and default-loop selection](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L42762-L42887) — `miniaudio.h:42762–42887`.

## G2 — Callback adapter and asynchronous duplex helpers

- [on_data adapter, pre-silencing, master volume/clipping, read/send helpers](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L20245-L20565) — `miniaudio.h:20245–20565`.
- [asynchronous duplex ring capture/playback helpers; distinct from WASAPI path](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L20566-L20695) — `miniaudio.h:20566–20695`.

## G3 — Converters and device initialization

- [post_init_setup converters, allowDynamicSampleRate false, optional input cache](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L42497-L42677) — `miniaudio.h:42497–42677`.
- [config defaults, descriptor creation, fixed callback buffers and worker/ring selection](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L43596-L43965) — `miniaudio.h:43596–43965`.

## R1 — Linear SRC initialization, phase and interpolation

- [linear config, rational rates, phase rescaling, heap layout, init, s16/f32 interpolation](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L53032-L53349) — `miniaudio.h:53032–53349`.

## R2 — Linear SRC processing

- [s16/f32 downsampling/upsampling loops, filter placement, phase advance](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L53350-L53694) — `miniaudio.h:53350–53694`.

## R3 — Linear SRC setters/counts/latency

- [direct ratio 1e6, count/latency helpers and reset](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L53696-L53849) — `miniaudio.h:53696–53849`.

## R4 — Generic resampler and data converter

- [linear backend vtable, generic config/process/setters; generic ratio denominator 1000](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L53854-L54274) — `miniaudio.h:53854–54274`.
- [data converter defaults, dynamic flag, midformat and execution-path planning](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L55905-L56214) — `miniaudio.h:55905–56214`.
- [data converter processing paths/setters; ratio delegation](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L56245-L56990) — `miniaudio.h:56245–56990`.

## A1 — Filter coefficients, processing and latency

- [biquad fixed point, normalization and direct-form-II transposed processing](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L47158-L47484) — `miniaudio.h:47158–47484`.
- [first-order LPF coefficient and recurrence](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L47637-L47774) — `miniaudio.h:47637–47774`.
- [second-order LPF design and biquad delegation](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L47776-L47931) — `miniaudio.h:47776–47931`.
- [LPF section split, Butterworth Q formulas, reinit and reported latency](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L47933-L48376) — `miniaudio.h:47933–48376`.

## A2 — PCM conversion and dither

- [LCG state/helpers and rectangle/triangle dither](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L14055-L14199) — `miniaudio.h:14055–14199`.
- [scalar format helper distinction](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L45096-L45115) — `miniaudio.h:45096–45115`.
- [s16 to f32 scale](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L45615-L45663) — `miniaudio.h:45615–45663`.
- [public f32 to s16 reference/optimized/SSE2/NEON and dispatch](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L46430-L46778) — `miniaudio.h:46430–46778`.

## A3 — Channel selection, weights, mixing and s24 defect

- [fixed point, six-plane table/dot product and config](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L54283-L54428) — `miniaudio.h:54283–54428`.
- [path selection, shuffle table and packed-s24 shuffle implementation](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L54453-L54695) — `miniaudio.h:54453–54695`.
- [weight initialization, spatial remapping and optional LFE](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L55267-L55485) — `miniaudio.h:55267–55485`.
- [mono, shuffle and matrix processing, public dispatch](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L55531-L55875) — `miniaudio.h:55531–55875`.

## B1 — SPSC byte and PCM rings

- [allocation, offset encoding/acquire/commit, occupancy](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L58137-L58512) — `miniaudio.h:58137–58512`.
- [frame-domain ring wrappers and metadata](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L58651-L58888) — `miniaudio.h:58651–58888`.

## B2 — Asynchronous duplex ring helper

- [five-period capacity and two-unscaled-period prime](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L58892-L58917) — `miniaudio.h:58892–58917`.

## B3 — Constants and buffer/count helpers

- [period count/latency profile and resampler LPF defaults](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L12216-L12238) — `miniaudio.h:12216–12238`.
- [resampled frame-count helper and scratch default](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L17461-L17489) — `miniaudio.h:17461–17489`.
- [milliseconds to frames and copy helper](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L44587-L44603) — `miniaudio.h:44587–44603`.

## T1 — Priority, MMCSS and timing

- [priority enum: highest/default 0, normal -2](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L4448-L4464) — `miniaudio.h:4448–4464`.
- [Win32 priority mapping/CreateThread](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L17864-L17899) — `miniaudio.h:17864–17899`.
- [WASAPI usage strings](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L22602-L22615) — `miniaudio.h:22602–22615`.
- [MMCSS start/revert and lifecycle](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24538-L24613) — `miniaudio.h:24538–24613`.
- [zeroed context config](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L43165-L43172) — `miniaudio.h:43165–43172`.

## D1 — Identity structures and active endpoint enumeration

- [device ID union and device info](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L7040-L7102) — `miniaudio.h:7040–7102`.
- [default eConsole resolution and MMDevice ID handling](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L22976-L23114) — `miniaudio.h:22976–23114`.
- [friendly name and active-only enumeration](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L23115-L23238) — `miniaudio.h:23115–23238`.

## D2 — Capability reporting and enumeration dispatch

- [shared mix format and single exclusive candidate/fallback search](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L22800-L22963) — `miniaudio.h:22800–22963`.
- [desktop/UWP enumeration and detailed info entry points](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L23430-L23538) — `miniaudio.h:23430–23538`.

## D3 — Device event/recovery implementation

- [state/default change policy, no-op added/removed/property, role ignored, exclusive restriction](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L22367-L22598) — `miniaudio.h:22367–22598`.
- [reinit leak comment, abandoned audio-client pointers and converter refresh](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24108-L24228) — `miniaudio.h:24108–24228`.
- [default-ID auto-routing eligibility and notification registration](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L24410-L24455) — `miniaudio.h:24410–24455`.

## E1 — executed isolated probe

[Original C harness](experiments/processing_probe.c), [normal results](experiments/processing_probe_results.txt), [sanitized results](experiments/processing_probe_sanitized_results.txt), [runner](experiments/run_probes.sh), [environment](experiments/environment.json). Processing-only build; no hardware measurement. Read [EXPERIMENTS](EXPERIMENTS.md) for exact methods and limits.

## L1 — licensing and optional components

- [Core license](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/LICENSE); [header license](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h#L95817-L95867).
- [stb_vorbis separate license alternatives](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/extras/stb_vorbis.c#L5544-L5584).
- [Removed historical Speex API](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/CHANGES.md#L397).

## Official Windows API documentation consulted

- [IAudioClient3::InitializeSharedAudioStream](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient3-initializesharedaudiostream): legal period range/multiplicity and API-owned buffer size.
- [IAudioCaptureClient::GetBuffer](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer): packet lifetime/flags, frame position, QPC in 100 ns units.
- [IAudioClock::GetPosition](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition): correlate position/frequency and QPC; position is not universally frames.
- [IMMNotificationClient](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immnotificationclient): opaque endpoint IDs, nonblocking callback and lifetime constraints.
- [IAudioClockAdjustment::SetSampleRate](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclockadjustment-setsamplerate) and [stream flags](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants): shared-render RATEADJUST service, non-real-time setter; Client3 compatibility not implied.
- [AvSetMmThreadCharacteristicsA](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsa): calling-thread task registration, failure result, revert requirement.

The attempted Microsoft `/win32/coreaudio/low-latency-audio` page was inaccessible through the web tool and is not used as evidence. No third-party summaries or issue allegations are relied on for the significant implementation findings.

## Negative claims and search scope

Absence claims about drift/timing are scoped to the traced desktop WASAPI registration → worker → default read/write → converters → native client calls. Full-header symbol searches for IAudioClock, GetPosition, ratio setters, duplex helpers and timing flags cross-checked the path. A symbol in another backend, a high-level engine pitch control or a private struct is not proof of a supported Windows device feature. Code comments are supporting rationale, while executable branches control behavior; contradictory comments are called out.
