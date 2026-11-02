# Native avatar API and lifecycle

Use `audiohole::avatar`. Public calls on `Session`, `Player` and `Panel` belong
to the GUI/host thread. Never call them from an audio callback. `Repository`
belongs to its loader thread. Returned `Image`/`ClipPtr` values are immutable
shared ownership; pixels can outlive the repository.

## Host flow

1. Initialize the platform image handler on the GUI thread.
2. Create a `Panel` owned by the GUI and a `Session(asset_root, native_decoder())`.
3. `select(personality_id, style, monotonic_ms())` clears old personality images,
   queued jobs, cooldowns and deduplication history immediately.
4. `baseline(state, revision, priority, now, invalidate_reaction)` records the
   current authoritative presentation. Revisions must increase when intent
   changes. Real operational-condition changes should set the final argument
   **true**, cancelling old reactions even when cosmetic priority was higher.
5. `react(Reaction{state, event_id, revision, occurred_ms, ttl_ms, priority}, now)`
   requests a temporary visual response. Use the currently observed revision;
   preserve event occurrence identity and timestamp where available.
6. Poll `tick(now)` from a GUI timer and pass the `Presentation` to `Panel::present`.
   A 16 ms timer samples the authored exposure timeline; it does not turn the
   artwork into 60 unique drawings per second. Delayed ticks catch up from the
   monotonic clock rather than stretching animation duration.
7. Stop the GUI timer, call `shutdown()`, then destroy the host. Shutdown cancels
   pending work, joins the loader and releases internally retained frames.

`Decision.accepted` on asynchronous reactions means queued for validation.
`last_decision()` after `tick()` reports final admission or rejection. Missing
assets produce `Presentation::diagnostics` and an owned fallback/hide; hosts
should log diagnostics once per change. The proposed presenter supplies that
change gate and an injectable diagnostic sink.

The worker never calls GUI callbacks. It processes at most one job at a time,
retains at most one pending baseline/reaction/inspection job and three completed
results, and checks cancellation between files. Generation/serial and revision
checks prevent old results from crossing personality/style or context changes.
Shutdown may wait for the current bounded local file/codec operation; it does
not forcibly terminate a thread or run a decoder on an audio thread.

## Timing and reactions

Durations come from the manifest. Looping clips use cumulative exposure bounds;
finite reactions expire to the latest installed baseline, never a saved old
baseline or an unconditional IDLE. Persistent error presentation uses an owned
static expression when its artwork is authored as a finite reaction, avoiding
continuous theatrical error loops. Persistent WORKING and CONCERNED loops can
still animate during recovery/degradation.

Baseline severity blocks less severe events. Higher priority can interrupt a
noninterruptible reaction; equal priority respects interruptibility. State
cooldowns use wall-clock monotonic time. Deduplication and cooldown histories
are bounded to 128 entries. The session rejects duplicate submitted occurrences
before decoding. Stale/future/negative-time events and old context revisions
are rejected. There is no delayed reaction queue to replay after recovery.

For deliberate lower-priority presentation changes within the same operation,
`invalidate_reaction=false` allows a current reaction to finish and then use the
latest baseline. The proposed AudioHole observer and viewer operational controls
use **true** for actual authoritative state changes.

Pause freezes visual time, not authoritative state updates or event expiry.
Speed is bounded to 0.1–4.0. Restart resets the selected timeline; Step pauses
and advances to the next authored exposure. Reduced motion uses the same
personality's corresponding static expression, or an owned frozen frame.
Manual `inspect()` bypasses event admission for the standalone art inspector;
the production GUI must not use it to react to operational events.

## Assets and fallbacks

Production defaults to approved-only. IDs are `default`, `uwu`, `vanilla`; Horu
is the character name, not a replacement configuration ID. The registry and
manifest personality must agree. Disabled variants hide; they do not borrow
chibi or another personality. Portable relative paths, canonical state/frame
filenames, anchors, declared format, SHA-256, full PNG structure, native decode,
dimensions, alpha background and transparent corners are checked.

The native loader enforces the consumed v1 runtime contract, bounded integral
metadata, supported transitions and identity/fallback rules. The retained
Python/jsonschema tools provide full authoring-schema validation. Approval
records are local metadata, not cryptographic signatures or an authenticity
service.

Fallback order is requested animation → corresponding static → owned IDLE
animation/static → last valid frame from the same session identity → hide.
A valid owned IDLE is mandatory for a valid pack/style. Runtime failures can
freeze the previously decoded owned frame. Switching personality/style clears
it, so Vanilla can never become a universal fallback. No I/O or implicit repair
occurs during playback/paint.

## Resource bounds and platform notes

JSON inputs are at most 2 MiB; operating PNGs at most 8 MiB each; clips at most
240 exposures and one hour; per-frame timing 16–60000 ms. Current accepted
frames are 512×768, 8-bit RGBA. The per-load decoded budget is 64 MiB and the
repository's retained pixel budget is 128 MiB by default. Weak caches reuse
identical owned frame hashes without retaining dead buffers. A decode may
temporarily hold one extra RGBA buffer and input/hash bytes. The wx panel retains
one scaled bitmap, capped at 4× source scale (24 MiB), not an unbounded set of sizes/frames. Budgets cover pixel
buffers, not every allocator, GUI object, JSON object or process library page.

Windows decoding uses WIC and per-call worker COM RAII; no wxPNG or new runtime
codec is required. Other wx builds use a registered PNG handler and suppress
codec dialogs, returning errors as diagnostics instead. Rendering uses straight
alpha and proportional scaling without cropping. Parent color/checkerboard is
a host surface, not a modification of the asset.
