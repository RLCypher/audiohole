# Scheduling and processing

## Cadence selection

**Verified:** `impl-node.c:insert_driver()` orders nodes by descending driver priority, with a lazy/request preference in zero-priority cases. `pw_context_recalc_graph()` groups connected runnable nodes, assigns drivers, collects rate/quantum/latency constraints, handles forces/locks, and starts followers before their driver. This is control-plane graph recalculation on topology/property changes. [S09–S10](SOURCE_REFERENCES.md#s09)

When free to choose quantum, it starts from the default or a requested latency translated to frames, clamps to per-node min/max and global floor/limit, and optionally floors to a power of two unless forced. Maximum-latency requests can tighten the upper constraint. Rates are selected from allowed rates with gcd/nearest-rate heuristics and limits on upsampling. Do not reduce this policy to "always use the smallest period" or assume it continuously estimates CPU slack. [S10, lines 1423–1494, 1666–1868.]

**Verified compiled defaults:** 48 kHz, quantum 1024, usual min/max 32/2048, hard floor/limit 4/8192, and power-of-two preference true. Configuration and distribution overrides can change these; comments in `pipewire.conf.in` are corroborated by `settings.c` definitions. [S13](SOURCE_REFERENCES.md#s13)

**Experimental Windows adaptation:** WASAPI supported periods are API/format constrained and need not be powers of two. Select within reported constraints and measured stability; do not copy PipeWire's power-of-two policy onto `IAudioClient3` fundamental-period multiples. [W05–W07](SOURCE_REFERENCES.md#w05)

## Per-cycle ordering and deadlines

**Verified:** activation records track required/pending dependencies and statuses. A new cycle resets pending from required; completion decrements targets, and `trigger_target_v1()` writes eventfd when pending reaches zero and a CAS advances trigger status. The driver starts processing and tracks completion of the group. Unfinished prior cycles are marked/reported as xruns. Remote clients may signal peers directly using transferred activation/eventfd resources. [S09](SOURCE_REFERENCES.md#s09), [S11](SOURCE_REFERENCES.md#s11), [S17](SOURCE_REFERENCES.md#s17)

This is dependency ordering rather than a global earliest-deadline-first scheduler. A node's OS priority and graph dependency eligibility are different concepts. Deadline feasibility depends on the critical path, parallel work placement, event handoff, and scheduler delay. There is no hard real-time guarantee demonstrated by the presence of eventfd or SCHED_FIFO alone.

The ALSA timer path calculates a logical next wake-up `T/(cF)` ahead, while its IRQ path responds to device readiness thresholds. Synchronization runs before graph dispatch; follower capture can defer its read until the resampler computes the size required. [S02, lines 2506–2518, 2943–2952, 3541–3578, 3620–3658.]

**Inferred budget:** with period `P`, budget execution against `P−wake_lateness−handoff_cost−safety_margin`. An asynchronous queue can isolate execution jitter but adds buffered age; `spa_io_async_buffers` explicitly separates writer and reader cycles. Removing a dependency wait is not free latency reduction. [S03, lines 355–358; S09.]

## Threads, memory, and priority

**Verified:** data-loop implementation delegates event processing to a loop thread with thread utility hooks. `module-rt` requests Linux real-time scheduling using SCHED_FIFO and can fall back to portal/RTKit facilities depending on configuration and permissions. It considers scheduling resource limits; it is not equivalent to setting an arbitrary Windows process priority to realtime. [S14](SOURCE_REFERENCES.md#s14)

Link/converter initialization prepares reusable memory. That useful pattern does not prove every PipeWire path is strictly allocation-free or nonblocking: the inspected ALSA availability recovery can enter one-second suspend waits. AudioHole's existing callback restrictions are stricter than blindly copying all upstream behavior. [S02, S06, S12.]

## Applicability to a few independent routes

**Experimental:** begin with event-driven WASAPI workers registered appropriately with MMCSS; preallocate audio/ASRC state, keep format negotiation and endpoint recreation off workers, and publish control changes at safe boundaries. Compare one worker per direction against a worker servicing compatible endpoint events; maximum 8 duplex holes can imply more streams/workers than the logical-hole count. No worker count is selected without timing/CPU evidence.

Keep independent destination deadlines and recovery boundaries. Use immutable/prepared command payloads or bounded message passing for lifecycle requests. Do not block an audio worker on a GUI mutex, diagnostic writer, or another route's recovery. Check the cost of event waits, pointer updates, format conversion, resampling, and cross-thread copies together.

Adopt measured deadline/xrun counters, late-wake histograms, callback execution percentiles and maximums, and queue depths. Evaluate CPU with and without active diagnostics and under load. Avoid a PipeWire-like server/client graph, remote shared-memory signaling, or dependency-counter scheduler unless later requirements demonstrate a benefit. Those are scalability/framework choices, not prerequisites for source-to-destination routing.
