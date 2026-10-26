# Clock synchronization in the examined PCM path

All **Verified** statements below refer to PipeWire 1.4.9, commit `fd60e04525f3a04d90bf50085222e0cc9139b4a4`. This describes ALSA PCM, not all Bluetooth/netexperiments/JACK backends. Equations are mathematical transcriptions unless explicitly labeled as an inference or proposal.

## Clock designation and timing records

**Verified:** `spa_io_clock` contains clock ID/name, monotonic nanoseconds, rational sample timebase, position/duration/delay, `rate_diff`, next wake-up estimate, target rate/duration, and accumulated xrun duration. The selected driver's record is visible through `spa_io_position.clock`. An ALSA node follows when its local ID differs from the position clock ID. Matching is enabled for a follower unless driver and local clock names match. Its resampling predicate also considers nominal rate inequality and a hardware pitch-control exception. [S02](SOURCE_REFERENCES.md#s02), [S03](SOURCE_REFERENCES.md#s03)

Clock names are configuration-sensitive: default ALSA names include playback/capture direction and card index (`api.alsa.p-N` / `api.alsa.c-N`), and `clock.name` can be configured. Thus a name is a declared relationship, not an algorithm that proves common oscillators. Incorrectly declaring independent devices to share a name can suppress compensation. Default direction-specific names do not prove that every card's capture and render are physically independent either. [S02, lines 215–217, 1031–1035, 2962–2992.]

Driver candidates are priority ordered and grouped by connectivity/policy. Hardware or timer-based drivers are possible. AudioHole's destination-paced proposal is therefore an adaptation, not a verbatim PipeWire master-selection policy. [S09–S10](SOURCE_REFERENCES.md#s09)

## Observation and error construction

**Verified:** `alsa_avail()` chooses `snd_pcm_avail_update()` only when not matching, IRQ scheduling is enabled, and resampling is unnecessary; otherwise it uses `snd_pcm_avail()`. `get_avail()` can recover errors and optionally obtains `snd_pcm_htimestamp()` available frames and timestamp. Hardware-timestamp extrapolation is **disabled by default** in `spa_alsa_init()` and can be enabled through configuration. Timestamp mode/type are set to monotonic in `set_swparams()`. [S02, lines 979–981, 2492–2495, 2754–2824.]

Let `F` be node frames/s, `Q` the graph duration in graph frames, `F_g` the graph rate, `T=ceil(Q F/F_g)` the node threshold, `H` headroom, `B` hardware buffer capacity, and `A` available frames. These are frames, not bytes and not samples summed across channels.

For timestamped availability `A_h` at `t_h`, measured at logical time `t`:

\[
\Delta = \operatorname{trunc}((t-t_h)F/10^9).
\]

If `|Δ| < 3T`, the delay observation adds `clamp(Δ,−T,T)`. Otherwise it counts bad timestamp observations; after more than the configured maximum (default 64), it disables htimestamp correction. Zero timestamps do not get extrapolated. The return availability and corrected delay are kept distinct. [S02, `get_avail()`; S02 header for default.]

`get_status()` computes:

| Stream | Observed delay D | Unclamped target D* | DLL error e |
|---|---|---|---|
| Playback | `B−min(corrected_availability,B)` | `T+H` | `D−D*` |
| Capture | corrected available frames | `max(T,read_size)+H` | `D*−D` |

Targets are then clamped to `min_delay..max_delay`. When resampling is active, `read_size` comes from `RateMatch.size`; the converter's reported delay is also read for clock/latency reporting. It is not simply added to the target in `get_status()`. For an IRQ-paced **driver** (`disable_tsched && !follower`), error instead equals `(current_time−next_time)F/10^9`. [S02, lines 2826–2877.]

**Inferred:** this combines delay feedback and timing observations. It does not calculate a long-window linear regression of two endpoint timestamps in this path. Relative hardware drift appears as persistent occupancy/timing error, which drives a rate correction. `rate_diff` is a controller result relative to monotonic timing for that clock, not a guaranteed direct meter of "source ppm minus destination ppm" across every graph.

## The actual DLL recurrence

**Verified:** `struct spa_dll` stores `bw,z1,z2,z3,w0,w1,w2` as doubles. `spa_dll_init()` zeros bandwidth and states; `spa_dll_set_bw()` must then establish coefficients. With bandwidth parameter `b`, period `T` frames, rate `F` frames/s:

\[
\omega=2\pi bT/F,\quad a=1-e^{-20\omega},\quad
u=1.5\omega/T,\quad v=\omega/1.5.
\]

For each frame-valued error `e_k`, updates occur **in this order**, using newly updated earlier states:

\[
z_{1,k}=z_{1,k-1}+a(ue_k-z_{1,k-1}),
\]
\[
z_{2,k}=z_{2,k-1}+a(z_{1,k}-z_{2,k-1}),
\]
\[
z_{3,k}=z_{3,k-1}+v z_{2,k},\qquad c_k=1-(z_{2,k}+z_{3,k}).
\]

The period/rate factor gives `b` a Hz-like bandwidth scale; this is not a claim that a single coefficient equals the complete closed-loop −3 dB bandwidth. `u` has inverse-frame units; `z1,z2,z3,c` are dimensionless. The source calls this a DLL. It is not a textbook three-term PID, and no Kalman estimator is demonstrated here. [S01](SOURCE_REFERENCES.md#s01), lines 27–57.

**Inferred behavior:** two smoothing states attenuate rapid observation noise; the accumulating state retains the steady rate mismatch. In a correct-sign stable plant, persistent delay error changes `c` until average production and consumption balance. A constant nonzero e does **not** produce a bounded fixed output in the standalone helper: `z3` keeps accumulating. Boundedness depends on the enclosing plant, error handling, and actuator.

At `T=128,F=48000,b=.128`: `ω=.00214466058485`, `a=.0419863107391`, `u=.0000251327412287`, `v=.00142977372323`. From zero states, one frame of positive error yields `c=.999999955631393` on the first update. This very small first correction must not be confused with the eventual correction for sustained drift. Coefficients and the first-step identity were checked locally; see EXPERIMENTS.md.

## Error gates, adaptive bandwidth, and actuation

**Verified, `check_position_config()` and `update_time()`:**

- `max_error=max(256,(T+H)/2)` frames.
- `max_resync=min(T+H,max_error)` frames.
- A threshold change uses `diff=last_threshold−T`, subtracts `diff` from error, and requests resynchronization.
- Errors beyond `±max_resync` request resynchronization; errors beyond `±max_error` are clipped before updating the DLL.
- The controller runs for a driver or a follower with matching enabled; a nonmatching follower uses `c=1`.
- First use after initialization sets `b=.128`, `next_time=current_time`, and `base_time=current_time`.

Define `W=F_g/Q`. Source auxiliary updates, preserving the unusual exact mean formula, are:

\[
m'={mW+(e-m)\over W+1},\qquad
s'={sW+(e-m)(e-m')\over W+1}.
\]

Every time `next_time−base_time > 3×10^9 ns`:

\[
b'=\operatorname{clamp}\left({|m'|+\sqrt{|s'|}\over1000},.001,.128\right).
\]

This is **not** the common running mean `(mW+e)/(W+1)`: the old mean is subtracted once more. `s` is the source's variance-like statistic, not demonstrated to be an unbiased variance estimator. Its absolute value is taken before square root. No rationale for the exact formula or scale 1000 was found in the inspected files. `SPA_DLL_BW_MIN=.016` exists in the helper but is not the ALSA clamp minimum here. [S01–S02.]

**Verified output:** playback publishes `RateMatch.rate=c`; capture publishes `1/c`. For software conversion, ACTIVE follows `matching`. A supported ALSA pitch element instead receives a hardware control update; it skips changes smaller than 10 units in a 1,000,000-based pitch representation. Thus not every matching path uses software ASRC. [S02, lines 2444–2478, 2931–2952.]

The next logical wake-up increments by `trunc(T×10^9/(cF))` ns. A negative threshold `diff` also adjusts it. Clock reporting advances position by the previous duration, assigns current graph duration/rate, reports delay plus converter delay, and sets `rate_diff=c`. These are scheduling/timing outputs; follower hardware frequency itself remains independent.

## Discontinuity and interruption handling

**Verified, S02:**

1. Playback driver early wake-up: if running delay exceeds target by more than `max_error`, postpone the next wake-up; delay is capped to three times target for this calculation.
2. Capture driver early wake-up: if fewer than `read_size` frames exist, postpone by the missing-frame time and return `-EAGAIN`.
3. Playback follower resync: for an unlinked stream, rewind excess queued frames or insert silence to reach target, then reset DLL state. This is an explicit discontinuity, not transparent continuous correction.
4. Capture follower resync: forward excess captured frames; if not enough are available, restrict the read; reset the DLL. Ordinary shortage can result in zero `max_read`.
5. Xrun recovery: estimate missing duration/frames, update xrun diagnostics, drop/prepare/start the affected driver and linked followers. `do_prepare()` primes playback with silence and resets buffers/DLL.
6. Suspend recovery attempts `snd_pcm_resume()` up to five times, with one-second `poll()` waits on `-EAGAIN`, before dropping/preparing. This blocking behavior can be reached from availability recovery and is unsuitable for an AudioHole real-time callback.
7. `handle_timeout()` detects a computed next time more than one second from the current logical time and falls back to a nominal interval. Device removal or failure may still propagate errors; this is not proof of arbitrary hotplug recovery.

The timer callback uses the previous logical `next_time` as the observation time, not simply its actual wall-clock entry time. `get_avail()` may extrapolate hardware availability to that logical time. Preserve the distinction when porting the concept to event timestamps. [S02, lines 2764–2824, 3630–3658.]

## Implications for destination pacing

**Experimental:** define queue occupancy in source frames, sample it at a consistent render phase, and use `r = source_frames/destination_frame`. If the source is 48,004.8 actual frames/s and destination is 48,000, equilibrium is `r=1.0001`. With no correction, occupancy increases 4.8 frames/s; 128 frames of spare capacity last about 26.67 s. This is arithmetic under ideal steady rates, not hardware measurement.

A direct use of the DLL's `c` requires a documented reciprocal/sign mapping: the recommended AudioHole actuator increases input consumption when the source queue is high. Do not transplant playback `delay-target` into an input-consumption actuator without checking the sign. Keep long-term rate tracking separate from reset/re-prime policy; invalidate timing history after stream recreation and use a new stream generation. A low-bandwidth loop cannot safely absorb unlimited dropouts by itself.
