# PipeWire research for AudioHole

Research date: 2026-10-25 (America/Los_Angeles). Informational work only.

The original 18,840-byte assignment is preserved byte-for-byte as the prefix of [AGENTS.md](AGENTS.md), followed by the owner's GitHub publication policy. All research work remains inside this directory. The dedicated publication branch is `research/pipewire`, targeting remote `work`; the shared local checkout is not switched. No production dependencies, engine edits, or changes to baseline documents are included.

## Reproducible baseline

- Initial AudioHole comparison: `a2aaf7a8ca2b14c3ca37b59cf6d8bc255124a984`, a 14-document snapshot. Publication is based on current remote `work`, `766ed88f7d7786bfb90fb7a520da34a15de010d1`, which includes foundation/simulated GUI code and newer synchronization guidance. Read [CURRENT_BASELINE.md](CURRENT_BASELINE.md) first for current applicability. No hardware behavior is inferred from either baseline's plans or simulated UI.
- PipeWire: release tag **1.4.9**, commit **`fd60e04525f3a04d90bf50085222e0cc9139b4a4`**, official [upstream repository](https://github.com/PipeWire/pipewire/tree/fd60e04525f3a04d90bf50085222e0cc9139b4a4). This is a deliberately fixed release baseline, not a claim about the latest release.
- [upstream/MANIFEST.json](upstream/MANIFEST.json) records exact source/blob references. Downloaded `upstream/pipewire/` and `upstream/windows/` snapshots are ignored local caches and are **not published**. The GitHub connector retrieved the originally inspected files after the shell clone failed at its proxy connection. An optional retrieval script verifies reference hashes for reproduction.
- Windows mappings use saved Microsoft SDK documentation, with individual blob hashes. Its `docs` branch was moving at retrieval; these documents are not claimed to be from one common commit.

## Reading order

1. [FINDINGS.md](FINDINGS.md): conclusions, evidence, and open questions.
2. [ARCHITECTURE.md](ARCHITECTURE.md): AudioHole baseline and the verified component/data flow.
3. [CLOCK_SYNC.md](CLOCK_SYNC.md): clock identity, observation, DLL, bandwidth, and recovery.
4. [RESAMPLING.md](RESAMPLING.md): correction plumbing, phase, filters, and quantization.
5. [ALGORITHMS.md](ALGORITHMS.md): equations, units, tuning provenance, and numerical risks.
6. [BUFFERING.md](BUFFERING.md), [SCHEDULING.md](SCHEDULING.md): bounded transport and deadlines.
7. [WINDOWS_APPLICABILITY.md](WINDOWS_APPLICABILITY.md): Windows API contracts and proposed adaptations.
8. [RECOMMENDATIONS.md](RECOMMENDATIONS.md): Adopt / Adapt / Investigate Further / Reject decisions for consideration.
9. [EXPERIMENTS.md](EXPERIMENTS.md): executed model checks and proposed hardware validation.
10. [SOURCE_REFERENCES.md](SOURCE_REFERENCES.md), [LICENSING.md](LICENSING.md): evidence and reuse boundaries.

[PUBLICATION.md](PUBLICATION.md) records branch scope and the ongoing publication workflow. Downloaded source, scratch files, caches, and build binaries are excluded; authored experiment scripts and small numerical result files are retained.

## Evidence conventions

**Verified** means directly supported by the pinned source, saved official documentation, or an explicitly described reproducible check. **Inferred** means a deduction with stated assumptions. **Experimental** identifies an unvalidated AudioHole proposal or a simulation. Source references use identifiers S01–S19 and W01–W09, resolved in SOURCE_REFERENCES.md. A section's reference applies to its stated verified facts; derived equations and proposals are labeled separately.

This research reconstructs the ALSA PCM → audioadapter → native audioconvert path in depth. It does not establish that every PipeWire backend uses the same controller, buffer policy, or rate actuator. No PipeWire daemon or audio hardware was exercised, and no Windows latency, CPU, audibility, or compatibility results are available.

## Local reproduction

From `research/pipewire/`:

```sh
python3 experiments/verify_research.py
python3 experiments/fetch_references.py --probe-only
mkdir -p experiments/build
cc -std=gnu11 -O2 -Wall -Wextra -Werror -I upstream/pipewire/spa/include experiments/dll_probe.c -lm -o experiments/build/dll_probe
./experiments/build/dll_probe > experiments/dll_results.csv
python3 experiments/numerics.py > experiments/numerics.json
```

The harness calls the unchanged upstream DLL and ring buffer headers. It models queue dynamics and rate quantization, not a complete PipeWire device/resampler. It introduces no dependency into AudioHole. NumPy is optional for the separate eigenvalue calculation; the C checks and manifest verification do not need it.

Reference retrieval needs Internet access unless valid files are already cached. Use `--require-reference-snapshots` with the checker only after retrieving the complete optional 49-file cache. Missing caches are reported explicitly and never described as verified. Compile output stays in the ignored `experiments/build/` directory.
