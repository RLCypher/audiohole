# AudioHole engine module and standalone validation host

The owner's subsequent integrated
[processing/queue overhead increment](../docs/AUDIO_OVERHEAD_IMPROVEMENTS_2026-11-15.md)
adds optimized kernels, bounded Automatic queue recovery and an opt-in CPU
benchmark. It also reduces host/UI diagnostic work. The module's original
validation-host scope below remains historical context.

This increment lives entirely under `engine/`. It does not modify or link the
GUI, GUI controller, simulator, profile builder, personalities or tray. The
parent repository baseline is `fda20900e8954993e267e2ca1d6d7be8a19840dc` (`work`).

This is an **initial routing validation prototype**, not a production release.
Portable tests validate configuration, sample handling and lifecycle logic.
Windows cross-compilation checks buildability. Live Windows routing, latency,
quality, long-running synchronization and device compatibility need hardware
validation. No automated result substitutes for those measurements.

The [first 0.1 hardware-run review](LIVE_RUN_REVIEW_2026-11-11.md) records successful
native/shared duplex initialization and clean audible Stop, with zero forward
FIFO underruns but periodic reverse underruns. Real 0.2 runs include one with
49 reverse underruns and another with none in either direction; see their
reviews. Version 0.3 adds an opt-in native low-latency comparison preset and
timing/padding diagnostics. The wired comparison reports 148 ms Low versus
168 ms baseline and reveals a startup full-refill increasing baseline's FIFO
target to 32 ms. Version 0.4 addresses that reproduced priming case and adds
last-underrun context. [Six initial 0.4 hardware runs](LIVE_RUN_REVIEW_2026-11-11_V04_BACKENDS.md)
verify startup accounting, while catching starvation in Experimental Standard
and the Low-labelled Miniaudio run. Longer/varied hardware testing remains needed. See
[changes](CHANGES.md) and [verification scope](VALIDATION.md).

Version 0.5 implements the owner's current [profile-v3 contract](PROFILE_V3_HANDOFF.md):
independent direction flags, explicit Exclusive preference with auditable Shared
fallback, and strict forced-mode errors. Preferred clock selection remains metadata.

Version 0.6 addresses the zero-transfer startup and near-capture starvation
conditions found in the real 0.5.1 runs. It adds bounded startup failure/reporting,
one pre-transfer Shared retry for eligible Exclusive preferences, fair native
render-event handling, and a short Shared capture wait using existing playback
headroom. See [the repair and hardware test guide](STARTUP_PROGRESS_TEST.md).
Software regression results do not establish a fix on the user's devices.

## Run the Windows validation application

1. Extract the validation ZIP into a writable folder.
2. Put an individual `audiohole-profile` record, current version 4, into its `Profile`
   folder. The host never hardcodes a route or imports demonstration devices.
3. Run `audiohole-engine-validation.exe`.
4. Use `list`, then `select 1` (or another listed number), then `start`.
5. Use `status` to inspect settings and actual counters; `stop` requests cleanup.
   Wait for `Stopped` before another Start. `quit` or Ctrl+C stops and exits.

For the latency comparison, use `Run-Low-Latency.cmd` or launch the executable
with `--low-latency`. The ordinary executable and `Run-Validation.cmd` retain
baseline tuning. See [the comparison guide](LOW_LATENCY_TEST.md) for the exact
changes and what to record. No profile/schema or GUI changes are required.

For 0.4, start with [the startup-buffer test](STARTUP_BUFFER_TEST.md): repeat the
same wired profile for about 15 minutes per preset and take readings after the
initial settling period. The baseline preset retains default-period/full-lead
scheduling but now includes the startup fix, so use the preserved 0.3 package
when an exact old behavior comparison is needed.

Start routes actual capture audio to the explicitly assigned playback endpoints.
The host changes no device/master volume and plays no synthetic test signal.
Explicit Exclusive access or an enabled Exclusive preference can reserve an
endpoint and affect other audio. Without a preference or effective forced mode,
Automatic remains Shared. All actual access modes and attempt receipts are reported.

The host reads the GUI's existing scanner inventory and local bindings **without
writing them**. Default Windows locations are:

```
%LOCALAPPDATA%/AudioHole/PreviewBuild/test
%LOCALAPPDATA%/AudioHole/PreviewBuild/test-local
```

These follow the current GUI's storage arrangement, not a permanent storage-root
decision. If your build stores scanner data elsewhere, run:

```
audiohole-engine-validation.exe --inventory "PATH-TO-TEST" --local "PATH-TO-TEST-LOCAL"
```

`--profiles PATH` selects another profile folder. Logs remain beside the validation
executable unless `--results PATH` explicitly selects another output directory.
Selecting GUI profile files does not redirect logs into GUI data.
`--check FILE` validates and
prepares a profile without opening devices or requiring an inventory. The check
also works in portable builds. Missing/ambiguous bindings never fall back to
friendly names, default Windows devices, guessed IDs or simulated endpoints.
Native opening rechecks endpoint state/direction; stale bindings can fail and
must be refreshed through the existing scanner. No scanner is run automatically.

## Configuration contract

The parser consumes the current [profile-v3 handoff](PROFILE_V3_HANDOFF.md).
The parent PROFILE_BUILDER.md is a historical baseline, superseded for this
module by the owner's schema update. Individual v1/v2 profiles remain read-only
loading-compatible. Version-7 aggregate transfers are separate GUI import/export
payloads and are rejected as engine input.
It is a read-only projection, not a replacement serializer: it never changes
filenames, IDs, notes or extensions in the original record. The owning host
retains the original JSON. Legacy aggregate migration belongs to the builder.

- Only `settings.preferences` imposes audio requirements. Legacy projection
  fields are ignored even when current preferences are omitted or null.
- With a direction's `advanced_tuning` off, its saved audio overrides and maps
  are retained but inactive. Its sibling direction is independent. Hole backend
  selection and direction `exclusive_mode_preferred` remain independently active.
- With overrides on, each endpoint property resolves endpoint → common →
  Automatic. Missing fields, null, empty string and `Automatic` mean inherit;
  buffer zero also means inherit. Explicit rates/channels/modes/formats are
  requirements. Invalid types/ranges reject separately.
- Access modes: `Shared`, `Exclusive`; sample formats: `float32`, `pcm_s16`,
  `pcm_s24_packed`, `pcm_s24_in_32`, `pcm_s32`. Recognizing a format in the schema
  does not promise hardware or adapter support.
- Effective forced Shared/Exclusive wins over preference. Otherwise a true
  `exclusive_mode_preferred` tries Exclusive first, then Shared after stopped
  acquisition/requirement failure or failed pre-transfer startup, preserving explicit
  format/rate/channel requirements and the supplied buffer request. A required Exclusive failure is never downgraded. The policy and
  per-endpoint attempt history are available to a future GUI host and the logs.
- Integration amendment: `buffer_ms` is an explicit API buffer request. The
  supplied request must be confirmed unchanged, while device/library alignment
  may produce a different allocation. Receipts retain actual positive frames and
  their rate; diagnostics show requested and allocated durations separately.
  Formats, rates, channels and forced access modes remain strict requirements.
  Missing or changed request receipts reject before Start. Automatic permits
  negotiated allocation. A scheduling period and an end-to-end latency limit
  are different concepts; neither is encoded by this field.
- An empty map duplicates a mono source into the first playback pair when the
  destination has at least two channels. Otherwise it connects matching indexes
  up to the smaller width. Extra outputs stay silent and unmatched inputs are
  discarded; stereo remains separate left/right. Explicit `{source, destination}` pairs
  use zero-based indexes, allow one source to feed several outputs, and reject
  duplicate destination assignments. Negotiated widths are rechecked.
- Enabled holes must have assignments; saved drafts can be incomplete. Up to
  eight enabled holes / sixteen paths are admitted; a duplex hole counts once.

## Backend choices in this validation build

| Selector | Actual implementation |
|---|---|
| `automatic` | Available native WASAPI comparison baseline, otherwise available miniaudio. Experimental is excluded. This initial selector does not retry a failed open through a different backend. |
| `wasapi` | Native event-driven capture/render and the shared prototype processing pipeline. |
| `miniaudio` | Separate WASAPI-only capture and playback devices, with the same queue/controller pipeline; no stock blocking duplex helper. |
| `experimental` | Native WASAPI I/O with an alternate occupancy DLL controller informed by the PipeWire research. |

Explicit selection never substitutes another implementation. The same selection
applies to both directions of a hole. Different holes can select different
adapters. Portable builds report all real Windows adapters unavailable; there is
no runtime simulated-device fallback. The restricted miniaudio adapter can be
disabled at build time.

Miniaudio is pinned to researched 0.11.25 / commit
`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`, archive SHA-256
`1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55`.
It is a validation dependency, not final selection under D-030/D-034. The pinned
research demonstrates converter limitations: this adapter rejects packed-s24 and
24-in-32 client requests, and packed-s24 native conversion paths. Use native
WASAPI to attempt those formats. The library's coarse dynamic-ratio setter and
phase-update APIs are not used. Decoding, media-engine and resource-manager
features are disabled. Full upstream license text accompanies binary packages;
MIT-0 is the selected upstream alternative for this prototype.

## Processing, clocks and evidence

Both I/O implementations feed a preallocated SPSC source queue. Rendering is
destination-paced. Interleaved samples are converted to float32, mapped and
linearly resampled with a double fractional phase accumulator. Integer output is
bounded to its encoding range. This is neither bit-perfect nor a validated
high-quality resampler, especially for large nominal rate changes.

Baseline synchronization is a bounded prototype occupancy PI controller;
Experimental uses an alternate second-order DLL. Queue targets initially cover
two endpoint blocks, at least 20 ms in baseline (5-ms floor in native Low), with
at least 200-ms capacity and a 16-MiB
per-route sample-storage limit. Larger reported blocks increase initial target/capacity.
Version 0.2 also reserves one actual observed capture packet plus one render block
and interpolation lookahead. The active target is reported separately from the
initial target. Queue allocation stays fixed; an unsafe observed block reports an
explicit capacity error rather than allocating on an audio callback.
Its baseline PI uses a critically damped ideal-model relation and anti-windup;
priming is excluded from clock-error estimation. This does not establish physical
clock lock, converter quality or optimal controller tuning.
Gains, correction bounds, priming and the one-second progress-freshness diagnostic
are **test settings, not accepted production thresholds or a latency promise**.
Approximately 10-ms attributable latency is not established by this prototype.

The PipeWire research informs the design; no PipeWire source or runtime is
incorporated. No superiority of either controller or backend has been measured.
Native capture packet position/QPC are obtained but are not yet used in a clock
estimator; correction ppm is an actuator command, not measured hardware drift.
Version 0.3 uses valid capture QPC to report last/max first-frame packet age at
receipt, plus capture drain gaps/burst sizes and shared render padding. These
aggregates do not measure end-to-end latency. Padding after submission is an
estimate from prior padding plus submitted frames, not a second Windows query;
zero-padding wake counts do not independently establish hardware underruns.

Version 0.4 bounds a startup-only automatic Shared PCM request to one accepted
period and fills any surplus native request with counted prefix silence, until
the first source transfer. Genuine running large requests retain the full FIFO
guard. Native preroll/prefix/submission counters expose the distinction; see the
startup test guide for their accounting. No new silence is inserted into normal
flow by this change. Explicit endpoint buffers and Exclusive remain unchanged.

The pipeline publishes starvation context through a fixed eight-slot SPSC mailbox
to one non-RT observer. A full mailbox drops new diagnostic events, with a counter;
it never stalls sample processing. The observer retains the last event for host
snapshots. Logs include frame indexes, block/queue sizes and native render-entry
timing where supplied. This is not a complete event history or measured physical
clock drift. Slow host snapshots may skip intermediate events.

Status distinguishes captured, rendered and transferred frames. Priming/starved
output is silence; it does not increase transferred frames. Silence from the
source is valid audio. Overflow drops new packets, underrun silences/re-primes,
and counters expose both. Running indicates observed buffer processing, not
audibility, remote-link health, clock lock or acceptable quality. Stale progress
is marked Degraded. Native discontinuities are counted; miniaudio does not expose
equivalent packet flags here, so that counter is unavailable for its adapter.

Every Start produces bounded `Results/*.jsonl` and `.txt` logs, recording the
selected original profile, actual settings, counters, UTC time and monotonic
elapsed milliseconds. Logs contain no samples and are never uploaded. They can
contain profile names/notes and logical IDs. A logging failure during admission
requests Stop; hitting the log-size bound reports it while routing continues.

## Module integration API

Link `audiohole::engine` from a future host. `audiohole::engine_contract` is the
smaller parser/planner/mapping library. Public headers are under
`include/audiohole/engine/` and contain no GUI/scanner/JSON dependencies.

```
parse_profile(bytes)                       -> Result<Profile>
prepare_profile(profile)                   -> Result<PreparedProfile>
bind_route(request, host_binding_snapshot) -> Result<BoundRoute>
Runtime::start(profile, bindings)           -> operation receipt
Runtime::stop()                            -> cancellation request
Runtime::snapshot()                        -> owned state/settings/counters/issues
```

Runtime commands/snapshots belong to the constructing control thread and must
never run on an audio callback. Admission is distinct from operation success.
The runtime owns its lifecycle worker; native sessions own STA workers and release
COM on those same threads. Miniaudio owns its separate device workers. No callback
allocates, parses JSON, logs, takes a mutex, touches a widget or performs teardown.
Control snapshots and logging can allocate/block and stay outside that path.
The registry reader is linked only into the validation host, not into the engine.

Stop has precedence over pending Start/results, is idempotent, and publishes
Stopped only after stream/worker cleanup. Destroying Runtime also cancels/joins.
Driver calls cannot be forcibly cancelled safely; there is no detached-thread or
hard cleanup-timeout guarantee. Use Quit/Ctrl+C for normal cleanup; forced process
termination cannot promise a terminal log entry.

This validation host requires Stop/Start for edits, rebindings and recovery. It
does not implement transactional live profile switching, automatic reconnection,
shared capture reuse, Test Beep or GUI integration. Multiple routes into one
destination reject rather than silently mix. Healthy sibling paths continue
after a path failure. No existing GUI/controller behavior has been replaced.

## Build and verification

From the repository root:

```
cmake -S engine -B engine/build -DBUILD_TESTING=ON
cmake --build engine/build --config Release
ctest --test-dir engine/build -C Release --output-on-failure
```

Use a Windows x64 toolchain for real I/O. Portable builds exercise contracts and
processing only. `AUDIOHOLE_ENGINE_WITH_MINIAUDIO=OFF` removes that adapter;
`AUDIOHOLE_ENGINE_BUILD_VALIDATION=OFF` builds the module without the scanner host
adapter. The existing application build need not include this directory.

Tests use injected software fixtures strictly for unavailable hardware-independent
logic: field precedence, override retention, invalid inputs, backend selection,
binding ambiguity, PCM bounds, channel mapping, SPSC ownership, command admission,
failure/Stop and cancellation during opening. Fixtures are never linked into
the validation application or reported as hardware evidence.
