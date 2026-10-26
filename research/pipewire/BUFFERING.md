# Buffering and latency

## Capacity, occupancy, and processing period are different

**Verified:** ALSA negotiates hardware period/capacity through driver constraints, while graph threshold is `ceil(graph_duration×node_rate/graph_rate)`. Target occupied/readable frames are selected separately and clamped. Default period is 1024 at 48 kHz, scaled for rate; batch/timer/IRQ paths change the negotiation. IRQ defaults to three periods for a documented FireWire workaround; timer mode can request as many periods as supported while keeping only a smaller working delay. Capacity does not imply every frame is queued. [S02](SOURCE_REFERENCES.md#s02), lines 2322–2400, 2826–2854, 3005–3045.

`recalc_headroom()` starts with configured headroom or optionally minimum hardware period. Timer/resampling paths add one period for batch devices and enforce at least 32 frames for capture jitter. It caps headroom at `buffer_frames−threshold` when capacity permits. Targets are occupancy-dependent and hardware/configuration-dependent; the examined code does not establish a universal rule to grow all buffers after every xrun. [S02, lines 2034–2068.]

**Inferred AudioHole application:** reserve capacity to survive unusual scheduling gaps while maintaining a smaller target occupancy consistent with the latency budget. Pick target from observed event/packet cadence and bounded jitter, not simply half the allocated capacity or the second-smallest driver buffer.

## Allocation and reuse

**Verified:** `impl-link.c:do_allocation()` negotiates buffers when ports are ready and the link enters ALLOCATING. It reuses existing output allocations when possible and passes the same buffer array to both ports. `buffers.c` lays out buffer skeletons/data, optionally in shared memory. `audioadapter.c` separately negotiates hardware/converter buffers. This is setup-time resource preparation, not a request to allocate sample storage on every process cycle. The native resampler allocates aligned filter/history memory at initialization and uses bounded history copying during processing. [S04](SOURCE_REFERENCES.md#s04), [S06](SOURCE_REFERENCES.md#s06), [S12](SOURCE_REFERENCES.md#s12)

Shared buffers reduce copies at compatible links, but the examined source does not justify calling the entire route zero-copy: format conversion, FIR work, history, and ALSA transfer remain. Memory-sharing/IPC features support PipeWire's broader service architecture. A small in-process AudioHole route can use simpler ownership without reproducing that machinery.

## Bounded SPSC transport

**Verified:** `spa_ringbuffer` holds only uint32 read/write indices. Reader acquires writer progress, copies data, then release-stores its read position; writer does the converse. Each side reads its own position relaxed. Fill is `(int32_t)(write−read)` using wrapping counters. Copy helpers split wraparound into at most two `memcpy` calls. They do not enforce capacity, prevent under/overflow, or implement an MPMC queue. [S07](SOURCE_REFERENCES.md#s07)

**Experimental AudioHole invariants:**

1. Preallocate capacity in **frames**, record format/block alignment, and validate byte conversions for overflow.
2. One producer and one consumer per ring; owner writes its own cursor only. Publish data before cursor progress with release/acquire ordering. Never reset live cursors concurrently.
3. Enforce `0≤available≤capacity`, bound each read/write, and maintain modular-distance assumptions. Check lock-free index atomics for the target compiler/platform.
4. A mirror needs an independent read cursor per destination or separate SPSC queues; all readers must retain samples until they are done. Slow consumers must not block the shared capture producer indefinitely.
5. Never make space by letting the producer overwrite the consumer's cursor without a specifically designed protocol. Overflow/re-prime must coordinate ownership.
6. Log compact counters/events nonblockingly; format and persist outside the audio path. Queue full must drop diagnostics rather than stall audio.

## Underruns, overruns, and re-prime

**Verified:** ALSA detects negative availability and calls recovery; follower resync can discard capture frames, rewind queued playback, or insert silence, then resets the DLL. Start preparation primes silence and resets buffer ownership. These actions can change sample continuity. Suspend recovery includes blocking waits and must not be copied into an AudioHole callback. [S02, lines 2608–2751, 3049–3104, 3313–3375.]

**Experimental:** distinguish mild occupancy error, deadline miss, queue boundary violation, invalid timestamps, and endpoint loss. Healthy drift uses ASRC; a large discontinuity records missing/inserted/discarded frame counts, resets the appropriate timing/filter history, and primes a new target. The supervisor recreates endpoints outside audio processing and cancels retries after explicit Stop. A generation tag rejects stale callbacks. A healthy reverse duplex direction should survive where requirements permit. No particular crossfade or re-prime duration is selected by this research.

## Latency accounting

**Verified:** rate-match delay is reported in node frames plus a fractional-frame term; native base delay is `N/2−1` input frames; asynchronous IO buffers alternate writer `(cycle+1)&1` and reader `cycle&1`, giving a one-cycle boundary when used. SPA latency parameters support quantum, rate/frame, and nanosecond terms. [S03](SOURCE_REFERENCES.md#s03), [S06](SOURCE_REFERENCES.md#s06), [S15](SOURCE_REFERENCES.md#s15)

**Inferred budgeting model, without double counting:**

\[
L_{route}=L_{capture\,delivery}+{q_s\over F_s}+L_{ASRC}
 +L_{render\,queue}+L_{execution/phase}+L_{external}.
\]

`q_s` is the queue depth at the declared observation point, not capacity. Capture delivery/period phase may already be represented in a timestamp-derived sample age; do not add it again. Similarly, a render padding or measured sample-age value can include an existing quantum. Attribute each physical waiting stage once before reporting a sum.

At 128/48 kHz one quantum is 2.667 ms. Two **distinct** one-quantum waits plus a 31-frame FIR base delay total about 5.979 ms before any remaining device, Windows, or execution delay. This is arithmetic illustration, not a promise that a route has exactly two waits or achieves that latency. Default 1024/48 kHz is 21.333 ms per quantum and cannot simply be imported into a ≤10 ms attributable budget.

AudioHole should report requested period, negotiated period/capacity, current queue target/depth, estimated filter delay, timestamp-based observations, and externally measured loopback separately. Clock records alone do not establish acoustic end-to-end latency.
