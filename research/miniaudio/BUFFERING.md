# Buffering and latency accounting

Revision and evidence labels: [README](README.md). Source keys [W2, W4, G1–G3, B1–B3](SOURCE_REFERENCES.md). All latency arithmetic below is an estimate, not a measurement.

## Mandatory and optional stages on Windows

| Stage | Mechanism / ownership | Bypass or caveat |
| --- | --- | --- |
| WASAPI endpoint buffer | Driver/engine negotiated frames, library maps/releases | Mandatory endpoint transfer; capacity and padding differ from requested subperiod. |
| Capture stack scratch | `ma_device_read__wasapi` copies packets into generic worker scratch | Stock blocking wrapper does not expose packet pointer to application callback. Native I/O/extension could change ownership path. |
| Capture converter | `ma_data_converter`: native→client rate/format/layout | Static identical configuration chooses passthrough; duplex worker still requests copy into client scratch. |
| Fixed callback adapter | Heap capture/render intermediary buffers in `ma_device__on_data` | Defaults on; `noFixedSizedCallback=MA_TRUE` bypasses accumulation/rechunking. |
| User routing queue/controller | AudioHole responsibility | Required isolation/drift mechanism for arbitrary clocks; size according to measured jitter. |
| Render converter | Client→native rate/format/layout | Bypass mathematical conversion when compatible; compatibility does not eliminate drift SRC. |
| Render stack scratch→endpoint copy | `ma_device_write__wasapi` maps/copies/releases | Remains in stock blocking path even when converter is bypassed. |
| Async duplex ring | `ma_duplex_rb`, callback backend helper | **Not** selected by stock WASAPI; do not include its priming as Windows duplex latency. |

Pure copies and dispatch add execution time but do not automatically add a period of residence time. An accumulation buffer waiting to fill does. Queue capacity is a maximum, not observed occupancy. Therefore “three periods requested” cannot alone establish total latency or attributable overhead.

## Fixed callback adapter

**Verified.** Initialization zeroes config; `noFixedSizedCallback` defaults false. Adapter capacity is the requested frame period, else `floor(ms*clientRate/1000)`, else internal period. Duplex forces playback adapter capacity equal to capture adapter capacity. Capture intermediary memory is silenced and its initial length set **full**, whereas playback starts empty. This affects startup callback/silence behavior; exclude startup transient or record it separately in measurement. Adapter copies capture until full, fires data callback, caches playback and serves it in pieces. [G2, G3]

For frame capacity C at client rate F, capture accumulation can wait almost `C/F` seconds for earliest samples depending on incoming chunk alignment; callback output can wait in playback's adapter until consumed. These bounds are not additive constants in all schedules. At C=480,F=48000 the block duration is 10 ms; actual residence time must be tracked. Disabling fixed sizing lets callbacks vary and removes this adapter, but WASAPI read/write batching and scratch remain. Callback code must accept actual frameCount and fill all requested output.

Output is pre-silenced by default; f32 output is clipped by default; master volume 1 avoids scaling. `noPreSilencedOutputBuffer`, `noClip` and variable callbacks can remove passes if the application supplies equivalent contracts. Do not turn off initialization/clip safety without full output writes and finite/range guarantees. [G2]

## Ring buffer computation and synchronization

`ma_rb` is a byte-oriented single-producer/single-consumer ring. `ma_pcm_rb` wraps it in format/channels and frame-to-byte conversion. Read/write offsets use 32-bit atomics and a top-bit lap flag to distinguish full from empty. Capacity per subbuffer is restricted below 2^31 bytes and allocations are SIMD-aligned; caller-owned preallocation is supported. This is not a general multi-producer/multi-consumer queue. [B1]

Acquire returns at most the requested **contiguous** region, bounded by wrap or the other pointer. Commit advances by processed bytes, resetting position and toggling lap on the physical end. An acquire/commit caller may need two spans at wrap. Available-read computes occupancy from the two encoded positions/laps; available-write is remaining capacity. Operation bounds and ownership discipline matter: a commit's local end-of-buffer check does not validate arbitrary misuse against another thread's last acquire. One producer publishes samples before write commit; consumer commits only bytes actually read.

No built-in blocking, drift servo, overwrite-on-full or autonomous underrun policy is supplied by the ring itself. No frames available can be a successful zero-size acquire, so AudioHole must compare actual size with demand and record losses. Reset both sides only with appropriate external quiescence; public reset does not make concurrent teardown safe. [B1]

**Verified helper behavior, not Windows buffering:** `ma_duplex_rb_init` sizes from five capture internal periods rescaled to client rate through `ma_calculate_frame_count_after_resampling`; primes by `seek_write(captureInternalPeriod*2)` without rescaling that priming count. At equal 48 kHz/480-frame capture period, capacity=2400 frames and initial readable=960 (20 ms of zeroed allocation). At 48 kHz client/44.1 kHz capture with 441-frame period, observed capacity=2400 and priming=882 client frames (18.375 ms), exposing the mixed-unit priming convention. The source helper count calculation includes a conditional extra output frame for unequal rates; it is not universally a simple ceil. No automatic controller changes these tuning values. [B2, B3, E1]

## Allocation and real-time reliability

Steady-state built-in conversion/read/write predominantly uses preallocated state and 4096-byte stack arrays. Four such arrays occur in the generic duplex conversion loop, with additional helper stack storage possible. At stereo f32 a 4096-byte scratch holds 512 frames; at 8 channels f32 it holds 128. A 480-frame period at 8 channels therefore splits callbacks/processing unless an adapter recombines it. These are buffer capacity calculations, not measured deadline costs. [G1, G2]

Device/converter/adapter/ring setup allocates; `ma_device__post_init_setup` frees/recreates converters and can realloc playback input cache on reinitialization. Recovery/notification code also takes locks, stops/starts devices and activates COM objects. It is false to call the entire lifecycle allocation-free or lock-free because steady-state PCM processing avoids general heap allocation. Error-path logging may run from audio/control contexts. A custom log callback must enqueue bounded records, not format/write files or block. [G3, D3, W4]

## Latency budget and attribution

Track separately:

1. SRC history/filter group delay: library helper count plus frequency-dependent phase, or measured impulse response.
2. Software residence: adapter/route queue occupancy divided by its frame rate.
3. Endpoint software buffering: render padding/capture backlog and negotiated capacity; do not count the same residence twice.
4. Windows engine processing and any Windows SRC/APOs.
5. Driver scheduling/transport.
6. Hardware ADC/DAC and internal DSP.
7. Wireless/network/external transport, where present.

A useful route budget is `L_route ≈ L_capture_wait + q/Fsource + L_SRC + L_render_wait + Cprocessing`, with each term's ownership and overlap recorded. The 10 ms target is AudioHole-attributable and conditional; report paired baseline subtraction and uncertainty rather than declaring total loopback latency attributable to AudioHole. Small nominal settings and Client3 success do not establish the target.

**Disposition:** Adopt explicit capacity/occupancy and requested/negotiated reporting; adapt variable-block processing and preallocated SPSC utilities; investigate actual copy/conversion cost before specializing native zero-copy. Reusing one capture packet directly for a destination cannot generally avoid lifetime, scheduling and independent-clock buffering requirements.
