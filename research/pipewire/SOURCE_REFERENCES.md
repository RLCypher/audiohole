# Source references

PipeWire source revision: **1.4.9 / `fd60e04525f3a04d90bf50085222e0cc9139b4a4`**. Each link below is immutable at that commit; line ranges are navigation aids, not claims that an entire file was executed. Exact source identifiers and hashes are recorded in [upstream/MANIFEST.json](upstream/MANIFEST.json). All source statements are **Verified by inspection**, unless the research document labels a deduction Inferred or a model/proposal Experimental.

The GitHub tag ref resolved directly to this commit. Initial shell clone failed at proxy connection; the GitHub connector supplied original UTF-8 files instead. Snapshot bytes were verified against Git blob SHA-1 values. No latest-release or full-repository coverage claim is made.

## PipeWire implementation and documentation

<a id="s01"></a>

### S01 — DLL helper

`struct spa_dll`, `spa_dll_init`, `spa_dll_set_bw`, `spa_dll_update`: double states, bandwidth constants, exact ordered recurrence. Verified helper and actual ALSA caller S02.

- [`spa/include/spa/utils/dll.h` (27-57)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/utils/dll.h#L27-L57)

<a id="s02"></a>

### S02 — ALSA PCM clocks, targets, recovery

`spa_alsa_init` (969–1035), `recalc_headroom` (2034–2068), period negotiation (2322–2400), `spa_alsa_update_rate_match` (2444–2478), `set_swparams` (2480–2522), `do_prepare`/`alsa_recover` (2608–2751), `get_avail`/`get_status` (2754–2854), `update_time` (2857–2960), `need_resample`/`setup_matching` (2962–2992), `check_position_config` (3005–3045), read/write synchronization, timer work, and follower reassignment. Primary evidence of actual controller use; wrappers accept RateMatch IO. Header constants include MAX_HTIMESTAMP_ERROR and BW_PERIOD.

- [`spa/plugins/alsa/alsa-pcm.c` (2444-3104)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm.c#L2444-L3104)
- [`spa/plugins/alsa/alsa-pcm.c` (3313-3375)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm.c#L3313-L3375)
- [`spa/plugins/alsa/alsa-pcm.c` (3541-3658)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm.c#L3541-L3658)
- [`spa/plugins/alsa/alsa-pcm.c` (3833-3874)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm.c#L3833-L3874)
- [`spa/plugins/alsa/alsa-pcm.h` (39-72)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm.h#L39-L72)
- [`spa/plugins/alsa/alsa-pcm-sink.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm-sink.c)
- [`spa/plugins/alsa/alsa-pcm-source.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/alsa-pcm-source.c)

<a id="s03"></a>

### S03 — Timing, rate matching and async buffer ABI

`spa_io_clock`, `spa_io_position`, `spa_io_rate_match`, `spa_io_async_buffers`: units, ID relationships, required frame counts, delay/phase contract, alternating cycles. Comments are API contracts; executed implementations are separately cited.

- [`spa/include/spa/node/io.h` (104-159)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/node/io.h#L104-L159)
- [`spa/include/spa/node/io.h` (308-358)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/node/io.h#L308-L358)

<a id="s04"></a>

### S04 — Adapter wiring

`link_io()` installs one RateMatch block on hardware/converter ports; `negotiate_buffers` negotiates reusable buffers. Confirms actual connection instead of assuming a utility header implies use.

- [`spa/plugins/audioconvert/audioadapter.c` (271-306)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/audioadapter.c#L271-L306)
- [`spa/plugins/audioconvert/audioadapter.c` (429-540)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/audioadapter.c#L429-L540)

<a id="s05"></a>

### S05 — Converter control and passthrough

`resample_update_rate_match`, `resample_is_passthrough`, `impl_node_process`: correction multiplication, per-direction size/delay, queue subtraction, graph-rate scaling. `setup_resample` area around 1990 prepares native state; props defaults around 120.

- [`spa/plugins/audioconvert/audioconvert.c` (2159-2215)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/audioconvert.c#L2159-L2215)
- [`spa/plugins/audioconvert/audioconvert.c` (3548-3574)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/audioconvert.c#L3548-L3574)

<a id="s06"></a>

### S06 — Native resampler and inspected test material

`impl_native_update_rate`, `impl_native_in_len/out_len/process/reset/delay/phase`, `build_filter`, `resample_native_init`, `MAKE_RESAMPLER_FULL/INTER`, scalar/interpolated dot products, quality/CPU dispatch. The two upstream test files are retained as reproducible future validation references; they were not compiled or executed. Only the small DLL/ring harness ran.

- [`spa/plugins/audioconvert/resample-native.c` (21-83)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample-native.c#L21-L83)
- [`spa/plugins/audioconvert/resample-native.c` (139-201)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample-native.c#L139-L201)
- [`spa/plugins/audioconvert/resample-native.c` (204-435)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample-native.c#L204-L435)
- [`spa/plugins/audioconvert/resample-native-impl.h` (29-143)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample-native-impl.h#L29-L143)
- [`spa/plugins/audioconvert/resample-native-c.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample-native-c.c)
- [`spa/plugins/audioconvert/resample-native-sse.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample-native-sse.c)
- [`spa/plugins/audioconvert/resample.h`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/resample.h)
- [`spa/plugins/audioconvert/test-resample.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/test-resample.c)
- [`spa/plugins/audioconvert/test-resample-delay.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/audioconvert/test-resample-delay.c)

<a id="s07"></a>

### S07 — Ring buffer

32-bit indices, acquire/release/relaxed ordering, signed modular fill, two-piece copies. No capacity stored or enforced in the structure. Actual header used by local wrap probe.

- [`spa/include/spa/utils/ringbuffer.h` (38-172)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/utils/ringbuffer.h#L38-L172)

<a id="s08"></a>

### S08 — Loopback module boundary

Capture/playback buffer transfer and optional target-delay allocation are distinct from hardware ASRC/controller plumbing. Inspection did not find a DLL in this module; the primary clock-control evidence remains S02–S05. Do not infer clock correction merely from an optional delay ring.

- [`src/modules/module-loopback.c` (360-441)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/modules/module-loopback.c#L360-L441)
- [`src/modules/module-loopback.c` (587-608)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/modules/module-loopback.c#L587-L608)

<a id="s09"></a>

### S09 — Node drivers and cycle processing

`insert_driver`, xrun diagnostics, `node_ready` graph-cycle processing and `trigger_targets`. Driver priority order, previous-cycle checks and processing initiation.

- [`src/pipewire/impl-node.c` (765-783)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/impl-node.c#L765-L783)
- [`src/pipewire/impl-node.c` (1349-1450)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/impl-node.c#L1349-L1450)
- [`src/pipewire/impl-node.c` (2068-2213)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/impl-node.c#L2068-L2213)

<a id="s10"></a>

### S10 — Graph recalculation and quantum policy

`find_best_rate`, `pw_context_recalc_graph`: grouping, allowed rates, force/lock constraints, latency requests, quantum scaling/clamping/power-of-two preference, start ordering. Rate heuristic is policy, not physical synchronization.

- [`src/pipewire/context.c` (1423-1515)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/context.c#L1423-L1515)
- [`src/pipewire/context.c` (1550-1665)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/context.c#L1550-L1665)
- [`src/pipewire/context.c` (1666-1909)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/context.c#L1666-L1909)

<a id="s11"></a>

### S11 — Official upstream scheduling explanation

Describes activation records, dependency graph, driver dispatch, remote peers, and lazy/request scheduling. Cross-check with S09 and S17: one prose passage says decrement required where the implementation actually decrements pending. This report follows implementation.

- [`doc/dox/internals/scheduling.dox`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/doc/dox/internals/scheduling.dox)

<a id="s12"></a>

### S12 — Allocation, sharing and port setup

`do_allocation`, `alloc_buffers`, `pw_buffers_negotiate`, port buffer installation/reuse. Setup uses reusable arrays/shared memory as needed; does not establish end-to-end zero-copy or absence of all RT allocations.

- [`src/pipewire/impl-link.c` (500-620)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/impl-link.c#L500-L620)
- [`src/pipewire/buffers.c` (29-109)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/buffers.c#L29-L109)
- [`src/pipewire/buffers.c` (300-342)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/buffers.c#L300-L342)
- [`src/pipewire/impl-port.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/impl-port.c)

<a id="s13"></a>

### S13 — Compiled defaults and sample configuration

48 kHz, quantum 1024, min/max 32/2048, floor/limit 4/8192, power-of-two preference. Defaults are configurable; VM/rule/distribution settings may differ.

- [`src/pipewire/settings.c` (23-39)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/settings.c#L23-L39)
- [`src/daemon/pipewire.conf.in` (16-65)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/daemon/pipewire.conf.in#L16-L65)

<a id="s14"></a>

### S14 — Data-loop threads and real-time policy

Loop worker and thread utilities; Linux SCHED_FIFO/rlimits and RTKit/portal alternatives. Module docs/implementation do not imply universal hard deadlines or Windows-equivalent numeric priorities.

- [`src/pipewire/data-loop.c`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/data-loop.c)
- [`src/modules/module-rt.c` (58-140)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/modules/module-rt.c#L58-L140)

<a id="s15"></a>

### S15 — Latency parameters

`spa_latency_info` and process latency helpers combine quantum/rate/ns terms. Use the negotiated timebase and distinguish these metadata estimates from external loopback measurements.

- [`spa/include/spa/param/latency.h`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/param/latency.h)
- [`spa/include/spa/param/latency-utils.h`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/param/latency-utils.h)

<a id="s16"></a>

### S16 — Official architecture/audio overview

Background on framework separation and audio processing. Supporting context only; detailed algorithm claims rely on implementations above.

- [`doc/dox/internals/design.dox`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/doc/dox/internals/design.dox)
- [`doc/dox/internals/audio.dox`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/doc/dox/internals/audio.dox)

<a id="s17"></a>

### S17 — Activation counters and eventfd triggering

`pw_node_activation_state_reset`, `trigger_target_v1`: required→pending copy, atomic decrement, CAS status, eventfd write. Implementation resolves the terminology ambiguity in S11.

- [`src/pipewire/private.h` (519-542)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/private.h#L519-L542)
- [`src/pipewire/private.h` (657-684)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/src/pipewire/private.h#L657-L684)

<a id="s18"></a>

### S18 — License and component exception evidence

MIT text plus top-level ALSA/JACK exceptions; explicit LGPL-2.1-or-later PulseAudio notice and ACP linkage. Reviewed selected components only; no full transitive dependency audit.

- [`LICENSE`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/LICENSE)
- [`COPYING`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/COPYING)
- [`spa/plugins/alsa/acp/alsa-util.c` (1-19)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/acp/alsa-util.c#L1-L19)
- [`spa/plugins/alsa/meson.build` (1-36)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/plugins/alsa/meson.build#L1-L36)

<a id="s19"></a>

### S19 — Numerical and configuration helper definitions

Rounding and scale macros, atomic wrappers and property keys. `SPA_SCALE32_UP` uses uint64 intermediates and ceil division; callers still validate ranges/nonzero denominators.

- [`spa/include/spa/utils/defs.h` (290-307)](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/utils/defs.h#L290-L307)
- [`spa/include/spa/utils/atomic.h`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/utils/atomic.h)
- [`spa/include/spa/utils/keys.h`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/utils/keys.h)
- [`spa/include/spa/node/keys.h`](https://github.com/PipeWire/pipewire/blob/fd60e04525f3a04d90bf50085222e0cc9139b4a4/spa/include/spa/node/keys.h)

## Windows official API references

Saved from MicrosoftDocs/sdk-api `docs` on 2026-10-25. Individual exact blob hashes are recorded; no common docs commit was pinned. Official web links below may evolve; use exact blob identifiers for the examined wording; optional local snapshots are excluded from publication. Documentation API contracts do not substitute for Windows driver measurements.

<a id="w01"></a>

### W01 — IAudioClock::GetPosition

Position/frequency units, correlated QPC in 100 ns, extrapolation, Reset semantics, S_FALSE quality.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/7505a98d3e855cb7dc9ad9cbba51d2363748ef07)

<a id="w02"></a>

### W02 — IAudioClock::GetFrequency

Units compatible with position; reported nominal frequency ignores drift/jitter.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getfrequency) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/145da16089e753484ecb73c8de7855369087008b)

<a id="w03"></a>

### W03 — IAudioCaptureClient::GetBuffer

Packet position in frames, 100 ns QPC, silence/discontinuity/timestamp flags, packet acquire/release rules.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/4a167250d427df523d58df3d27407d66e61d4f1d)

<a id="w04"></a>

### W04 — IAudioClient::GetCurrentPadding

Shared render occupancy versus next-packet shared capture length; exclusive event-driven distinction.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getcurrentpadding) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/ae9ccfe77db0749eef23c67255e679fdfe3dbcb6)

<a id="w05"></a>

### W05 — IAudioClient3::GetSharedModeEnginePeriod

Format-dependent fundamental/minimum/maximum shared periods and allowed multiples.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient3-getsharedmodeengineperiod) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/7bfe717ebc5ad958041a37ebfe9adae5dccab0be)

<a id="w06"></a>

### W06 — IAudioClient3::InitializeSharedAudioStream

Shared periodicity initialization constraints and errors.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient3-initializesharedaudiostream) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/1871ab65e1f7d5128cf6fd04d07b29cee388ac6f)

<a id="w07"></a>

### W07 — IAudioClient::Initialize

Mode/format/event initialization, exclusive event buffer duration/periodicity and alignment.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/5cd1348dd4f65abd4403fc8bdda18352c9fbd126)

<a id="w08"></a>

### W08 — AvSetMmThreadCharacteristicsW

Associate thread with configured multimedia task, returned handle and errors. Teardown must use the corresponding platform lifetime contract.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsw) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/90edefb9bab5460393874476b7d1a376dcb13dba)

<a id="w09"></a>

### W09 — IAudioClockAdjustment::SetSampleRate

Shared-mode requirement, rejected rate possibility, explicit prohibition on real-time processing-thread calls.

- [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclockadjustment-setsamplerate) · [exact Git blob](https://api.github.com/repos/MicrosoftDocs/sdk-api/git/blobs/d6c14de4962aa8d23d4d713256021c4c3756788d)

## AudioHole baseline and executed evidence

AudioHole commit: `a2aaf7a8ca2b14c3ca37b59cf6d8bc255124a984`. Read-only baseline: README, AGENTS, SPEC, ARCHITECTURE, DECISIONS, DEPENDENCIES, DEVICE_PROBING and ROADMAP; relevant requirements and decision identifiers are catalogued in [ARCHITECTURE.md](ARCHITECTURE.md). Current remote `work` was additionally reviewed before publication; see [CURRENT_BASELINE.md](CURRENT_BASELINE.md). No existing baseline documents were rewritten. Minor pre-existing state-name wording inconsistencies are not resolved by this research; accepted decisions/canonical states govern any future implementation.

Executed evidence: [experiment methodology](EXPERIMENTS.md), [C harness](experiments/dll_probe.c), [raw model results](experiments/dll_results.csv), [sanitized results](experiments/dll_results_sanitized.csv), [numerical script](experiments/numerics.py), [numerical results](experiments/numerics.json), [consistency checker](experiments/verify_research.py). The upstream native resampler tests were retained as future references and were **not run**. No hardware measurements or Windows tests are represented by these files.

