# Comparing baseline and low latency

In 0.4 both presets include the startup-only refill correction and new underrun
context. Follow [the current startup-buffer test](STARTUP_BUFFER_TEST.md) first.
The following describes the 0.3 Low scheduling policy retained in 0.4; use the
preserved 0.3 package for an exact comparison with the original baseline.

Use the same individual profile JSON in the new package's `Profile` folder.
No profile editing or GUI change is needed. Run only one validation process at
a time. Both launchers use the same executable, scanned bindings and profiles.

1. Run `Run-Validation.cmd` for the baseline. `list`, `select NUMBER`, `start`.
2. Check `status`. Take several LagMeter readings, noting a typical value and
   the observed range. Leave routing up long enough to inspect both directions'
   counters; 10–15 minutes is useful given the previous intermittent return issue.
3. `stop`, confirm `Stopped`, then `quit`.
4. Run `Run-Low-Latency.cmd` and repeat with the same profile and phone setup.
5. If practical, repeat the baseline to check for startup/warm-up differences.
   Keep mixer routing, headset link, phone positions, volume and gain unchanged.

The direct equivalent of the low launcher is:

```
audiohole-engine-validation.exe --low-latency
```

Starting the executable without that flag retains baseline tuning. Neither
launcher starts audio automatically. The low flag applies only to native
WASAPI and the native-I/O Experimental adapter; miniaudio is unchanged. Automatic
still chooses native WASAPI first and never selects Experimental.

## What changes in Low

- Shared automatic streams request the smallest legal Client3 engine period.
  If rejected, the client is recreated and the default shared period is tried,
  followed by the existing legacy shared compatibility path if necessary.
  The original PCM requirements remain unchanged; Exclusive is never enabled
  implicitly. The fallback reason and actual period are logged separately.
- Automatic shared playback aims to keep one accepted engine period plus a
  1-ms margin submitted, instead of filling its entire allocation. Unknown
  periods retain full-buffer behavior. Explicit `buffer_ms` and Exclusive
  streams retain baseline submission behavior.
- The initial native software queue floor falls from 20 ms to 5 ms, while
  retaining two reported periods and the observed packet-plus-render safety
  guard. If the devices still deliver large packets, the guard still needs
  that reserve. No buffer is resized or unread audio discarded to lower latency.
- Controller gains, correction bounds, sample conversion and mapping are
  unchanged. The existing controller still derives its proportional gain from
  the active target. These are opt-in comparison settings, not production policy.

At the previous 48-kHz/480-frame period and 1,056-frame render allocation, Low's
render lead target is 528 frames / 11 ms, versus the baseline 22-ms allocation.
That is a submission-policy difference, not proof of an 11-ms acoustic saving.
Smaller accepted periods can reduce both render lead and software queue reserve.
Hardware packet sizes can prevent the expected queue reduction.

## What to send back

Keep the `.txt` or `.jsonl` from each run in `Results`, identify which LagMeter
readings belong to each, and note any clicks/dropouts. Start records identify
`latency_preset`; source/destination fields report advertised period bounds,
`period_selection`, fallback details, actual allocation and render lead target.

Compare actual periods, packet/block sizes, controller target, sampled FIFO
occupancy, full-rate frame progress and underrun/overrun counters. Capture
diagnostics add longest nonempty-drain arrival gap, largest total frames drained
in one wake, and last/max valid first-frame packet age from native QPC. Packet
age is capture timestamp to receipt, not microphone-to-headphone delay.

Shared rendering reports padding before the write and estimated padding after
submission (before plus submitted frames; playback can advance during the call).
`render_empty_wakes` counts observations of zero padding; it is not a measured
hardware underrun counter. Native timing aggregates are independently sampled
and do not reconstruct a full event trace or measure physical clock drift.

If Low clicks, starves, or makes progress slower, stop and return to the baseline.
Do not compensate by quietly ignoring explicit profile parameters. A rejected
minimum can reflect another shared stream holding the engine period; the log
should show fallback rather than claiming that the minimum was applied.

The approximately 116-ms LagMeter observation is for the complete acoustic
monitoring loop, potentially including phone input/output delay. The engine
cannot remove wireless transport, mixer DSP or phone delay. A lower end-to-end
result must be established on the real setup; this package does not predeclare
one. Keep the phone's direct-sound leakage check and, where practical, confirm
that stopping AudioHole removes the delayed return while the phone emits.
