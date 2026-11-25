# Timing, threading and scheduling

Pinned revision: [README](README.md). **Verified source** except proposed instrumentation. References [W1, W4, G1, T1, D3](SOURCE_REFERENCES.md).

## Thread roles

- Context WASAPI command thread (normal priority) obtains render/capture services via a protected command queue and completion events. COM initialization/service acquisition does not recur for every buffer.
- One worker per synchronous `ma_device` owns start → processing loop → stop. It initializes COM on Windows, waits on lifecycle events, starts backend clients, signals start completion, runs generic read/write loop, stops backend and signals completion. A duplex object uses that same worker for capture and render.
- OS endpoint notifications enter separate callbacks; this revision can stop/reinitialize/restart inside those callbacks. This must not be confused with the data callback thread, or with a fully deferred recovery design.
- Application controller/supervisor responsibilities remain AudioHole's. Multiple routes/devices multiply workers; actual counts depend on chosen sharing topology.

The ordinary Win32 thread wrapper calls `CreateThread` and `SetThreadPriority`; the enum value for normal is −2, highest/default is 0, and realtime is 1. Realtime maps to `THREAD_PRIORITY_TIME_CRITICAL`. `ma_context_config_init` zero-initializes, so the default processing worker requests `THREAD_PRIORITY_HIGHEST`, while the context command thread explicitly requests normal. Higher priority is not an MMCSS task registration and does not reserve deadline completion. [T1]

## MMCSS is opt-in here

`ma_device_start__wasapi_nolock` checks dynamic `avrt.dll` availability and `ma_to_usage_string__wasapi`:

| Usage | Task passed to AvSetMmThreadCharacteristicsA |
| --- | --- |
| default (zero) | NULL mapping; registration skipped |
| games | Games |
| pro_audio | Pro Audio |

Registration runs on the worker doing startup, so normally affects the processing thread. Stop calls `AvRevertMmThreadCharacteristics` if a handle exists. No call to `AvSetMmThreadPriority` was found on this path. A failed registration handle does not cause stream initialization to fail; inspect it through instrumentation rather than assuming task status from configuration. [T1]

[Microsoft documents](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsa) that registration associates the **calling thread** with a named configured task, returns a handle or zero, and requires revert after the task. This supports explicit configuration and observation; it is not proof any given driver and CPU load will meet deadlines.

## Wake behavior and deadlines

WASAPI capture waits when GetBuffer reports no data or temporary buffer error; render waits on insufficient space, and exclusive render waits after full-buffer submission. Auto-reset events are attached to audio clients. The default 5000 ms wait is an error/stall timeout, not a polling interval and not a deadline. Backend loopback has a 10 ms retry wait. Lifecycle wake-up sets endpoint events when stopping. Stop can drain render according to buffer duration; timeout is computed as `floor(actualBufferFrames*1000/internalRate)` ms. [W4]

For a batch N at actual rate F, ideal service interval is N/F seconds. Deadline headroom depends on current buffered frames and direction, not just callback frameCount. Example: 96 frames/48000=2 ms, so occasional multi-millisecond scheduling stalls can exhaust a minimal queue; this is arithmetic, not measured Windows jitter. Legacy `GetBufferSize()/periodCount` is an internal chunk, not an authoritative timer/engine-period measurement. Capture packet size, render capacity and Client3 engine period can differ. [W2, G1]

The generic duplex worker first waits for capture then writes render. Render backpressure can dominate average cadence, but a source stall can delay the user callback and output. An application callback must not call blocking lifecycle APIs on the same device. Offload device events and diagnostic formatting to other threads with bounded queues.

## Timing information not collected

`GetCurrentPadding` appears in the available-frame helper/stop drain logic; it is not a stream-wide drift estimator. Capture `GetBuffer` requests NULL position and NULL QPC position. No IAudioClock-based synchronization was found. Raw Windows timing is therefore not delivered in the data callback contract, which receives buffers and frame count. [W4, T1]

[Windows capture documentation](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer) supplies frame position and QPC time in 100 ns units and flags validity problems. [IAudioClock](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition) position must be interpreted with its reported frequency: seconds=p/f; it is not automatically a frame count. These are useful native instrumentation options, not capabilities already exposed by the traced miniaudio data path.

## Proposed instrumentation

**Experimental.** Preallocate a binary record ring. Per processing iteration record QPC start/end, frame demand, produced/consumed counts, queue occupancy, discontinuity flag and error code. Optional timestamp pairs must retain their units, stream reset generation and validity. A non-real-time consumer exports records and computes wake lateness, callback p50/p95/p99/max, missed deadline count and CPU time/frame. ETW/WPR can correlate scheduling and driver events. Registration and native period choices must be logged once off the audio thread.

Compare identical MMCSS policy and ordinary priority in backend benchmarks. A direct backend tested with Pro Audio cannot fairly be compared to stock miniaudio defaults and attributed solely to library overhead. Likewise use identical real queue/controller behavior.

**Disposition:** Adapt explicit scheduling policy and tracing; investigate loaded-system behavior. Do not copy the blocking notification/reinit pattern as a recommended Windows lifecycle design.
