# miniaudio research for AudioHole

Research date: 2026-10-25 (America/Los_Angeles). Informational research; no architecture or dependency adopted.

**Primary conclusion:** miniaudio supplies useful WASAPI I/O and standalone processing mechanisms, but its Windows duplex path does not supply the independent-clock controller AudioHole needs. A backend decision remains open. The most actionable evidence is the traced blocking duplex path, the low-latency shared-mode negotiation condition, and two reproduced resampler rate-update limitations, plus a packed-s24 shuffle output-extent defect.

## Read in this order

1. [Project baseline](PROJECT_BASELINE.md): requirements, actual checkout status, questions.
2. [Findings](FINDINGS.md): evidence and disposition register.
3. [WASAPI backend](WASAPI_BACKEND.md), [duplex and clocks](DUPLEX_AND_CLOCKS.md), [resampling](RESAMPLING.md).
4. [Buffering](BUFFERING.md), [scheduling](SCHEDULING.md), [algorithms and numerics](ALGORITHMS.md), [devices and recovery](DEVICE_MANAGEMENT.md).
5. [Backend comparison](BACKEND_COMPARISON.md), [recommendations](RECOMMENDATIONS.md), [experiments](EXPERIMENTS.md), [licensing](LICENSING.md).
6. [Source references](SOURCE_REFERENCES.md): pinned links, functions, Windows documentation, acquisition details.

## Evidence contract

- **Verified:** examined source/official documentation, or an explicitly identified executed software probe.
- **Inferred:** consequence of that evidence; not a hardware result.
- **Experimental:** proposed design or validation method, not an adopted implementation.

All miniaudio source references concern **0.11.25**, commit **`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`**, unless stated otherwise. All `miniaudio.h` line numbers refer to the unsplit root header at that revision. Local-only source snapshot: `upstream/miniaudio/` (excluded from publication). Hash inventory: [upstream/SNAPSHOT.json](upstream/SNAPSHOT.json).

Initial supplied baseline: `a2aaf7a8ca2b14c3ca37b59cf6d8bc255124a984` (specifications only). Publication baseline: **`766ed88f7d7786bfb90fb7a520da34a15de010d1`**, the current remote `work` head, reviewed before publication. It contains the Windows foundation and a simulated GUI/controller, but no real capture/render engine. Destination pacing, bounded FIFOs and adaptive synchronization are now accepted constraints; native WASAPI is the leading candidate, not selected. See [baseline reconciliation](PROJECT_BASELINE.md). Publication adds only this research directory to the remote base tree without switching or modifying the shared checkout, using an isolated Git tree/commit through the GitHub connector. No upstream source, build outputs or scratch files are published.

## Reproduced results and limits

[The original C probe](experiments/processing_probe.c) compiles the unmodified upstream header with device I/O disabled. [Results](experiments/processing_probe_results.txt) reproduce generic ratio quantization, phase rescaling wrap, converter bypass behavior, duplex helper sizing, and a packed-s24 shuffle contract violation. [Run instructions and all proposed hardware experiments](EXPERIMENTS.md).

No Windows machine, Windows SDK execution, audio hardware, ETW traces, latency measurement, CPU benchmark, or long-duration routing test was available. Linux software behavior is not Windows WASAPI validation. Approximately 10 ms remains a conditional project objective.

[AGENTS.md](AGENTS.md) contains the assignment verbatim, byte-verified against the upload before research: 23,719 bytes; SHA-256 `e93b191cfcb50307adbda512f93ae1d8daf433f742c083145295bee0db756c52`.
